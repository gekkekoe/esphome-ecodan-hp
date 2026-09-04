#include "optimizer.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        // ─────────────────────────────────────────────────────────────────
        // Ecodan RTC helpers
        // ─────────────────────────────────────────────────────────────────

        bool Optimizer::aa_enabled() const {
            return this->state_.auto_adaptive_control_enabled != nullptr && this->state_.auto_adaptive_control_enabled->state;
        }

        bool Optimizer::solver_enabled() const {
            return this->state_.sw_use_solver != nullptr && this->state_.sw_use_solver->state;
        }

        int Optimizer::get_current_ecodan_hour() {
            if (this->state_.ecodan_instance == nullptr) return -1;
            time_t ts = this->state_.ecodan_instance->get_status().timestamp();
            if (ts == -1) return -1;
            // Ecodan reports local time; timestamp() uses mktime() which interprets
            // ControllerDateTime as local time → epoch. Use localtime_r to get back
            // the correct local hour. gmtime_r would give UTC = wrong hour.
            struct tm t;
            localtime_r(&ts, &t);
            return t.tm_hour;
        }

        int Optimizer::get_current_ecodan_day() {
            if (this->state_.ecodan_instance == nullptr) return -1;
            time_t ts = this->state_.ecodan_instance->get_status().timestamp();
            if (ts == -1) return -1;
            struct tm t;
            localtime_r(&ts, &t);
            return t.tm_yday;
        }

        bool Optimizer::has_old_odin_data() { 
          if (this->odin_mutex_ == NULL) return false;
          bool has_data = false;
          
          if (xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
              has_data = (odin_data_ready_ && odin_production_.size() == 24);
              xSemaphoreGive(this->odin_mutex_);
          }
          return has_data;
        }

        float Optimizer::get_current_solar_irradiance() {
            float current_solar = 0.0f;
            int hr = this->get_current_ecodan_hour();
            
            if (hr >= 0 && hr < 48) {
                if (this->odin_mutex_ != NULL && xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
                    if (!this->odin_solar_forecast_.empty() && hr < (int)this->odin_solar_forecast_.size()) {
                        current_solar = this->odin_solar_forecast_[hr];
                    }
                    xSemaphoreGive(this->odin_mutex_);
                }
            }
            return current_solar;
        }

        // get numeric operating mode for mpc call
        uint8_t Optimizer::get_current_operation_mode() {

            if (this->state_.ecodan_instance == nullptr) return 0;
            auto &status = this->state_.ecodan_instance->get_status();
            
            return static_cast<uint8_t>(status.Operation);
        }

        // ─────────────────────────────────────────────────────────────────
        // ODIN production store (called from YAML after fetch completes)
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::store_odin_data(int current_hour,
                                        float min_output, 
                                        float max_output, 
                                        const std::vector<float>& prod,
                                        const std::vector<float>& prod_z1,
                                        const std::vector<float>& prod_z2,
                                        const std::vector<float>& solar,
                                        const std::vector<float>& op_mode) {
            if (current_hour == -1) return;

            if (this->odin_mutex_ == NULL ||
                xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(OPTIMIZER_TAG, "Failed to acquire ODIN mutex during store.");
                return;
            }

            this->odin_min_output_ = min_output;
            this->odin_max_output_ = max_output;

            // odin_production_ and odin_operation_mode_ are DP_HOURS=24 (engine only plans today).
            // odin_solar_forecast_ is kept at 48 — the server passes today+tomorrow solar.
            bool is_first_run = (!this->odin_data_ready_ || this->odin_production_.size() != 24);
            if (is_first_run) {
                this->odin_solar_forecast_.assign(48, 0.0f);
                this->odin_operation_mode_.assign(24, NAN);
                this->odin_production_.assign(24, NAN);
                this->odin_data_ready_ = true;
            }

            const bool two_zone = (!prod_z1.empty() && !prod_z2.empty());
            if (two_zone) {
                if (this->odin_production_z1_.size() != 24) this->odin_production_z1_.assign(24, NAN);
                if (this->odin_production_z2_.size() != 24) this->odin_production_z2_.assign(24, NAN);
            } else {
                this->odin_production_z1_.clear();
                this->odin_production_z2_.clear();
            }

            int first_update = is_first_run ? current_hour : current_hour + 1;

            // Midnight-wrap: solve ran at hour 23, first_update==24 means the new data
            // is for tomorrow (slots 24-47 in the engine's 48h output arrays).
            // We also advance odin_data_day_ to tomorrow so the day-staleness check in
            // resolve_solver_result_() does not immediately invalidate the data at 00:00.
            bool midnight_wrap = (first_update == 24);

            int _day = this->get_current_ecodan_day();
            if (_day >= 0) {
                // If wrapping into the next day, register tomorrow's yday so that the
                // data stays valid when the clock ticks past midnight.
                this->odin_data_day_ = midnight_wrap ? ((_day + 1) % 365) : _day;
            }

            // odin_production_/odin_operation_mode_ are a 24h "today" window, always
            // indexed 0-23 regardless of calendar day.
            // On midnight-wrap the engine's prod/op_mode slots for tomorrow start at
            // index 24, so apply a source offset of 24 when reading from those arrays.
            int prod_first_update = midnight_wrap ? 0 : first_update;
            int prod_src_offset   = midnight_wrap ? 24 : 0;
            if (prod_first_update >= 0 && prod_first_update < 24) {
                for (int i = prod_first_update; i < 24; i++) {
                    int src = i + prod_src_offset;
                    if (src < (int)prod.size())    this->odin_production_[i]     = prod[src];
                    if (src < (int)op_mode.size()) this->odin_operation_mode_[i] = op_mode[src];
                }
            }

            if (two_zone) {
                int zone_first = (prod_first_update < current_hour) ? prod_first_update : current_hour;
                for (int i = zone_first; i < 24; i++) {
                    // Same midnight-wrap source offset as the combined vector: the
                    // zone arrays are the same 48h rolling window, so on wrap the
                    // new day's plan sits at 24-47.
                    int src = i + prod_src_offset;
                    if (src < (int)prod_z1.size()) this->odin_production_z1_[i] = prod_z1[src];
                    if (src < (int)prod_z2.size()) this->odin_production_z2_[i] = prod_z2[src];
                }
            }

            if (first_update >= 0 && first_update < 48) {
                for (int i = first_update; i < 48; i++) {
                    if (i < (int)solar.size())   this->odin_solar_forecast_[i] = solar[i];
                }
            }

            xSemaphoreGive(this->odin_mutex_);
            ESP_LOGI(OPTIMIZER_TAG, "ODIN production targets loaded (48h). current_hour=%d midnight_wrap=%d data_day=%d two_zone=%d",
                     current_hour, (int)midnight_wrap, this->odin_data_day_,
                     (int)prod_z1.size() > 0 && (int)prod_z2.size() > 0);
        }

        // ─────────────────────────────────────────────────────────────────
        // Solver soft-stop: cut/restore relay when ODIN says 0 kWh
        // ─────────────────────────────────────────────────────────────────
        void Optimizer::apply_solver_soft_stop(bool should_stop, OptimizerZone zone) {
            if (this->state_.ecodan_instance == nullptr) return;
            auto &status = this->state_.ecodan_instance->get_status();

            bool is_locked = false;
            if (this->state_.status_short_cycle_lockout != nullptr && 
                this->state_.status_short_cycle_lockout->has_state()) {
                is_locked = this->state_.status_short_cycle_lockout->state;
            }

            // Never touch relays during special operating modes
            if (status.DefrostActive || is_locked || this->is_dhw_active(status)) {
                return;
            }

            int current_hour = this->get_current_ecodan_hour();
            if (current_hour < 0) return;  // Ecodan time not yet valid
            auto *relay = (zone == OptimizerZone::ZONE_2) ? this->state_.relay_switch_z2 : this->state_.relay_switch_z1;
            if (relay == nullptr) return;

            const char *zl = (zone == OptimizerZone::ZONE_2) ? "Z2" : "Z1";
            const int zi = (zone == OptimizerZone::ZONE_2) ? 1 : 0;

            if (should_stop) {
                // One write per hour guard
                if (this->solver_stop_active_[zi] && this->solver_stop_hour_[zi] == current_hour)
                    return;

                ESP_LOGI(OPTIMIZER_TAG, "Solver soft-stop %s: disable demand for hour %d", zl, current_hour);

                if (relay->state) relay->turn_off();

                this->solver_stop_active_[zi] = true;
                this->solver_stop_hour_[zi]   = current_hour;

            } else {
                // One write per hour guard to avoid chattering
                if (this->solver_resume_hour_[zi] == current_hour)
                    return;
                this->solver_resume_hour_[zi] = current_hour;

                ESP_LOGI(OPTIMIZER_TAG, "Solver soft-start %s: enabled demand for hour %d", zl, current_hour);

                relay->turn_on();
                this->solver_stop_active_[zi] = false;
                this->solver_stop_hour_[zi]   = -1;
            }
        }
    } // namespace optimizer
} // namespace esphome
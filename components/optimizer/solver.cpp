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

        int Optimizer::get_current_ecodan_hour() {
            if (this->state_.ecodan_instance == nullptr) return 0;
            time_t ts = this->state_.ecodan_instance->get_status().timestamp();
            if (ts == -1) return 0;
            struct tm t;
            gmtime_r(&ts, &t);
            return t.tm_hour;
        }

        int Optimizer::get_current_ecodan_day() {
            if (this->state_.ecodan_instance == nullptr) return 0;
            time_t ts = this->state_.ecodan_instance->get_status().timestamp();
            if (ts == -1) return 0;
            struct tm t;
            gmtime_r(&ts, &t);
            return t.tm_yday;
        }

        bool Optimizer::has_old_odin_data() { 
          if (this->odin_mutex_ == NULL) return false;
          bool has_data = false;
          
          if (xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
              has_data = (odin_data_ready_ && odin_energy_.size() == 24);
              xSemaphoreGive(this->odin_mutex_);
          }
          return has_data;
        }

        // ─────────────────────────────────────────────────────────────────
        // ODIN production store (called from YAML after fetch completes)
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::store_odin_data(int current_hour,
                                        const std::vector<float>& prod,
                                        const std::vector<float>& energy) {
            if (current_hour == -1) return;

            if (this->odin_mutex_ == NULL ||
                xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(OPTIMIZER_TAG, "Failed to acquire ODIN mutex during store.");
                return;
            }

            // First run or size mismatch — full replace
            if (!this->odin_data_ready_ || this->odin_production_.size() != 24) {
                this->odin_production_ = prod;
                this->odin_energy_     = energy;
                this->odin_production_.resize(24);
                this->odin_energy_.resize(24);
                this->odin_data_day_   = this->get_current_ecodan_day();
                this->odin_data_ready_ = true;
            }

            // Partial update: overwrite current hour onward
            if (current_hour >= 0 && current_hour < 24) {
                for (int i = current_hour; i < 24; i++) {
                    if (i < prod.size() && i < energy.size()) {
                        this->odin_production_[i] = prod[i];
                        this->odin_energy_[i]     = energy[i];
                    } else {
                        ESP_LOGW(OPTIMIZER_TAG, "ODIN payload shorter than expected, stopping partial update at hour %d", i);
                        break; 
                    }
                }
            }

            xSemaphoreGive(this->odin_mutex_);
            ESP_LOGI(OPTIMIZER_TAG, "ODIN production targets loaded. current_hour=%d", current_hour);
        }

        // ─────────────────────────────────────────────────────────────────
        // Solver soft-stop: cut/restore relay when ODIN says 0 kWh
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::apply_solver_soft_stop(bool should_stop) {
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
            auto *relay_z1   = this->state_.demand_switch_z1;
            auto *relay_z2   = this->state_.demand_switch_z2;

            if (should_stop) {
                // One write per hour guard
                if (this->solver_stop_active_ && this->solver_stop_hour_ == current_hour)
                    return;

                ESP_LOGI(OPTIMIZER_TAG, "Solver soft-stop: 0 kWh hour %d — cutting relay", current_hour);

                if (this->state_.asgard_vt_z1 != nullptr) {
                    if (relay_z1 != nullptr && relay_z1->state) relay_z1->turn_off();
                    auto call = this->state_.asgard_vt_z1->make_call();
                    call.set_mode(climate::CLIMATE_MODE_OFF);
                    call.perform();
                }
                if (this->state_.asgard_vt_z2 != nullptr) {
                    if (relay_z2 != nullptr && relay_z2->state) relay_z2->turn_off();
                    auto call = this->state_.asgard_vt_z2->make_call();
                    call.set_mode(climate::CLIMATE_MODE_OFF);
                    call.perform();
                }

                this->solver_stop_active_ = true;
                this->solver_stop_hour_   = current_hour;

            } else {
                // One write per hour guard to avoid chattering.
                if (this->solver_resume_hour_ == current_hour)
                    return;
                this->solver_resume_hour_ = current_hour;

                ESP_LOGI(OPTIMIZER_TAG, "Solver: ensuring relay ON for heating hour %d (was_stopped=%d)",
                         current_hour, this->solver_stop_active_);

                if (this->state_.asgard_vt_z1 != nullptr) {
                    if (relay_z1 != nullptr && !relay_z1->state) relay_z1->turn_on();
                    auto call = this->state_.asgard_vt_z1->make_call();
                    call.set_mode(climate::CLIMATE_MODE_HEAT);
                    call.perform();
                }
                if (this->state_.asgard_vt_z2 != nullptr) {
                    if (relay_z2 != nullptr && !relay_z2->state) relay_z2->turn_on();
                    auto call = this->state_.asgard_vt_z2->make_call();
                    call.set_mode(climate::CLIMATE_MODE_HEAT);
                    call.perform();
                }

                this->solver_stop_active_ = false;
                this->solver_stop_hour_   = -1;
            }
        }
    } // namespace optimizer
} // namespace esphome

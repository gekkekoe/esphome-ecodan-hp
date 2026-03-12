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

        // ─────────────────────────────────────────────────────────────────
        // ODIN schedule store (called from YAML after fetch completes)
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::store_odin_data(int current_hour,
                                        const std::vector<float>& sched,
                                        const std::vector<float>& energy) {
            if (current_hour == -1) return;

            if (this->odin_mutex_ == NULL ||
                xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(OPTIMIZER_TAG, "Failed to acquire ODIN mutex during store.");
                return;
            }

            // First run or size mismatch — full replace
            if (!this->odin_data_ready_ || this->odin_schedule_.size() != 24) {
                this->odin_schedule_ = sched;
                this->odin_energy_   = energy;
                this->odin_schedule_.resize(24);
                this->odin_energy_.resize(24);
                this->odin_data_day_ = this->get_current_ecodan_day();
                this->odin_data_ready_ = true;
            }

            // Partial update: overwrite current hour onward
            if (current_hour >= 0 && current_hour < 24) {
                for (int i = current_hour; i < 24; i++) {
                    this->odin_schedule_[i] = sched[i];
                    this->odin_energy_[i]   = energy[i];
                }
            }

            xSemaphoreGive(this->odin_mutex_);
            ESP_LOGI(OPTIMIZER_TAG, "ODIN targets loaded. current_hour=%d", current_hour);
        }

        // ─────────────────────────────────────────────────────────────────
        // Solver soft-stop: cut/restore relay when ODIN says 0 kWh
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::apply_solver_soft_stop(bool should_stop) {
            auto &status = this->state_.ecodan_instance->get_status();

            // Never touch relays during special operating modes
            if (status.DefrostActive ||
                this->state_.status_short_cycle_lockout->state ||
                this->is_dhw_active(status)) {
                return;
            }

            int current_hour = this->get_current_ecodan_hour();
            auto *relay_z1   = this->state_.demand_switch_z1;
            auto *relay_z2   = this->state_.demand_switch_z2;

            if (should_stop) {
                // One write per hour guard
                if (this->solver_stop_active_ && this->solver_stop_hour_ == current_hour)
                    return;

                if (relay_z1 == nullptr) {
                    ESP_LOGW(OPTIMIZER_TAG, "Solver soft-stop: demand_switch_z1 not wired");
                    return;
                }

                ESP_LOGI(OPTIMIZER_TAG, "Solver soft-stop: 0 kWh hour %d — cutting relay", current_hour);

                this->solver_stop_active_ = true;
                this->solver_stop_hour_   = current_hour;

                // Set thermostat to OFF first so it won't re-trigger the relay.
                if (this->state_.asgard_vt_z1 != nullptr) {
                    auto call = this->state_.asgard_vt_z1->make_call();
                    call.set_mode("off");
                    call.perform();
                }
                if (relay_z2 != nullptr && this->state_.asgard_vt_z2 != nullptr) {
                    auto call = this->state_.asgard_vt_z2->make_call();
                    call.set_mode("off");
                    call.perform();
                }

                relay_z1->turn_off();
                if (relay_z2 != nullptr)
                    relay_z2->turn_off();

            } else {
                if (!this->solver_stop_active_)
                    return;

                ESP_LOGI(OPTIMIZER_TAG, "Solver soft-stop: lifting stop (was hour %d, now %d)",
                         this->solver_stop_hour_, current_hour);

                // Clear state BEFORE performing thermostat calls.
                // This prevents a re-entrant run_auto_adaptive_loop (triggered by
                // ThermostatClimate callbacks) from calling apply_solver_soft_stop(false)
                this->solver_stop_active_ = false;
                this->solver_stop_hour_   = -1;

                // Restore thermostats to HEAT — relay hysteresis handles turn-on
                if (this->state_.asgard_vt_z1 != nullptr) {
                    auto call = this->state_.asgard_vt_z1->make_call();
                    call.set_mode("heat");
                    call.perform();
                }
                if (relay_z2 != nullptr && this->state_.asgard_vt_z2 != nullptr) {
                    auto call = this->state_.asgard_vt_z2->make_call();
                    call.set_mode("heat");
                    call.perform();
                }
            }
        }

    } // namespace optimizer
} // namespace esphome

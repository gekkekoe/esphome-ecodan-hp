#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"
#include "esphome/core/log.h"
#include <cmath>

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        // Raw Data Collection Model
        void Optimizer::update_heat_model()
        {
            if (this->state_.ecodan_instance == nullptr) return;
            auto &status = this->state_.ecodan_instance->get_status();
            uint32_t now = millis();
            bool is_running = (status.CompressorFrequency > 0) || status.CompressorOn;
            bool is_heating_active = status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;

            if (this->last_check_ms_ != 0) {    
                float minutes_passed = (now - this->last_check_ms_) / 60000.0f;

                // If gap > 10 minutes (reboot/reconnect), reset free cooling window
                // to avoid accumulating stale time into the measurement.
                if (minutes_passed > 10.0f) {
                    if (this->fc_active_) {
                        ESP_LOGD(OPTIMIZER_TAG, "Free heating/cooling window reset after %.0f min gap (reconnect/reboot)", minutes_passed);
                        this->fc_active_ = false;
                    }
                    this->last_check_ms_ = now;
                    return;
                }

                if (is_running && is_heating_active) {
                    this->daily_runtime_global += minutes_passed;
                }

                // --- Free cooling window (HP-off period, any time of day) ---
                float current_room_tmp = this->get_room_current_temp(OptimizerZone::ZONE_1);
                bool hp_off = !is_running || !is_heating_active;

                if (!isnan(current_room_tmp) && !isnan(status.OutsideTemperature)) {
                    if (hp_off && !this->fc_active_) {
                        this->fc_active_        = true;
                        this->fc_room_start_    = current_room_tmp;
                        this->fc_outside_sum_   = status.OutsideTemperature;
                        this->fc_solar_sum_     = this->get_current_solar_irradiance(); 
                        this->fc_outside_count_ = 1;
                        this->fc_hours_         = 0.0f;
                        ESP_LOGD(OPTIMIZER_TAG, "Passive thermal window started: room=%.2fC", current_room_tmp);

                    } else if (hp_off && this->fc_active_) {
                        this->fc_outside_sum_  += status.OutsideTemperature;
                        this->fc_solar_sum_    += this->get_current_solar_irradiance(); 
                        this->fc_outside_count_++;
                        this->fc_hours_ += minutes_passed / 60.0f;

                    } else if (!hp_off && this->fc_active_) {
                        // HP just came back on — seal the measurement window
                        float delta_cool  = this->fc_room_start_ - current_room_tmp;
                        float t_hours     = this->fc_hours_;
                        float t_outside   = this->fc_outside_sum_ / this->fc_outside_count_;
                        float avg_sol     = this->fc_solar_sum_ / this->fc_outside_count_; 
                        float t_room_avg  = (this->fc_room_start_ + current_room_tmp) / 2.0f;
                        float delta_T_avg = t_room_avg - t_outside;
                        this->fc_active_ = false;

                        // Base quality gates (need enough time and a decent inside/outside delta)
                        if (t_hours >= 2.0f && delta_T_avg > 2.0f) {
                            
                            if (avg_sol < 50.0f) {
                                // NO SIGNIFICANT SOLAR: Learn pure thermal time constant (Tau).
                                // Happens at night or during heavily overcast days.
                                // Strictly require the room to have cooled down to calculate physics.
                                if (t_hours >= 3.0f && delta_cool > 0.15f && delta_cool < 5.0f) {
                                    float tau = (delta_T_avg * t_hours) / delta_cool;
                                    
                                    if (tau > 5.0f && tau < 300.0f) {
                                        if (this->state_.num_raw_hl_tm_product != nullptr) {
                                            float cur = this->state_.num_raw_hl_tm_product->state;
                                            float next = (cur <= 0.001f || std::isnan(cur)) ? tau
                                                         : (0.20f * tau + 0.80f * cur);
                                            this->state_.num_raw_hl_tm_product->publish_state(next);
                                        }
                                        ESP_LOGI(OPTIMIZER_TAG,
                                            "Free heating/cooling (No Solar): %.2f->%.2fC (%.2fK) in %.1fh, "
                                            "outside=%.1fC -> Tau=%.1fh",
                                            this->fc_room_start_, current_room_tmp,
                                            delta_cool, t_hours, t_outside, tau);
                                    } else {
                                        ESP_LOGW(OPTIMIZER_TAG, "Free heating/cooling Tau=%.1fh out of range, discarded", tau);
                                    }
                                }
                            } else {
                                // SOLAR ACTIVE: Learn passive solar gain.
                                // solar_factor is dimensionless: K of extra room warmth per kW of solar irradiance.
                                // Formula: free_heating_K / avg_solar_kW
                                //   free_heating_K = expected_drop (from Tau) - actual_drop
                                //   This avoids needing thermal_mass explicitly (which is unknown here).
                                // Units: solar_factor [K / kW] — used in optimizer as:
                                //   free_solar_heat_kw = (irradiance_W/m2 / 1000) * solar_factor
                                // which gives kW of equivalent heat input to the room.
                                if (this->state_.num_raw_solar_factor != nullptr && this->state_.num_raw_hl_tm_product != nullptr) {
                                    float tau = this->state_.num_raw_hl_tm_product->state;
                                    
                                    if (tau > 5.0f) {  // only when Tau is reliably learned
                                        float expected_drop = (delta_T_avg * t_hours) / tau;
                                        // positive = house cooled less than expected (solar kept it warm)
                                        // negative = house cooled more than expected (no solar effect)
                                        float free_heating_kelvin = expected_drop - delta_cool;
                                        float avg_sol_kw = avg_sol / 1000.0f;
                                        
                                        if (free_heating_kelvin > 0.0f && avg_sol_kw > 0.05f) {
                                            // K of passive gain per kW solar irradiance per hour
                                            float learned_solar_factor = (free_heating_kelvin / t_hours) / avg_sol_kw;
                                            
                                            // Physical range: 0.01–0.5 K/kW
                                            // Upper bound: ~0.5 means a very glassy, light-mass house.
                                            // Values above 0.5 are likely measurement noise or DHW contamination.
                                            learned_solar_factor = std::max(0.01f, std::min(0.5f, learned_solar_factor));
                                            
                                            float cur = this->state_.num_raw_solar_factor->state;
                                            float next = (cur <= 0.001f || std::isnan(cur)) ? learned_solar_factor
                                                         : (0.20f * learned_solar_factor + 0.80f * cur);
                                            this->state_.num_raw_solar_factor->publish_state(next);
                                            
                                            ESP_LOGI(OPTIMIZER_TAG, "Passive solar gain: factor=%.3f K/kW (avg_sol=%.0fW, expected_drop=%.2fK, actual_drop=%.2fK, t=%.1fh)",
                                                     next, avg_sol, expected_drop, delta_cool, t_hours);
                                        } else {
                                            ESP_LOGD(OPTIMIZER_TAG, "Free passive solar gain: No measurable solar gain (expected drop %.2fK, actual drop %.2fK)", 
                                                     expected_drop, delta_cool);
                                        }
                                    }
                                }
                            }
                        } else {
                            ESP_LOGD(OPTIMIZER_TAG,
                                "Free heating/cooling window invalid: delta_T=%.1fK, hours=%.1f",
                                delta_T_avg, t_hours);
                        }
                    }
                }
            }
            this->last_check_ms_ = now;
            // Track outside temperature periodically
            if (!isnan(status.OutsideTemperature)) {
                this->daily_outside_temp_sum_ += status.OutsideTemperature;
                this->daily_outside_temp_count_++;
            }

            // Track room temperature periodically
            float z1_temp = this->get_room_current_temp(OptimizerZone::ZONE_1);
            float current_room_temp = z1_temp;

            // If the system has 2 zones, calculate the average of both rooms
            if (status.has_2zones()) {
                float z2_temp = this->get_room_current_temp(OptimizerZone::ZONE_2);
                
                // Safely average them if both are valid floats
                if (!isnan(z1_temp) && !isnan(z2_temp)) {
                    current_room_temp = (z1_temp + z2_temp) / 2.0f;
                } else if (isnan(z1_temp) && !isnan(z2_temp)) {
                    // Fallback to Zone 2 if Zone 1 is missing for some reason
                    current_room_temp = z2_temp;
                }
            }

            if (!isnan(current_room_temp)) {
                this->daily_room_temp_sum_ += current_room_temp;
                this->daily_room_temp_count_++;
                
                // Track daily min/max to determine the temperature delta
                if (current_room_temp < this->daily_room_temp_min_) this->daily_room_temp_min_ = current_room_temp;
                if (current_room_temp > this->daily_room_temp_max_) this->daily_room_temp_max_ = current_room_temp;
            }

            // Track max output power periodically
            if (this->state_.computed_output_power != nullptr) {
                float out_pwr = this->state_.computed_output_power->state;
                if (!isnan(out_pwr) && out_pwr > this->daily_max_output_power_) {
                    this->daily_max_output_power_ = out_pwr;
                }
            }

            int current_day  = this->get_current_ecodan_day();
            int current_hour = this->get_current_ecodan_hour();

            // Ecodan time not yet valid — skip to prevent false day/hour transitions
            if (current_day < 0 || current_hour < 0) return;

            // Initialize on boot to prevent jump
            if (this->last_processed_day_ == -1) {
                this->last_processed_day_ = current_day;
                this->last_processed_hour_ = current_hour;
                return;
            }

            // Check if day changed (e.g. 303 -> 304)
            if (current_day != this->last_processed_day_) {
                ESP_LOGI(OPTIMIZER_TAG, "Raw Data Collection: Day transition detected (%d -> %d). Saving stats...", 
                         this->last_processed_day_, current_day);
                
                this->update_learning_model(this->last_processed_day_);

                // trigger new data fetch
                this->odin_fetch_requested_ = true;

                // Reset variables for the new day
                this->last_processed_day_ = current_day;
                
                this->daily_outside_temp_sum_ = 0.0f;
                this->daily_outside_temp_count_ = 0;
                
                this->daily_room_temp_sum_ = 0.0f;
                this->daily_room_temp_count_ = 0;
                this->daily_room_temp_min_ = 99.0f;
                this->daily_room_temp_max_ = -99.0f;
                this->daily_max_output_power_ = 0.0f;
                
                this->daily_runtime_global = 0.0f;
            }

            // HOURLY MPC TRIGGER
            if (this->solver_enabled() 
                && current_hour != this->last_processed_hour_) {
                ESP_LOGI(OPTIMIZER_TAG, "Hour transition detected (%d -> %d). Requesting ODIN hourly course-correction...", 
                         this->last_processed_hour_, current_hour);
                         
                this->odin_fetch_requested_ = true; // Signal YAML to fetch
                this->last_processed_hour_ = current_hour;
            }

            // Set latest energy snapshots
            if (is_running && is_heating_active) {
                this->last_total_heating_produced_ = this->state_.daily_heating_produced->state;

                if (this->state_.solver_kwh_meter_feedback_source == nullptr || 
                    this->state_.solver_kwh_meter_feedback_source->active_index().value_or(0) == 0) {
                    this->last_total_heating_consumed_ = this->state_.daily_heating_consumed->state;
                }
                else {
                    // Ensure the feedback number is also not null before reading
                    this->last_total_heating_consumed_ = (this->state_.solver_kwh_meter_feedback != nullptr) ? 
                                                        this->state_.solver_kwh_meter_feedback->state : 0.0f;
                }
            }
        }

        void Optimizer::update_learning_model(int day_of_year)
        {
            // Calculate average outside temperature
            float avg_outside = 7.0f; // Safe fallback
            if (this->daily_outside_temp_count_ > 0) {
                avg_outside = this->daily_outside_temp_sum_ / this->daily_outside_temp_count_;
            }

            // Calculate average room temperature and the daily delta
            float avg_room = 20.0f; // Safe fallback
            float delta_room = 0.0f;
            if (this->daily_room_temp_count_ > 0) {
                avg_room = this->daily_room_temp_sum_ / this->daily_room_temp_count_;
                if (this->daily_room_temp_max_ >= this->daily_room_temp_min_) {
                    delta_room = this->daily_room_temp_max_ - this->daily_room_temp_min_;
                }
            }

            // Retrieve runtime (convert minutes to hours)
            float runtime_hours = this->daily_runtime_global / 60.0f;
            float heat_produced_kwh = this->last_total_heating_produced_;
            float elec_consumed_kwh = this->last_total_heating_consumed_;
            float max_out_kw = this->daily_max_output_power_;

            ESP_LOGI(OPTIMIZER_TAG, "Daily Raw Stats: Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, MaxOut=%.1fkW, AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC",
                     heat_produced_kwh, elec_consumed_kwh, runtime_hours, max_out_kw, avg_outside, avg_room, delta_room);
            
            // Only update the globals if significant heating occurred (> 2 kWh produced, > 1 hour run)
            if (heat_produced_kwh < 2.0f || runtime_hours < 1.0f) {
                ESP_LOGD(OPTIMIZER_TAG, "Stats Collection: Skipped. Insufficient heating data today.");
                return;
            }

            // Helper for Exponential Moving Average (EMA) on Number components
            auto update_ema_num = [](esphome::number::Number *comp, float observed, float alpha) {
                if (comp == nullptr) return;
                float current = comp->state;
                if (current <= 0.01f || std::isnan(current)) current = observed; // Initialize if empty
                else current = (alpha * observed) + ((1.0f - alpha) * current);
                
                // Publish back to ESPHome so the dashboard and NVS flash are updated
                comp->publish_state(current); 
            };

            const float ALPHA = 0.15f; 

            update_ema_num(this->state_.num_raw_heat_produced, heat_produced_kwh, ALPHA);
            update_ema_num(this->state_.num_raw_elec_consumed, elec_consumed_kwh, ALPHA);
            update_ema_num(this->state_.num_raw_runtime_hours, runtime_hours, ALPHA);
            // avg_outside_temp is already a daily mean — publish raw so the solver
            if (this->state_.num_raw_avg_outside_temp != nullptr)
                this->state_.num_raw_avg_outside_temp->publish_state(avg_outside);
            update_ema_num(this->state_.num_raw_avg_room_temp, avg_room, ALPHA);
            update_ema_num(this->state_.num_raw_delta_room_temp, delta_room, ALPHA);
            update_ema_num(this->state_.num_raw_max_output, max_out_kw, ALPHA);

            ESP_LOGI(OPTIMIZER_TAG, "Numbers updated (15%% EMA): Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC",
                     this->state_.num_raw_heat_produced->state, this->state_.num_raw_elec_consumed->state, 
                     this->state_.num_raw_runtime_hours->state, this->state_.num_raw_avg_outside_temp->state,
                     this->state_.num_raw_avg_room_temp->state, this->state_.num_raw_delta_room_temp->state);
        }

    } // namespace optimizer
} // namespace esphome
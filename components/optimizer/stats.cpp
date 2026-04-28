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
            bool is_cooling_active = status.Operation == esphome::ecodan::Status::OperationMode::COOL_ON; // ADD THIS

            if (this->last_check_ms_ != 0) {    
                float minutes_passed = (now - this->last_check_ms_) / 60000.0f;

                if (minutes_passed > 10.0f) {
                    if (this->fc_active_) {
                        ESP_LOGD(OPTIMIZER_TAG, "Free heating/cooling window reset after %.0f min gap (reconnect/reboot)", minutes_passed);
                        this->fc_active_ = false;
                    }
                    this->last_check_ms_ = now;
                    return;
                }

                // Track runtimes separately
                if (is_running && is_heating_active) {
                    this->daily_runtime_global += minutes_passed;
                }
                if (is_running && is_cooling_active) {
                    this->daily_runtime_cool_ += minutes_passed;
                }

                // --- Free cooling window (HP-off period, any time of day) ---
                float current_room_tmp = this->get_room_current_temp(OptimizerZone::ZONE_1);
                
                // The HP is only "off" for passive drift learning if it's NOT heating AND NOT cooling.
                bool hp_off = !is_running || (!is_heating_active && !is_cooling_active);

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
                            
                            if (avg_sol < 150.0f) {
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
                                            auto call = this->state_.num_raw_hl_tm_product->make_call();
                                            call.set_value(next);
                                            call.perform();
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
                                // SOLAR ACTIVE: Learn passive solar gain factor.
                                // Unit: kWh of heat input per W/m² irradiance (independent of solar_kwp).
                                // Formula: (K_gained × TM) / (W/m² × hours)
                                // Typical range: 0.001–0.05 kWh/W/m²
                                if (this->state_.num_raw_solar_factor != nullptr && this->state_.num_raw_hl_tm_product != nullptr) {
                                    float tau = this->state_.num_raw_hl_tm_product->state;

                                    if (tau > 5.0f) {  // only when Tau is reliably learned
                                        float expected_drop = (delta_T_avg * t_hours) / tau;
                                        float free_heating_kelvin = expected_drop - delta_cool;

                                        // avg_sol is raw W/m² irradiance
                                        if (free_heating_kelvin > 0.0f && avg_sol > 50.0f) {
                                            // TM = Tau * HL. Derive HL from EMA stats using the same
                                            // formula as odin_server: avg_thermal_power / delta_T_out.
                                            // Fallback to 0.22 kW/K if data is insufficient.
                                            float derived_heat_loss = 0.22f;
                                            if (this->state_.num_raw_heat_produced != nullptr &&
                                                this->state_.num_raw_avg_room_temp != nullptr &&
                                                this->state_.num_raw_avg_outside_temp != nullptr) {
                                                float ema_heat    = this->state_.num_raw_heat_produced->state;
                                                float ema_room    = this->state_.num_raw_avg_room_temp->state;
                                                float ema_outside = this->state_.num_raw_avg_outside_temp->state;
                                                float delta_t_out = ema_room - ema_outside;
                                                if (delta_t_out > 2.0f && ema_heat > 0.1f) {
                                                    float hl = (ema_heat / 24.0f) / delta_t_out;
                                                    derived_heat_loss = std::max(0.05f, std::min(0.6f, hl));
                                                }
                                            }
                                            float thermal_mass_est = tau * derived_heat_loss;
                                            float learned_solar_factor = (free_heating_kelvin * thermal_mass_est) / (avg_sol * t_hours);

                                            // Physical range: 0.001–0.05 kWh/W/m²
                                            learned_solar_factor = std::max(0.001f, std::min(0.05f, learned_solar_factor));

                                            float cur = this->state_.num_raw_solar_factor->state;
                                            float next = (cur <= 0.001f || std::isnan(cur)) ? learned_solar_factor
                                                         : (0.20f * learned_solar_factor + 0.80f * cur);
                                            auto call = this->state_.num_raw_solar_factor->make_call();
                                            call.set_value(next);
                                            call.perform();

                                            ESP_LOGI(OPTIMIZER_TAG,
                                                "Passive solar factor: %.4f kWh/W/m² (avg_sol=%.0fW/m², expected_drop=%.2fK, actual_drop=%.2fK, hl=%.3f)",
                                                next, avg_sol, expected_drop, delta_cool, derived_heat_loss);
                                        } else {
                                            ESP_LOGD(OPTIMIZER_TAG,
                                                "Passive solar: no measurable gain (free_K=%.2f, sol=%.0fW/m²)",
                                                free_heating_kelvin, avg_sol);
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

                // Delay the fetch by 30s so NVS writes from update_learning_model
                // finish before the HTTP call starts — both on the main loop,
                // immediate trigger risks watchdog timeout at midnight.
                this->odin_fetch_pending_ms_ = millis() + 180000; // fire after 3 min — waits for ODIN 00:01 refresh

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
                this->daily_runtime_cool_ = 0.0f;

                // Reset daily accumulators
                this->last_total_heating_produced_ = 0.0f;
                this->last_total_heating_consumed_ = 0.0f;
                this->last_total_cooling_produced_ = 0.0f;
                this->last_total_cooling_consumed_ = 0.0f;

                this->last_total_dhw_produced_ = 0.0f;
                this->last_total_dhw_consumed_ = 0.0f;
                
                // Reset global trackers to prevent false deltas across midnight
                this->last_global_prod_ = -1.0f;
                this->last_global_cons_ = -1.0f;

                // Reset wind-down window — any session spanning midnight gets cleanly cut off
                this->last_was_dhw_     = false;
                this->last_was_heating_ = false;
                this->last_run_time_    = UINT32_MAX - 700000UL;
            }

            // PENDING FETCH: fired by day transition after 30s delay
            if (this->odin_fetch_pending_ms_ > 0 && millis() >= this->odin_fetch_pending_ms_) {
                this->odin_fetch_pending_ms_ = 0;
                this->set_odin_fetch_request();
                ESP_LOGI(OPTIMIZER_TAG, "Day transition: delayed fetch now firing.");
            }

            // HOURLY MPC TRIGGER
            if (this->solver_enabled()) {
                time_t ts = this->state_.ecodan_instance->get_status().timestamp();
                if (ts > 0) {
                    struct tm t;
                    localtime_r(&ts, &t);
                    int cur_min = t.tm_min;
                    // don't trigger for 23:55 since day transition will trigger MPC
                    if (current_hour != 23 && cur_min >= 55 && current_hour != this->last_pre_hour_triggered_) {
                        this->last_pre_hour_triggered_ = current_hour;
                        ESP_LOGI(OPTIMIZER_TAG, "Pre-hour trigger at %02d:55 — requesting ODIN solve for hour %d.",
                                current_hour, (current_hour + 1) % 24);
                        this->set_odin_fetch_request();
                    }
                }

                // if (current_hour != this->last_processed_hour_) {
                //     ESP_LOGD(OPTIMIZER_TAG, "Hour transition detected (%d -> %d).", this->last_processed_hour_, current_hour);
                //     //this->set_odin_fetch_request();
                //     this->last_processed_hour_ = current_hour;
                // } 
                // else {
                //     time_t ts = this->state_.ecodan_instance->get_status().timestamp();
                //     if (ts > 0) {
                //         struct tm t;
                //         localtime_r(&ts, &t);
                //         int cur_min = t.tm_min;
                //         if (cur_min >= 55 && current_hour != this->last_pre_hour_triggered_) {
                //             this->last_pre_hour_triggered_ = current_hour;
                //             ESP_LOGI(OPTIMIZER_TAG, "Pre-hour trigger at %02d:55 — requesting ODIN solve for hour %d.",
                //                     current_hour, (current_hour + 1) % 24);
                //             this->set_odin_fetch_request();
                //         }
                //     }
                // }
            }

            // Track the last active mode persistently to catch delayed meter updates and wind-down.
            // Uses member variables (not statics) so state is tied to the Optimizer instance
            // and resets correctly on day rollover.
            if (is_running) {
                this->last_run_time_ = millis();
                this->last_was_dhw_ = (status.Operation == esphome::ecodan::Status::OperationMode::DHW_ON ||
                                       status.Operation == esphome::ecodan::Status::OperationMode::LEGIONELLA_PREVENTION);
                this->last_was_heating_ = is_heating_active;
                this->last_was_cooling_ = is_cooling_active;
            }

            bool dhw_active_window = false;
            bool heat_active_window = false;
            bool cool_active_window = false;

            if (is_running || (millis() - this->last_run_time_ <= 600000)) {
                if (this->last_was_dhw_)          dhw_active_window  = true;
                else if (this->last_was_heating_) heat_active_window = true;
                else if (this->last_was_cooling_) cool_active_window = true;
            } else {
                // Window expired — clear so next session starts clean
                this->last_was_dhw_     = false;
                this->last_was_heating_ = false;
                this->last_was_cooling_ = false;
            }

            // Fetch current global meter states
            float current_global_prod = (this->state_.daily_heating_produced != nullptr && this->state_.daily_heating_produced->has_state()) 
                                        ? this->state_.daily_heating_produced->state : NAN;
            
            float current_global_cons = NAN;
            if (this->state_.solver_kwh_meter_feedback_source == nullptr ||
                this->state_.solver_kwh_meter_feedback_source->active_index().value_or(0) == 0) {
                if (this->state_.daily_heating_consumed != nullptr && this->state_.daily_heating_consumed->has_state())
                    current_global_cons = this->state_.daily_heating_consumed->state;
            } else {
                if (this->state_.solver_kwh_meter_feedback != nullptr && this->state_.solver_kwh_meter_feedback->has_state())
                    current_global_cons = this->state_.solver_kwh_meter_feedback->state;
            }

            // 1. Process Production Delta.
            // On midnight reset the sensor drops back to 0, giving a negative delta.
            // Re-anchor the baseline without emitting a delta so the next tick is correct.
            float delta_prod = 0.0f;
            if (!std::isnan(current_global_prod)) {
                if (this->last_global_prod_ < 0) {
                    // First valid reading after boot — anchor, emit nothing
                    this->last_global_prod_ = current_global_prod;
                } else {
                    float raw_prod = current_global_prod - this->last_global_prod_;
                    if (raw_prod >= 0) {
                        delta_prod = raw_prod;
                    } else {
                        // Midnight reset detected — re-anchor silently
                        ESP_LOGD(OPTIMIZER_TAG, "Prod meter reset (%.2f -> %.2f), re-anchoring",
                                 this->last_global_prod_, current_global_prod);
                    }
                    this->last_global_prod_ = current_global_prod;
                }
            }

            // 2. Process Consumption Delta.
            // Same midnight reset handling: re-anchor without emitting a delta.
            // Standby power accumulated since midnight is correctly ignored.
            float delta_cons = 0.0f;
            if (!std::isnan(current_global_cons)) {
                if (this->last_global_cons_ < 0) {
                    // First valid reading after boot — anchor, emit nothing
                    this->last_global_cons_ = current_global_cons;
                } else {
                    float raw_cons = current_global_cons - this->last_global_cons_;
                    if (raw_cons >= 0) {
                        delta_cons = raw_cons;
                    } else {
                        // Midnight reset detected — re-anchor silently
                        ESP_LOGD(OPTIMIZER_TAG, "Cons meter reset (%.2f -> %.2f), re-anchoring",
                                 this->last_global_cons_, current_global_cons);
                    }
                    this->last_global_cons_ = current_global_cons;
                }
            }

            // 3. Bucket the deltas accurately.
            // Ignore physically impossible hardware spikes (>50 kWh in a minute).
            if (delta_prod < 50.0f && delta_cons < 50.0f) {
                if (heat_active_window) {
                    this->last_total_heating_produced_ += delta_prod;
                    this->last_total_heating_consumed_ += delta_cons;
                } else if (cool_active_window) {
                    this->last_total_cooling_produced_ += delta_prod;
                    this->last_total_cooling_consumed_ += delta_cons;
                } else if (dhw_active_window) {
                    this->last_total_dhw_produced_ += delta_prod;
                    this->last_total_dhw_consumed_ += delta_cons;
                }
                // Pure standby (no active window) falls through and is safely ignored
            }

            // Maintain total raw consumption for YAML dashboard tracking
            this->last_total_all_consumed_ = current_global_cons;
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

            // Cooling local variables
            float cool_runtime_hours = this->daily_runtime_cool_ / 60.0f;
            float cool_produced_kwh = this->last_total_cooling_produced_;
            float cool_elec_consumed_kwh = this->last_total_cooling_consumed_;

            ESP_LOGI(OPTIMIZER_TAG, "Daily Raw Stats: Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, MaxOut=%.1fkW, AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC",
                     heat_produced_kwh, elec_consumed_kwh, runtime_hours, max_out_kw, avg_outside, avg_room, delta_room);

            // Helper for Exponential Moving Average (EMA) on Number components
            auto update_ema_num = [](esphome::number::Number *comp, float observed, float alpha) {
                if (comp == nullptr) return;
                float current = comp->state;
                if (current <= 0.01f || std::isnan(current)) current = observed; // Initialize if empty
                else current = (alpha * observed) + ((1.0f - alpha) * current);
                
                // Use make_call so ESPHome's template number saves to NVS (restore_value)
                auto call = comp->make_call();
                call.set_value(current);
                call.perform();
            };

            const float ALPHA = 0.15f; 

            // ALWAYS UPDATE: Passive Data & Building Physics ---
            update_ema_num(this->state_.num_raw_avg_room_temp, avg_room, ALPHA);
            update_ema_num(this->state_.num_raw_delta_room_temp, delta_room, ALPHA);

            // ONLY UPDATE WHEN HEATING OR COOLING: System Performance ---
            if (heat_produced_kwh >= 2.0f && runtime_hours >= 1.0f) {
                update_ema_num(this->state_.num_raw_heat_produced, heat_produced_kwh, ALPHA);
                update_ema_num(this->state_.num_raw_elec_consumed, elec_consumed_kwh, ALPHA);
                update_ema_num(this->state_.num_raw_runtime_hours, runtime_hours, ALPHA);
                update_ema_num(this->state_.num_raw_avg_outside_temp, avg_outside, ALPHA);

                ESP_LOGI(OPTIMIZER_TAG, "Full Heating update (15%% EMA): Heat=%.1fkWh, Elec=%.1fkWh, Run=%.1fh, AvgOut=%.1fC, AvgRoom=%.1fC",
                         this->state_.num_raw_heat_produced->state, this->state_.num_raw_elec_consumed->state, 
                         this->state_.num_raw_runtime_hours->state, this->state_.num_raw_avg_outside_temp->state,
                         this->state_.num_raw_avg_room_temp->state);

            } else if (cool_produced_kwh >= 2.0f && cool_runtime_hours >= 1.0f) {
                // Separate parallel track for cooling statistics (Energy Efficiency Ratio / EER modeling)
                update_ema_num(this->state_.num_raw_cool_produced, cool_produced_kwh, ALPHA);
                update_ema_num(this->state_.num_raw_cool_elec_consumed, cool_elec_consumed_kwh, ALPHA);
                update_ema_num(this->state_.num_raw_cool_runtime_hours, cool_runtime_hours, ALPHA);
                update_ema_num(this->state_.num_raw_cool_avg_outside_temp, avg_outside, ALPHA);

                ESP_LOGI(OPTIMIZER_TAG, "Full Cooling update (15%% EMA): CoolProd=%.1fkWh, CoolElec=%.1fkWh, Run=%.1fh, AvgOut=%.1fC",
                         this->state_.num_raw_cool_produced->state, this->state_.num_raw_cool_elec_consumed->state, 
                         this->state_.num_raw_cool_runtime_hours->state, this->state_.num_raw_cool_avg_outside_temp->state);

            } else {
                // Output a log message, but do not abort before passive stats are saved
                ESP_LOGI(OPTIMIZER_TAG, "Passive stats saved (AvgOut=%.1fC, AvgRoom=%.1fC, DeltaRoom=%.1fC). Heating/Cooling skipped (<2kWh or <1h).",
                         this->state_.num_raw_avg_outside_temp->state, 
                         this->state_.num_raw_avg_room_temp->state, 
                         this->state_.num_raw_delta_room_temp->state);
            }
        }

    } // namespace optimizer
} // namespace esphome
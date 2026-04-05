#include "optimizer.h"
#include <algorithm>
#include <cmath>

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        // ─────────────────────────────────────────────────────────────────
        // Heating system profile lookup
        // ─────────────────────────────────────────────────────────────────

        HeatingProfile Optimizer::get_heating_profile_(int type_index) {
            HeatingProfile p;
            if (type_index <= 1) {
                // UFH
                p.base_min_delta_t    = 2.0f;
                p.min_delta_cold_limit = 4.0f;
                p.max_delta_t         = 6.5f;
                p.max_error_range     = 2.0f;
                p.defrost_memory_ms   = 35 * 60 * 1000UL;
            } else if (type_index <= 3) {
                // Hybrid
                p.base_min_delta_t    = 3.0f;
                p.min_delta_cold_limit = 5.0f;
                p.max_delta_t         = 8.0f;
                p.max_error_range     = 2.0f;
                p.defrost_memory_ms   = 25 * 60 * 1000UL;
            } else {
                // Radiator
                p.base_min_delta_t    = 4.0f;
                p.min_delta_cold_limit = 6.0f;
                p.max_delta_t         = 10.0f;
                p.max_error_range     = 1.5f;
                p.defrost_memory_ms   = 15 * 60 * 1000UL;
            }
            return p;
        }

        // ─────────────────────────────────────────────────────────────────
        // defrost sates — locked during/after defrost
        // ─────────────────────────────────────────────────────────────────

        Optimizer::DefrostState Optimizer::resolve_defrost_state_() {
            auto &status = this->state_.ecodan_instance->get_status();
            const uint32_t LOCK_DURATION_MS = 15 * 60 * 1000;
            bool in_lock_window = (this->last_defrost_time_ > 0) && 
                                  ((millis() - this->last_defrost_time_) < LOCK_DURATION_MS);

            if (status.DefrostActive || in_lock_window) {
                ESP_LOGD(OPTIMIZER_TAG, "Using locked defrost states: outside temp: %.1f, hp return: %.1f, z1 return: %.1f, z2 return: %.1f for adaptive calculations.", 
                    this->state_before_defrost_.locked_outside_temp_, this->state_before_defrost_.locked_return_temp_, 
                    this->state_before_defrost_.locked_return_temp_z1_, this->state_before_defrost_.locked_return_temp_z2_);
            }
            else {
                // clear state, outside defrost window
                this->state_before_defrost_ = DefrostState{};
            }

            return this->state_before_defrost_;
        }

        // ─────────────────────────────────────────────────────────────────
        // ODIN solver bias — mutex-safe, returns {bias, heating_off}
        // ─────────────────────────────────────────────────────────────────
        Optimizer::SolverResult Optimizer::resolve_solver_result_(float room_target_temp, float current_room_temp) {
            SolverResult result{-1.0f, false, OptimizerOperationMode::UNAVAILABLE, -1 };

            if (this->state_.sw_use_solver == nullptr || !this->state_.sw_use_solver->state)
                return result;

            if (this->odin_mutex_ == NULL || xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(10)) != pdTRUE)
                return result;

            // Check if we have both energy (for on/off) and production (for scaling)
            if (this->odin_data_ready_ && !this->odin_production_.empty() && !this->odin_operation_mode_.empty()) {
                int current_day  = this->get_current_ecodan_day();
                int current_hour = this->get_current_ecodan_hour();

                if (current_day != this->odin_data_day_) {
                    // Day rolled over — data is stale, wait for new solve
                    this->odin_data_ready_ = false;
                    xSemaphoreGive(this->odin_mutex_);
                    return result;
                }

                if (current_hour >= 0 && current_hour < 24) {
                    auto mode = to_operation_mode(this->odin_operation_mode_[current_hour]);
                    float odin_prod   = this->odin_production_[current_hour];
                    xSemaphoreGive(this->odin_mutex_);

                    // NAN = no data for this hour yet — fall back to AA, no soft-stop
                    if (std::isnan(odin_prod) || mode == OptimizerOperationMode::UNAVAILABLE) {
                        ESP_LOGW(OPTIMIZER_TAG, "ODIN data is NAN at hour %d. Forcing fallback.", current_hour);
                        return result;
                    }

                    result.current_hour = current_hour;
                    result.mode = mode;
                    result.heating_off = (odin_prod < 0.1f);
                    if (result.heating_off) {
                        apply_solver_soft_stop(true);
                        return result;
                    }

                    apply_solver_soft_stop(false);
                    
                    int heating_type_index = 0;
                    if (this->state_.heating_system_type != nullptr) {
                        heating_type_index = this->state_.heating_system_type->active_index().value_or(0);
                    }
                    HeatingProfile prof = this->get_heating_profile_(heating_type_index);

                    float max_out = 7.0f;
                    if (this->odin_max_output_ != 0) {
                        max_out = this->odin_max_output_;
                    }
                    if (max_out < 1.0f) max_out = 7.0f;
                    
                    // Calculate how hard ODIN wants the heat pump to work (Ratio between 0.0 and 1.0)
                    result.load_ratio = std::clamp(odin_prod / max_out, 0.0f, 1.0f);
                    
                    ESP_LOGD(OPTIMIZER_TAG, "ODIN -> Hour %d | Prod: %.1f/%.1f | factor: %.2f | mode: %d", current_hour, odin_prod, max_out, result.load_ratio, static_cast<uint8_t>(mode));
                    
                    return result;
                }
            }

            xSemaphoreGive(this->odin_mutex_);
            return result;
        }

        // ─────────────────────────────────────────────────────────────────
        // Heating flow calculation (Delta-T + defrost ramp + step-down)
        // ─────────────────────────────────────────────────────────────────

        float Optimizer::calculate_heating_flow_(std::size_t zone_i,
                                                  const ecodan::Status &status,
                                                  const HeatingProfile &prof,
                                                  bool set_point_reached,
                                                  float cold_factor,
                                                  float actual_outside_temp,
                                                  float zone_min, float zone_max,
                                                  float error_factor,
                                                  float smart_boost) {

            auto defrost_state = this->resolve_defrost_state_();
            float locked_return_temp = defrost_state.get_return_temp(
                status.has_independent_zone_temps(),
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);

            float actual_return_temp = this->get_return_temp(
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);

            if (!isnan(locked_return_temp)) {
                actual_return_temp = locked_return_temp;
            }

            float dynamic_min = prof.base_min_delta_t +
                                cold_factor * (prof.min_delta_cold_limit - prof.base_min_delta_t);
            float target_delta = dynamic_min + error_factor * smart_boost * (prof.max_delta_t - dynamic_min);

            if (isnan(actual_return_temp)) {
                ESP_LOGE(OPTIMIZER_TAG, "Z%d HEATING: actual_return_temp is NAN. Reverting to %.2f°C.",
                         (zone_i + 1), zone_min);
                return this->clamp_flow_temp(zone_min, zone_min, zone_max);
            }

            float calculated_flow;

            // Defrost recovery ramp
            const float DEFROST_RISK_MIN = -2.0f, DEFROST_RISK_MAX = 3.0f;
            bool defrost_enabled = this->state_.defrost_risk_handling_enabled->state;
            bool is_defrost_weather = false;
            uint32_t now = millis();

            if (actual_outside_temp >= DEFROST_RISK_MIN && actual_outside_temp <= DEFROST_RISK_MAX
                && this->last_defrost_time_ > 0
                && (now - this->last_defrost_time_) < prof.defrost_memory_ms) {
                is_defrost_weather = true;
            }

            if (is_defrost_weather && defrost_enabled) {
                float ratio       = std::clamp((float)(now - this->last_defrost_time_) / prof.defrost_memory_ms, 0.0f, 1.0f);
                float ramped_delta = prof.base_min_delta_t + fmax(target_delta - prof.base_min_delta_t, 0.0f) * ratio;
                calculated_flow   = actual_return_temp + ramped_delta;
                calculated_flow   = this->round_nearest(calculated_flow);
                ESP_LOGW(OPTIMIZER_TAG, "Z%d Defrost Recovery: %.0f%% done. Flow: %.2f",
                         (zone_i + 1), ratio * 100.0f, calculated_flow);
            } else {
                if (set_point_reached) {
                    // Setpoint reached — fall back to base delta
                    calculated_flow = actual_return_temp + prof.base_min_delta_t;
                    ESP_LOGD(OPTIMIZER_TAG, "Z%d Setpoint reached. Base delta T.", (zone_i + 1));
                } else {
                    calculated_flow = actual_return_temp + target_delta;
                }
                calculated_flow = this->round_nearest(calculated_flow);

                // Predictive boost adjustment
                auto &pcp_adj = (zone_i == 1) ? this->pcp_adjustment_z2_ : this->pcp_adjustment_z1_;
                float actual_flow_temp = this->get_feed_temp(
                    (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);

                if (pcp_adj > 0.0f) {
                    if ((actual_flow_temp - calculated_flow) >= 1.0f) {
                        calculated_flow += pcp_adj;
                    } else {
                        pcp_adj = 0.0f;
                    }
                }
                ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING: flow=%.2f°C, return=%.2f°C (boost %.1f)", (zone_i + 1), calculated_flow, actual_return_temp, pcp_adj);
            }

            // Clamp + step-down (order depends on post-DHW window)
            if (this->is_post_dhw_window(status)) {
                calculated_flow = this->clamp_flow_temp(calculated_flow, zone_min, zone_max);
                calculated_flow = this->enforce_step_down(status,
                    this->get_feed_temp((zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2),
                    calculated_flow);
            } else {
                calculated_flow = this->enforce_step_down(status,
                    this->get_feed_temp((zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2),
                    calculated_flow);
                calculated_flow = this->clamp_flow_temp(calculated_flow, zone_min, zone_max);
            }

            return calculated_flow;
        }

        // ─────────────────────────────────────────────────────────────────
        // Cooling flow calculation
        // ─────────────────────────────────────────────────────────────────

        float Optimizer::calculate_cooling_flow_(std::size_t zone_i,
                                                  const ecodan::Status &status,
                                                  float target_delta_t) {
            float actual_return_temp = this->get_return_temp(
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);

            float calculated_flow;
            if (isnan(actual_return_temp)) {
                ESP_LOGE(OPTIMIZER_TAG, "Z%d COOLING: actual_return_temp is NAN. Reverting to smart start temp.", (zone_i + 1));
                calculated_flow = this->state_.cooling_smart_start_temp->state;
            } else {
                calculated_flow = actual_return_temp - target_delta_t;
                ESP_LOGD(OPTIMIZER_TAG, "Z%d COOLING: calc=%.1f°C (return %.1f - delta %.1f)",
                         (zone_i + 1), calculated_flow, actual_return_temp, target_delta_t);
            }

            calculated_flow = this->clamp_flow_temp(calculated_flow,
                                                    this->state_.minimum_cooling_flow_temp->state,
                                                    this->state_.cooling_smart_start_temp->state);
            return this->round_nearest_half(calculated_flow);
        }

        // ─────────────────────────────────────────────────────────────────
        // Per-zone adaptive processing
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::process_adaptive_zone_(std::size_t i,
                                               const ecodan::Status &status,
                                               const HeatingProfile &prof,
                                               float cold_factor,
                                               float actual_outside_temp,
                                               float zone_max, float zone_min,
                                               float &out_flow_heat,
                                               float &out_flow_cool) {
            auto zone = (i == 0) ? esphome::ecodan::Zone::ZONE_1 : esphome::ecodan::Zone::ZONE_2;
            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);

            bool is_heating_mode   = status.is_auto_adaptive_heating(zone);
            bool is_heating_active = status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;
            bool is_cooling_mode   = status.has_cooling() && status.is_auto_adaptive_cooling(zone);
            bool is_cooling_active = status.Operation == esphome::ecodan::Status::OperationMode::COOL_ON;

            // Multi-zone heating active refinement
            if (is_heating_active && status.has_2zones()) {
                auto mz = status.MultiZoneStatus;
                if (i == 0 && (mz == 0 || mz == 3)) is_heating_active = false;
                if (i == 1 && (mz == 0 || mz == 2)) is_heating_active = false;
                if (status.has_independent_zone_temps() && (status.WaterPump2Active || status.WaterPump3Active))
                    is_heating_active = true;
            }

            if (!is_heating_mode && !is_cooling_mode) return;

            float room_temp         = this->get_room_current_temp((i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);
            float room_target_temp  = this->get_room_target_temp((i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);
            float actual_flow_temp  = this->get_feed_temp((i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);
            float flow_rate = status.FlowRate;

            if (isnan(room_temp) || isnan(room_target_temp) || isnan(actual_flow_temp)) return;

            float setpoint_bias = this->state_.auto_adaptive_setpoint_bias->state;
            if (isnan(setpoint_bias)) setpoint_bias = 0.0f;

            auto feedback_src = (i == 0)
                ? this->state_.temperature_feedback_source_z1->active_index().value_or(0)
                : this->state_.temperature_feedback_source_z2->active_index().value_or(0);

            ESP_LOGD(OPTIMIZER_TAG,
                "Z%d src=%d room=%.1f target=%.1f flow=%.1f flow_rate=%.1f outside=%.1f bias=%.1f H=%d C=%d",
                (i + 1), feedback_src, room_temp, room_target_temp,
                actual_flow_temp, flow_rate, actual_outside_temp, setpoint_bias, is_heating_active, is_cooling_active);
            
            room_target_temp += setpoint_bias;

            float error           = is_heating_mode ? (room_target_temp - room_temp) : (room_temp - room_target_temp);
            bool  use_linear      = (heating_type_index % 2 != 0);
            float error_positive  = fmax(error, 0.0f);

            // Scale max_error_range down with cold_factor so the system reaches
            // full output at a smaller room error when it's cold outside.
            float effective_error_range = prof.max_error_range * (1.0f - 0.5f * std::min(cold_factor, 1.0f));
            float x               = fmin(error_positive / effective_error_range, 1.0f);
            float error_factor    = use_linear ? x : x * x * (3.0f - 2.0f * x);
            float smart_boost     = is_heating_mode ? this->calculate_smart_boost(heating_type_index, error) : 1.0f;
            float dynamic_min     = prof.base_min_delta_t + cold_factor * (prof.min_delta_cold_limit - prof.base_min_delta_t);

            bool set_point_reached = error < 0.0f;
            bool solver_enabled = this->solver_enabled();

            if (solver_enabled) {
                static int odin_last_executed_dhw_hour_ = -1;

                auto [solver_load_ratio, solver_heating_off, solver_operating_mode, current_hour] = this->resolve_solver_result_(room_target_temp, room_temp);
                if (solver_operating_mode == OptimizerOperationMode::DHW_ON) {
                    if (this->state_.sw_force_dhw != nullptr && !this->state_.sw_force_dhw->state) {
                        if (odin_last_executed_dhw_hour_ != current_hour) {
                            if (!this->is_dhw_active(status)) {
                                ESP_LOGD(OPTIMIZER_TAG, "ODIN Starting executing planned DHW");
                                this->state_.sw_force_dhw->turn_on();
                                odin_last_executed_dhw_hour_ = current_hour;
                            }
                        }
                    } else {
                        ESP_LOGD(OPTIMIZER_TAG, "ODIN DHW planned, but not configured");
                    }
                } else {
                    odin_last_executed_dhw_hour_ = -1;
                }

                if (solver_load_ratio < 0.0f && !solver_heating_off) {
                    if (millis() < 2*60000) {
                        ESP_LOGD(OPTIMIZER_TAG, "Z%d ODIN enabled but data not ready. Skipping one AA iteration.", (i + 1));
                        return;
                    }
                    else {
                        ESP_LOGD(OPTIMIZER_TAG, "Z%d ODIN data still pending (+5m). Falling back to auto adaptive.", (i + 1));
                    }
                } 
                else if (solver_heating_off || solver_operating_mode == OptimizerOperationMode::OFF) {
                    ESP_LOGD(OPTIMIZER_TAG,
                        "Z%d ODIN heating stopped, solver_load_ratio: %.2f, mode: %d",
                        (i + 1), solver_load_ratio, static_cast<uint8_t>(solver_operating_mode));
                    return;
                } 
                else {
                    float desired_delta = 0.0f;

                    // If we have a reliable flow rate, use the deterministic physical formula.
                    // Otherwise, fall back to the heuristic load-ratio mapping.
                    if (flow_rate > 5.0f) {
                        float max_out = 7.0f;
                        if (this->odin_max_output_ != 0) {
                            max_out = this->odin_max_output_;
                        }
                        
                        // kW = (flow / 60) * (delta T) * 4.18
                        // delta T = (kW * 60) / (flow * 4.18)
                        float shc_override = this->state_.ecodan_instance->get_specific_heat_constant();
                        float specific_heat_constant = std::isnan(shc_override) ? status.estimate_water_constant(actual_flow_temp) : shc_override;
                        float target_kw = solver_load_ratio * max_out;
                        float physical_delta = (target_kw * 60.0f) / (flow_rate * specific_heat_constant);
                        desired_delta = std::clamp(physical_delta, prof.base_min_delta_t, prof.max_delta_t);

                        ESP_LOGD(OPTIMIZER_TAG,
                            "Z%d ODIN (Physical) demands %.1fkW. Flow: %.1f L/min -> \u0394T: %.2f (Clamped: %.2f)",
                            (i + 1), target_kw, flow_rate, physical_delta, desired_delta);
                    } else {
                        // Fallback: Heuristic linear mapping when flow is too low/starting up
                        desired_delta = prof.base_min_delta_t + solver_load_ratio * (prof.max_delta_t - prof.base_min_delta_t);
                        
                        ESP_LOGD(OPTIMIZER_TAG,
                            "Z%d ODIN (Heuristic) Low Flow (%.1f L/min). Load Ratio: %.2f -> \u0394T: %.2f",
                            (i + 1), flow_rate, solver_load_ratio, desired_delta);
                    }
                    
                    // Reverse-engineer the error_factor so calculate_heating_flow_ produces the exact desired Delta T
                    if (prof.max_delta_t > dynamic_min) {
                        error_factor = (desired_delta - dynamic_min) / (prof.max_delta_t - dynamic_min);
                        error_factor = std::max(0.0f, error_factor); // Allow > 1.0 if physical demand is higher than profile max
                    } else {
                        error_factor = 1.0f;
                    }
                    
                    // Disable local smart boost, and setpoint reached
                    smart_boost = 1.0f; 
                    set_point_reached = false;

                    ESP_LOGD(OPTIMIZER_TAG,
                        "Z%d ODIN Final mapped error_factor: %.2f",
                        (i + 1), error_factor);
                }
            }

            float target_delta = dynamic_min + error_factor * smart_boost * (prof.max_delta_t - dynamic_min);
            ESP_LOGD(OPTIMIZER_TAG,
                "Z%d target_delta=%.2f cold_factor=%.2f dyn_min=%.2f eff_range=%.2f error_factor=%.2f boost=%.2f linear=%d, use_solver=%d",
                (i + 1), target_delta, cold_factor, dynamic_min, effective_error_range, error_factor, smart_boost, use_linear, solver_enabled);

            if (is_heating_mode) {
                out_flow_heat = this->calculate_heating_flow_(
                    i, status, prof, set_point_reached,
                    cold_factor, actual_outside_temp,
                    zone_min, zone_max, error_factor, smart_boost);
            } else if (is_cooling_mode && is_cooling_active) {
                out_flow_cool = this->calculate_cooling_flow_(i, status, target_delta);
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // Main loop entry point
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::run_auto_adaptive_loop() {
            if (this->adaptive_loop_running_) {
                ESP_LOGD(OPTIMIZER_TAG, "run_auto_adaptive_loop: re-entrant call ignored");
                return;
            }
            this->adaptive_loop_running_ = true;

            bool aa_enabled = this->aa_enabled();
            bool solver_enabled = this->solver_enabled();
            auto *override_z1 = this->state_.sw_odin_override_z1;
            auto *override_z2 = this->state_.sw_odin_override_z2;

            // If the user disables Auto Adaptive or the Solver, we must ensure the hardware relays are released.
            if (!aa_enabled || !solver_enabled) {
                bool z1_locked = override_z1 != nullptr && override_z1->state;
                bool z2_locked = override_z2 != nullptr && override_z2->state;
                
                if (z1_locked || z2_locked || this->solver_stop_active_) {
                    ESP_LOGI(OPTIMIZER_TAG, "Solver/AA disabled, but override switches are active. Forcing release.");
                    if (override_z1 != nullptr && override_z1->state) override_z1->turn_off();
                    if (override_z2 != nullptr && override_z2->state) override_z2->turn_off();
                }
            }
            else {
                // aa and solver enabled, ensure that override is on
                if (override_z1 != nullptr && !override_z1->state) override_z1->turn_on();
                if (override_z2 != nullptr && !override_z2->state) override_z2->turn_on();
            }

            if (!aa_enabled) {
                this->adaptive_loop_running_ = false;
                return;
            }

            auto &status = this->state_.ecodan_instance->get_status();

            if (this->is_system_hands_off(status)) {
                ESP_LOGD(OPTIMIZER_TAG, "System is busy (DHW/Defrost/Lockout). Exiting.");
                this->adaptive_loop_running_ = false;
                return;
            }

            if (status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP) {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 1 not in fixed flow mode. Exiting.");
                this->adaptive_loop_running_ = false;
                return;
            }

            if (status.has_2zones() &&
                status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP) {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 2 not in fixed flow mode. Exiting.");
                this->adaptive_loop_running_ = false;
                return;
            }

            if (isnan(this->state_.hp_feed_temp->state) || isnan(status.OutsideTemperature)) {
                ESP_LOGW(OPTIMIZER_TAG, "Sensor data unavailable. Exiting.");
                this->adaptive_loop_running_ = false;
                return;
            }

            auto defrost_state = this->resolve_defrost_state_();

            float actual_outside_temp = isnan(defrost_state.locked_outside_temp_) ? status.OutsideTemperature : defrost_state.locked_outside_temp_;
            if (isnan(actual_outside_temp)) {
                this->adaptive_loop_running_ = false;
                return;
            }

            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);
            auto prof = this->get_heating_profile_(heating_type_index);

            // Cold factor: quadratic, expanded to 1.5
            const float MILD = 15.0f, COLD = -5.0f;
            float cf_raw   = (MILD - std::clamp(actual_outside_temp, COLD, MILD)) / (MILD - COLD);
            float cold_factor = cf_raw * cf_raw * 1.5f;

            ESP_LOGD(OPTIMIZER_TAG,
                "[*] Auto-adaptive cycle: independent_zone_temps=%d has_cooling=%d cold_factor=%.2f min_delta=%.2f max_delta=%.2f",
                status.has_independent_zone_temps(), status.has_cooling(), cold_factor, prof.base_min_delta_t, prof.max_delta_t);

            auto max_zones = status.has_2zones() ? 2 : 1;

            // Resolve per-zone flow limits
            float max_flow[2], min_flow[2];
            max_flow[0] = this->state_.maximum_heating_flow_temp->state;
            min_flow[0] = this->state_.minimum_heating_flow_temp->state;
            if (min_flow[0] > max_flow[0]) {
                ESP_LOGW(OPTIMIZER_TAG, "Z1 Min/Max conflict. Forcing min=max (%.1f)", max_flow[0]);
                min_flow[0] = max_flow[0];
            }
            max_flow[1] = max_flow[0];
            min_flow[1] = min_flow[0];

            if (status.has_2zones()) {
                max_flow[1] = this->state_.maximum_heating_flow_temp_z2->state;
                min_flow[1] = this->state_.minimum_heating_flow_temp_z2->state;
                if (min_flow[1] > max_flow[1]) {
                    ESP_LOGW(OPTIMIZER_TAG, "Z2 Min/Max conflict. Forcing min=max (%.1f)", max_flow[1]);
                    min_flow[1] = max_flow[1];
                }
            }

            float flows_heat[2] = {0.0f,   0.0f};
            float flows_cool[2] = {100.0f, 100.0f};

            for (std::size_t i = 0; i < max_zones; i++) {
                this->process_adaptive_zone_(
                    i, status, prof, cold_factor, actual_outside_temp,
                    max_flow[i], min_flow[i],
                    flows_heat[i], flows_cool[i]);
            }

            bool heating_demand = flows_heat[0] > 0.0f || flows_heat[1] > 0.0f;
            bool cooling_demand = flows_cool[0] < 100.0f || flows_cool[1] < 100.0f;

            if (status.has_independent_zone_temps()) {
                if (heating_demand) {
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) &&
                        status.Zone1FlowTemperatureSetPoint != flows_heat[0])
                        set_flow_temp(flows_heat[0], OptimizerZone::ZONE_1);

                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) &&
                        status.Zone2FlowTemperatureSetPoint != flows_heat[1])
                        set_flow_temp(flows_heat[1], OptimizerZone::ZONE_2);

                } else if (cooling_demand) {
                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_1) &&
                        status.Zone1FlowTemperatureSetPoint != flows_cool[0])
                        set_flow_temp(flows_cool[0], OptimizerZone::ZONE_1);

                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_2) &&
                        status.Zone2FlowTemperatureSetPoint != flows_cool[1])
                        set_flow_temp(flows_cool[1], OptimizerZone::ZONE_2);
                }
            } else {
                if (heating_demand) {
                    float final_flow = fmax(flows_heat[0], flows_heat[1]);
                    if (status.Zone1FlowTemperatureSetPoint != final_flow)
                        set_flow_temp(final_flow, OptimizerZone::SINGLE);
                } else if (cooling_demand) {
                    float final_flow = fmin(flows_cool[0], flows_cool[1]);
                    if (status.Zone1FlowTemperatureSetPoint != final_flow)
                        set_flow_temp(final_flow, OptimizerZone::SINGLE);
                }
            }

            this->adaptive_loop_running_ = false;
        }

    } // namespace optimizer
} // namespace esphome

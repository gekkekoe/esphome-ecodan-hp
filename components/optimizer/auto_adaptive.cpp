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
        // Outside temp — locked during/after defrost
        // ─────────────────────────────────────────────────────────────────

        float Optimizer::resolve_outside_temp_() {
            auto &status = this->state_.ecodan_instance->get_status();
            float actual = status.OutsideTemperature;

            const uint32_t LOCK_DURATION_MS = 15 * 60 * 1000;
            bool in_lock_window = (millis() - this->last_defrost_time_) < LOCK_DURATION_MS;

            if (!isnan(this->locked_outside_temp_)) {
                if (status.DefrostActive || in_lock_window) {
                    ESP_LOGD(OPTIMIZER_TAG, "Using locked outside temp: %.1f°C (sensor: %.1f°C)",
                             this->locked_outside_temp_, actual);
                    return this->locked_outside_temp_;
                } else {
                    this->locked_outside_temp_ = NAN;
                }
            }
            return actual;
        }

        // ─────────────────────────────────────────────────────────────────
        // ODIN solver bias — mutex-safe, returns {bias, heating_off}
        // ─────────────────────────────────────────────────────────────────

        Optimizer::SolverBiasResult Optimizer::resolve_solver_bias_(float room_target_temp) {
            SolverBiasResult result{0.0f, false};

            if (this->state_.sw_use_solver == nullptr || !this->state_.sw_use_solver->state)
                return result;

            if (this->odin_mutex_ == NULL ||
                xSemaphoreTake(this->odin_mutex_, pdMS_TO_TICKS(10)) != pdTRUE)
                return result;

            if (this->odin_data_ready_ && !this->odin_schedule_.empty() && !this->odin_energy_.empty()) {
                int current_day  = this->get_current_ecodan_day();
                int current_hour = this->get_current_ecodan_hour();

                if (current_day != this->odin_data_day_) {
                    // Day rolled over — data is stale
                    this->odin_data_ready_ = false;
                    xSemaphoreGive(this->odin_mutex_);
                    apply_solver_soft_stop(false);
                    return result;
                }

                if (current_hour >= 0 && current_hour < 24) {
                    float odin_target = this->odin_schedule_[current_hour];
                    float odin_energy = this->odin_energy_[current_hour];

                    result.heating_off = (odin_energy < 0.05f);

                    if (result.heating_off) {
                        xSemaphoreGive(this->odin_mutex_);
                        apply_solver_soft_stop(true);
                        return result;
                    }

                    if (!std::isnan(odin_target)) {
                        xSemaphoreGive(this->odin_mutex_);
                        apply_solver_soft_stop(false);
                        result.bias = odin_target - room_target_temp;
                        return result;
                    }
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
                                                  float cold_factor,
                                                  float actual_outside_temp,
                                                  float zone_min, float zone_max,
                                                  float error, float error_factor,
                                                  float smart_boost) {
            float actual_return_temp = this->get_return_temp(
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2);

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
                if (error < 0.0f) {
                    // Setpoint reached — fall back to base delta
                    calculated_flow = actual_return_temp + prof.base_min_delta_t;
                    ESP_LOGD(OPTIMIZER_TAG, "Z%d Setpoint reached (error %.1f). Base delta T.", (zone_i + 1), error);
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
                ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING: flow=%.2f°C (boost %.1f)", (zone_i + 1), calculated_flow, pcp_adj);
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

            // Solver bias (also handles soft-stop internally)
            auto [solver_bias, solver_heating_off] = this->resolve_solver_bias_(room_target_temp);

            auto feedback_src = (i == 0)
                ? this->state_.temperature_feedback_source_z1->active_index().value_or(0)
                : this->state_.temperature_feedback_source_z2->active_index().value_or(0);

            ESP_LOGD(OPTIMIZER_TAG,
                "Z%d src=%d room=%.1f target=%.1f flow=%.1f flow_rate=%.1f outside=%.1f bias=%.1f H=%d C=%d solver_bias=%.1f solver_heating_off=%d",
                (i + 1), feedback_src, room_temp, room_target_temp,
                actual_flow_temp, flow_rate, actual_outside_temp, setpoint_bias, is_heating_active, is_cooling_active, solver_bias, solver_heating_off);

            if (solver_heating_off) return;
            
            room_target_temp += setpoint_bias + solver_bias;

            float error           = is_heating_mode ? (room_target_temp - room_temp) : (room_temp - room_target_temp);
            bool  use_linear      = (heating_type_index % 2 != 0);
            float error_positive  = fmax(error, 0.0f);
            float x               = fmin(error_positive / prof.max_error_range, 1.0f);
            float error_factor    = use_linear ? x : x * x * (3.0f - 2.0f * x);
            float smart_boost     = is_heating_mode ? this->calculate_smart_boost(heating_type_index, error) : 1.0f;

            float dynamic_min  = prof.base_min_delta_t + cold_factor * (prof.min_delta_cold_limit - prof.base_min_delta_t);
            float target_delta = dynamic_min + error_factor * smart_boost * (prof.max_delta_t - dynamic_min);

            ESP_LOGD(OPTIMIZER_TAG,
                "Z%d target_delta=%.2f cold_factor=%.2f dyn_min=%.2f error_factor=%.2f boost=%.2f linear=%d",
                (i + 1), target_delta, cold_factor, dynamic_min, error_factor, smart_boost, use_linear);

            if (is_heating_mode && is_heating_active) {
                out_flow_heat = this->calculate_heating_flow_(
                    i, status, prof, cold_factor, actual_outside_temp,
                    zone_min, zone_max, error, error_factor, smart_boost);
            } else if (is_cooling_mode && is_cooling_active) {
                out_flow_cool = this->calculate_cooling_flow_(i, status, target_delta);
            }
        }

        // ─────────────────────────────────────────────────────────────────
        // Main loop entry point
        // ─────────────────────────────────────────────────────────────────

        void Optimizer::run_auto_adaptive_loop() {
            if (!this->state_.auto_adaptive_control_enabled->state) return;

            auto &status = this->state_.ecodan_instance->get_status();

            if (this->is_system_hands_off(status)) {
                ESP_LOGD(OPTIMIZER_TAG, "System is busy (DHW/Defrost/Lockout). Exiting.");
                return;
            }

            if (status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP) {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 1 not in fixed flow mode. Exiting.");
                return;
            }

            if (status.has_2zones() &&
                status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP) {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 2 not in fixed flow mode. Exiting.");
                return;
            }

            if (isnan(this->state_.hp_feed_temp->state) || isnan(status.OutsideTemperature)) {
                ESP_LOGW(OPTIMIZER_TAG, "Sensor data unavailable. Exiting.");
                return;
            }

            float actual_outside_temp = this->resolve_outside_temp_();
            if (isnan(actual_outside_temp)) return;

            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);
            auto prof = this->get_heating_profile_(heating_type_index);

            // Cold factor: quadratic, expanded to 1.5
            const float MILD = 15.0f, COLD = -5.0f;
            float cf_raw   = (MILD - std::clamp(actual_outside_temp, COLD, MILD)) / (MILD - COLD);
            float cold_factor = cf_raw * cf_raw * 1.5f;

            ESP_LOGD(OPTIMIZER_TAG,
                "[*] Auto-adaptive cycle: independent_zone_temps=%d has_cooling=%d cold_factor=%.2f min_delta=%.2f max_delta=%.2f use_solver=%d",
                status.has_independent_zone_temps(), status.has_cooling(), cold_factor, prof.base_min_delta_t, prof.max_delta_t, this->state_.sw_use_solver->state);

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
        }

    } // namespace optimizer
} // namespace esphome

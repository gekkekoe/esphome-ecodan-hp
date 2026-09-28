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

                ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING: flow=%.2f°C, return=%.2f°C", (zone_i + 1), calculated_flow, actual_return_temp);
            }

            // Write-path limits (short-cycle guard, rounding, step limit
            // vs. actual feed, zone clamp) — shared with the ODIN forwarder.
            // The guard uses the (possibly defrost-locked) return temp and is
            // skipped during the defrost recovery ramp — pre-refactor behavior.
            calculated_flow = this->apply_flow_limits(
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2,
                calculated_flow, target_delta,
                (is_defrost_weather && defrost_enabled) ? NAN : actual_return_temp);

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

            // Write-path limits (short-cycle guard, step limit vs. actual
            // feed, cooling clamp) — shared with the ODIN forwarder.
            calculated_flow = this->apply_flow_limits(
                (zone_i == 0) ? OptimizerZone::ZONE_1 : OptimizerZone::ZONE_2,
                calculated_flow, target_delta_t, actual_return_temp);

            return calculated_flow;
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
            // ODIN is in direct control via the MQTT forwarder: it is the
            // sole writer of the flow setpoint. Computing and writing a local
            // value would fight ODIN on every 5-minute cycle. The AA loop
            // keeps running (lockouts, DHW, stats) but leaves zone flow
            // control to ODIN while it holds a valid command. It resumes
            // writing when the forwarder releases (valid_until expired /
            // ODIN unreachable — the designed ASGARD 2b.5 failure case); the
            // live takeover sensor clears on that release.
            if (this->odin_forwarder_takeover_active()) {
                ESP_LOGD(OPTIMIZER_TAG, "Z%d ODIN forwarder active — AA flow setpoint write skipped.", (i + 1));
                return;
            }
            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);

            bool is_heating_mode   = status.is_auto_adaptive_heating(zone);
            bool is_heating_active = is_compressor_active(status) && status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;
            bool is_cooling_mode   = status.has_cooling() && status.is_auto_adaptive_cooling(zone);
            bool is_cooling_active = is_compressor_active(status) && status.Operation == esphome::ecodan::Status::OperationMode::COOL_ON;

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

            float target_delta = dynamic_min + error_factor * smart_boost * (prof.max_delta_t - dynamic_min);
            ESP_LOGD(OPTIMIZER_TAG,
                "Z%d target_delta=%.2f cold_factor=%.2f dyn_min=%.2f eff_range=%.2f error_factor=%.2f boost=%.2f linear=%d",
                (i + 1), target_delta, cold_factor, dynamic_min, effective_error_range, error_factor, smart_boost, use_linear);

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
            auto *override_z1 = this->state_.sw_odin_override_z1;
            auto *override_z2 = this->state_.sw_odin_override_z2;

            // If the user disables Auto Adaptive, we must ensure the hardware relays are released.
            if (!aa_enabled) {
                // While the ODIN forwarder is still holding a valid command,
                // it owns the relay override. Releasing it here would hand
                // the relay back to the virtual thermostat under ODIN's feet.
                if (!this->odin_forwarder_takeover_active()) {
                    bool z1_locked = override_z1 != nullptr && override_z1->state;
                    bool z2_locked = override_z2 != nullptr && override_z2->state;

                    if (z1_locked || z2_locked) {
                        ESP_LOGI(OPTIMIZER_TAG, "AA disabled, but override switches are active. Forcing release.");
                        if (override_z1 != nullptr && override_z1->state) override_z1->turn_off();
                        if (override_z2 != nullptr && override_z2->state) override_z2->turn_off();
                    }
                }
            }
            else {
                // AA enabled, ensure that override is on
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
                "[*] Auto-adaptive cycle: independent_zone_temps=%d has_cooling=%d cold_factor=%.2f min_delta=%.2f max_delta=%.2f multizone_status=%d operation=%d",
                status.has_independent_zone_temps(), status.has_cooling(), cold_factor, prof.base_min_delta_t, prof.max_delta_t,
                status.MultiZoneStatus, static_cast<uint8_t>(status.Operation));

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
#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {

        using namespace esphome::ecodan;

        Optimizer::Optimizer(OptimizerState state) : state_(state) {

            auto update_if_changed = [](float &storage, float new_val, auto callback) {
                if (std::isnan(new_val)) return; 
                if (std::isnan(storage) || std::abs(storage - new_val) > 0.01f) {
                    // store new value first then invoke callback
                    auto previous = storage;
                    storage = new_val;

                    callback(new_val, previous);
                }
            };
            
            if (this->state_.hp_feed_temp != nullptr) {
                this->state_.hp_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_hp_feed_temp_, x, [this](float new_v, float old_v) {
                        this->on_feed_temp_change(new_v, OptimizerZone::SINGLE);
                    });
                });
            } 

            if (this->state_.z1_feed_temp != nullptr) {
                this->state_.z1_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_z1_feed_temp_, x, [this](float new_v, float old_v) {
                        this->on_feed_temp_change(new_v, OptimizerZone::ZONE_1);
                    });
                });
            } 

            if (this->state_.z2_feed_temp != nullptr) {
                this->state_.z2_feed_temp->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_z2_feed_temp_, x, [this](float new_v, float old_v) {
                        this->on_feed_temp_change(new_v, OptimizerZone::ZONE_2);
                    });
                });
            }

            if (this->state_.operation_mode != nullptr) { 
                this->state_.operation_mode->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_operation_mode_, x, [this](float new_v, float old_v) {
                        this->on_operation_mode_change(static_cast<uint8_t>(new_v), static_cast<uint8_t>(old_v));
                    });
                });
            }

            if (this->state_.status_compressor != nullptr) { 
                this->state_.status_compressor->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_compressor_status_, x, [this](float new_v, float old_v) {
                        this->on_compressor_state_change(static_cast<bool>(new_v), static_cast<bool>(old_v));
                    });
                });
            }
            if (this->state_.status_defrost != nullptr) { 
                this->state_.status_defrost->add_on_state_callback([this, update_if_changed](float x) {
                    update_if_changed(this->last_defrost_status_, x, [this](float new_v, float old_v) {
                        this->on_defrost_state_change(static_cast<bool>(new_v), static_cast<bool>(old_v));
                    });
                });
            }
        }

        void Optimizer::process_adaptive_zone_(
            std::size_t i,
            const ecodan::Status &status,
            float cold_factor,
            float min_delta_cold_limit,
            float base_min_delta_t,
            float max_delta_t,
            float max_error_range,
            float actual_outside_temp,
            float zone_max_flow_temp,
            float zone_min_flow_temp,
            float &out_flow_heat,
            float &out_flow_cool)
        {
            auto zone = (i == 0) ? esphome::ecodan::Zone::ZONE_1 : esphome::ecodan::Zone::ZONE_2;
            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);

            bool is_heating_mode = status.is_auto_adaptive_heating(zone);
            bool is_heating_active = status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;
            bool is_cooling_mode = status.has_cooling() && status.is_auto_adaptive_cooling(zone);
            bool is_cooling_active = status.Operation == esphome::ecodan::Status::OperationMode::COOL_ON;

            if (is_heating_active && status.has_2zones())
            {
                auto multizone_status = status.MultiZoneStatus;
                if (i == 0 && (multizone_status == 0 || multizone_status == 3))
                {
                    is_heating_active = false;
                }
                else if (i == 1 && (multizone_status == 0 || multizone_status == 2))
                {
                    is_heating_active = false;
                }

                // for z2 with z1/z2 circulation pump and mixing tank, demand translate into pump being active
                if (status.has_independent_z2() && (status.WaterPump2Active || status.WaterPump3Active))
                    is_heating_active = true;
            }

            float setpoint_bias = this->state_.auto_adaptive_setpoint_bias->state;
            if (isnan(setpoint_bias))
                setpoint_bias = 0.0f;

            float room_temp = (i == 0) ? status.Zone1RoomTemperature : status.Zone2RoomTemperature;
            float room_target_temp = (i == 0) ? status.Zone1SetTemperature : status.Zone2SetTemperature;
            float requested_flow_temp = (i == 0) ? status.Zone1FlowTemperatureSetPoint : status.Zone2FlowTemperatureSetPoint;
            float actual_flow_temp = status.has_independent_z2() ? ((i == 0) ? status.Z1FeedTemperature : status.Z2FeedTemperature) : status.HpFeedTemperature;
            float actual_return_temp = status.has_independent_z2() ? ((i == 0) ? status.Z1ReturnTemperature : status.Z2ReturnTemperature) : status.HpReturnTemperature;

            if (!is_heating_mode && !is_cooling_mode)
                return;

            if (this->state_.temperature_feedback_source->active_index().value_or(0) == 1)
            {
                room_temp = (i == 0) ? this->state_.temperature_feedback_z1->state : this->state_.temperature_feedback_z2->state;
            }
            if (isnan(room_temp) || isnan(room_target_temp) || isnan(requested_flow_temp) || isnan(actual_flow_temp))
                return;

            ESP_LOGD(OPTIMIZER_TAG, "Processing Zone %d: Room=%.1f, Target=%.1f, Actual Feedtemp: %.1f, Return temp: %.1f, Outside: %.1f, Bias: %.1f, heating: %d, cooling: %d",
                     (i + 1), room_temp, room_target_temp, actual_flow_temp, actual_return_temp, actual_outside_temp, setpoint_bias, is_heating_active, is_cooling_active);
 
            room_target_temp += setpoint_bias;

            float error = is_heating_mode ? (room_target_temp - room_temp)
                                          : (room_temp - room_target_temp);

            bool use_linear_error = (heating_type_index % 2 != 0);
            float calculated_flow = NAN;
            float error_positive = fmax(error, 0.0f);
            float error_normalized = error_positive / max_error_range;
            float x = fmin(error_normalized, 1.0f);
            float error_factor = use_linear_error ? x : x * x * (3.0f - 2.0f * x);

            float saturation_limit = 0.4f; // If we are nearing setpoint, scale down cold factor
            float demand_factor = std::clamp(error_positive / saturation_limit, 0.0f, 1.0f);
            float effective_cold_factor = cold_factor * demand_factor;
            
            // apply cold factor
            float dynamic_min_delta_t = base_min_delta_t + (effective_cold_factor * (min_delta_cold_limit - base_min_delta_t));
            float target_delta_t = dynamic_min_delta_t + error_factor * (max_delta_t - dynamic_min_delta_t);

            ESP_LOGD(OPTIMIZER_TAG, "Effective delta T: %.2f, Effective cold factor: %.2f, dynamic min delta T: %.2f, max delta T: %.2f, error factor: %.2f, linear profile: %d", 
                target_delta_t, effective_cold_factor, dynamic_min_delta_t, max_delta_t, error_factor, use_linear_error);

            if (is_heating_mode && is_heating_active)
            {
                if (isnan(actual_return_temp))
                {
                    ESP_LOGE(OPTIMIZER_TAG, "Z%d HEATING (Delta T): FAILED, actual_return_temp is NAN. Reverting to %.2f°C.", (i + 1), zone_min_flow_temp);
                    calculated_flow = zone_min_flow_temp;
                }
                else
                {
                    const float DEFROST_RISK_MIN_TEMP = -2.0f;
                    const float DEFROST_RISK_MAX_TEMP = 3.0f;
                    const uint32_t DEFROST_MEMORY_MS = 10 * 60 * 1000UL;
                    bool defrost_handling_enabled = this->state_.defrost_risk_handling_enabled->state;

                    bool is_defrost_weather = false;
                    if (actual_outside_temp >= DEFROST_RISK_MIN_TEMP && actual_outside_temp <= DEFROST_RISK_MAX_TEMP)
                    {
                        uint32_t last_defrost = this->last_defrost_time_;
                        uint32_t current_ms = millis();
                        if (last_defrost > 0)
                        {
                            if ((current_ms - last_defrost) < DEFROST_MEMORY_MS)
                            {
                                is_defrost_weather = true;
                            }
                            ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Defrost handling enabled: %d, defrost conditions: %d, last_defrost_ts: %d, current_ts: %d", 
                                defrost_handling_enabled, is_defrost_weather, last_defrost, current_ms);
                        }
                    }

                    if (is_defrost_weather && defrost_handling_enabled)
                    {
                        calculated_flow = actual_return_temp + base_min_delta_t;
                        calculated_flow = this->round_nearest(calculated_flow);
                        ESP_LOGW(OPTIMIZER_TAG, "Z%d HEATING (Delta T): Defrost risk detected. Forcing minimum delta T (%.1f°C). Flow set to %.2f°C",
                                 (i + 1), base_min_delta_t, calculated_flow);
                    }
                    else
                    {
                        calculated_flow = actual_return_temp + target_delta_t;
                        calculated_flow = this->round_nearest(calculated_flow);

                        // if there was a boost adjustment, check if it's still needed and clear if needed
                        float short_cycle_prevention_adjustment = this->predictive_short_cycle_total_adjusted_;
                        if (short_cycle_prevention_adjustment > 0.0f)
                        {
                            ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING (boost adjustment): boost: %.1f°C, calcluated_flow: %.2f°C, actual_flow: %.2f°C",
                                     (i + 1), short_cycle_prevention_adjustment, calculated_flow, actual_flow_temp);

                            if ((actual_flow_temp - calculated_flow) >= 1.0f)
                            {
                                calculated_flow += short_cycle_prevention_adjustment;
                            }
                            else
                            {
                                this->predictive_short_cycle_total_adjusted_ = 0.0f;
                                short_cycle_prevention_adjustment = 0;
                            }
                        }
                        ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING (Delta T): calculated_flow: %.2f°C (boost: %.1f)",
                                 (i + 1), calculated_flow, short_cycle_prevention_adjustment);
                    }
                }

                // step down limit to avoid compressor halt (it seems to be triggered when delta actual_flow_temp - calculated_flow >= 2.0)
                calculated_flow = enforce_step_down(actual_flow_temp, calculated_flow);
                calculated_flow = this->clamp_flow_temp(calculated_flow, zone_min_flow_temp, zone_max_flow_temp);
                out_flow_heat = calculated_flow;
            }
            else if (is_cooling_mode && is_cooling_active)
            {
                ESP_LOGD(OPTIMIZER_TAG, "Using 'Delta-T Control' strategy for cooling.");

                if (isnan(actual_return_temp))
                {
                    ESP_LOGE(OPTIMIZER_TAG, "Z%d COOLING (Delta T): FAILED, actual_return_temp is NAN. Reverting to smart start temp.");
                    calculated_flow = this->state_.cooling_smart_start_temp->state;
                }
                else
                {
                    calculated_flow = actual_return_temp - target_delta_t;

                    ESP_LOGD(OPTIMIZER_TAG, "Z%d COOLING (Delta T): calc: %.1f°C (Return %.1f - Scaled delta T %.1f)",
                             (i + 1), calculated_flow, actual_return_temp, target_delta_t);
                }
                calculated_flow = this->clamp_flow_temp(calculated_flow, 
                                                        this->state_.minimum_cooling_flow_temp->state, 
                                                        this->state_.cooling_smart_start_temp->state);
                out_flow_cool = this->round_nearest_half(calculated_flow);
            }
        }


        void Optimizer::run_auto_adaptive_loop()
        {
            if (!this->state_.auto_adaptive_control_enabled->state)
                return;
            auto &status = this->state_.ecodan_instance->get_status();

            if (this->is_system_hands_off(status))
            {
                ESP_LOGD(OPTIMIZER_TAG, "System is busy (DHW, Defrost, or Lockout). Exiting.");
                return;
            }

            if (status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                status.HeatingCoolingMode != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP)
            {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 1 is not in a fixed flow mode. Exiting.");
                return;
            }

            if (status.has_2zones() &&
                (status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::HEAT_FLOW_TEMP &&
                 status.HeatingCoolingModeZone2 != esphome::ecodan::Status::HpMode::COOL_FLOW_TEMP))
            {
                ESP_LOGD(OPTIMIZER_TAG, "Zone 2 is not in a fixed flow mode. Exiting.");
                return;
            }

            if (isnan(this->state_.hp_feed_temp->state) || isnan(status.OutsideTemperature))
            {
                ESP_LOGW(OPTIMIZER_TAG, "Sensor data unavailable. Exiting.");
                return;
            }

            float actual_outside_temp = status.OutsideTemperature;

            if (isnan(actual_outside_temp))
                return;

            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);
            float base_min_delta_t, max_delta_t, max_error_range, min_delta_cold_limit;

            if (heating_type_index <= 1)
            {
                // UFH
                base_min_delta_t = 2.25f;
                min_delta_cold_limit = 4.0f;
                max_delta_t = 6.5f;
                max_error_range = 2.0f;
            }
            else if (heating_type_index <= 3)
            {
                // Hybrid
                base_min_delta_t = 3.0f;
                min_delta_cold_limit = 5.0f;
                max_delta_t = 8.0f;
                max_error_range = 2.0f;
            }
            else
            {
                // Radiator
                base_min_delta_t = 4.0f;
                min_delta_cold_limit = 6.0f;
                max_delta_t = 10.0f;
                max_error_range = 1.5f;
            }

            const float MILD_WEATHER_TEMP = 15.0f;
            const float COLD_WEATHER_TEMP = -5.0f;
            float clamped_outside_temp = std::clamp(actual_outside_temp, COLD_WEATHER_TEMP, MILD_WEATHER_TEMP);
            float linear_cold_factor = (MILD_WEATHER_TEMP - clamped_outside_temp) / (MILD_WEATHER_TEMP - COLD_WEATHER_TEMP);
            float cold_factor = linear_cold_factor * linear_cold_factor;

            ESP_LOGD(OPTIMIZER_TAG, "[*] Starting auto-adaptive cycle, z2 independent: %d, has_cooling: %d, cold factor: %.2f, min delta T: %.2f, max delta T: %.2f", 
                status.has_independent_z2(), status.has_cooling(), cold_factor, max_delta_t, max_delta_t);

            float calculated_flows_heat[2] = {0.0f, 0.0f};
            float calculated_flows_cool[2] = {100.0f, 100.0f};

            auto max_zones = status.has_2zones() ? 2 : 1;

            float max_flow_z1 = this->state_.maximum_heating_flow_temp->state;
            float min_flow_z1 = this->state_.minimum_heating_flow_temp->state;
            if (min_flow_z1 > max_flow_z1)
            {
                ESP_LOGW(OPTIMIZER_TAG, "Zone 1 Min/Max conflict. Forcing Min (%.1f) to Max (%.1f)", min_flow_z1, max_flow_z1);
                min_flow_z1 = max_flow_z1;
            }

            float max_flow_z2 = max_flow_z1;
            float min_flow_z2 = min_flow_z1;

            if (status.has_2zones())
            {
                max_flow_z2 = this->state_.maximum_heating_flow_temp_z2->state;
                min_flow_z2 = this->state_.minimum_heating_flow_temp_z2->state;
                if (min_flow_z2 > max_flow_z2)
                {
                    ESP_LOGW(OPTIMIZER_TAG, "Zone 2 Min/Max conflict. Forcing Min (%.1f) to Max (%.1f)", min_flow_z2, max_flow_z2);
                    min_flow_z2 = max_flow_z2;
                }
            }

            for (std::size_t i = 0; i < max_zones; i++)
            {
                this->process_adaptive_zone_(
                    i,
                    status,
                    cold_factor,
                    min_delta_cold_limit,
                    base_min_delta_t,
                    max_delta_t,
                    max_error_range,
                    actual_outside_temp,
                    (i == 0) ? max_flow_z1 : max_flow_z2,
                    (i == 0) ? min_flow_z1 : min_flow_z2,
                    calculated_flows_heat[i],
                    calculated_flows_cool[i]);
            }

            bool is_heating_demand = calculated_flows_heat[0] > 0.0f || calculated_flows_heat[1] > 0.0f;
            bool is_cooling_demand = calculated_flows_cool[0] < 100.0f || calculated_flows_cool[1] < 100.0f;

            if (status.has_independent_z2())
            {
                if (is_heating_demand)
                {
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) && status.Zone1FlowTemperatureSetPoint != calculated_flows_heat[0])
                    {
                        set_flow_temp(calculated_flows_heat[0], OptimizerZone::ZONE_1);
                    }
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != calculated_flows_heat[1])
                    {
                        set_flow_temp(calculated_flows_heat[1], OptimizerZone::ZONE_2);
                    }
                }
                else if (is_cooling_demand)
                {
                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_1) && status.Zone1FlowTemperatureSetPoint != calculated_flows_cool[0])
                    {
                        set_flow_temp(calculated_flows_cool[0], OptimizerZone::ZONE_1);
                    }
                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != calculated_flows_cool[1])
                    {
                        set_flow_temp(calculated_flows_cool[1], OptimizerZone::ZONE_2);
                    }
                }
            }
            else
            {
                if (is_heating_demand)
                {
                    float final_flow = fmax(calculated_flows_heat[0], calculated_flows_heat[1]);
                    if (status.Zone1FlowTemperatureSetPoint != final_flow)
                    {
                        set_flow_temp(final_flow, OptimizerZone::SINGLE);
                    }
                }
                else if (is_cooling_demand)
                {
                    float final_flow = fmin(calculated_flows_cool[0], calculated_flows_cool[1]);
                    if (status.Zone1FlowTemperatureSetPoint != final_flow)
                    {
                        set_flow_temp(final_flow, OptimizerZone::SINGLE);
                    }
                }
            }
        }

    } // namespace optimizer
} // namespace esphome
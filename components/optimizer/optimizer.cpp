#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {

        using namespace esphome::ecodan;

        Optimizer::Optimizer(OptimizerState state) : state_(state) {}

        bool Optimizer::get_predictive_boost_state()
        {
            return this->predictive_short_cycle_total_adjusted_ > 0.0f;
        }

        void Optimizer::reset_predictive_boost()
        {
            this->predictive_short_cycle_total_adjusted_ = 0.0f;
            this->update_boost_sensor();
        }

        bool Optimizer::is_system_hands_off(const ecodan::Status &status)
        {
            if (status.DefrostActive)
            {
                //ESP_LOGD(OPTIMIZER_TAG, "System is Defrosting");
                return true;
            }
            if (this->state_.status_short_cycle_lockout->state)
            {
                //ESP_LOGD(OPTIMIZER_TAG, "System is in Lockout");
                return true;
            }
            if (status.Operation == esphome::ecodan::Status::OperationMode::DHW_ON ||
                status.Operation == esphome::ecodan::Status::OperationMode::FROST_PROTECT ||
                status.Operation == esphome::ecodan::Status::OperationMode::LEGIONELLA_PREVENTION)
            {
                //ESP_LOGD(OPTIMIZER_TAG, "System is busy (DHW, Frost, etc.)");
                return true;
            }
            return false;
        }
        
        float Optimizer::clamp_flow_temp(float calculated_flow, float min_temp, float max_temp)
        {
            if (calculated_flow > max_temp)
            {
                ESP_LOGW(OPTIMIZER_TAG, "Flow limited to %.1f°C (Zone Max Limit), calculated_flow: %.1f",
                        max_temp, calculated_flow);
                return max_temp;
            }
            if (calculated_flow < min_temp)
            {
                ESP_LOGW(OPTIMIZER_TAG, "Flow limited to %.1f°C (Zone Min Limit), calculated_flow: %.1f",
                        min_temp, calculated_flow);
                return min_temp;
            }
            return calculated_flow;
        }

        void Optimizer::process_adaptive_zone_(
            std::size_t i,
            const ecodan::Status &status,
            float base_min_delta_t,
            float max_delta_t,
            float max_error_range,
            float dynamic_min_delta_t,
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

            if (is_heating_active && status.has_2zones() && status.has_independent_z2())
            {
                auto multizone_status = status.MultiZoneStatus;
                bool multizone_circulation_pump_active = (status.WaterPump2Active || status.WaterPump3Active);

                if (i == 0 && (multizone_status == 0 || multizone_status == 3))
                {
                    is_heating_active = false;
                }
                else if (i == 1 && (multizone_status == 0 || multizone_status == 2))
                {
                    is_heating_active = false;
                }

                // for z2 with z1/z2 circulation pump and mixing tank, demand translate into pump being active
                if (multizone_circulation_pump_active)
                    is_heating_active = true;
            }

            if (!is_heating_mode && !is_cooling_mode)
                return;

            float setpoint_bias = this->state_.auto_adaptive_setpoint_bias->state;
            if (isnan(setpoint_bias))
                setpoint_bias = 0.0f;

            float room_temp = (i == 0) ? status.Zone1RoomTemperature : status.Zone2RoomTemperature;
            float room_target_temp = (i == 0) ? status.Zone1SetTemperature : status.Zone2SetTemperature;
            float requested_flow_temp = (i == 0) ? status.Zone1FlowTemperatureSetPoint : status.Zone2FlowTemperatureSetPoint;
            float actual_flow_temp = status.has_independent_z2() ? ((i == 0) ? status.Z1FeedTemperature : status.Z2FeedTemperature) : this->state_.hp_feed_temp->state;
            float actual_return_temp = status.has_independent_z2() ? ((i == 0) ? status.Z1ReturnTemperature : status.Z2ReturnTemperature) : this->state_.hp_return_temp->state;

            if (this->state_.temperature_feedback_source->active_index().value_or(0) == 1)
            {
                room_temp = (i == 0) ? this->state_.temperature_feedback_z1->state : this->state_.temperature_feedback_z2->state;
            }
            if (isnan(room_temp) || isnan(room_target_temp) || isnan(requested_flow_temp) || isnan(actual_flow_temp))
                return;

            ESP_LOGD(OPTIMIZER_TAG, "Processing Zone %d: Room=%.1f, Target=%.1f, Actual Feedtemp: %.1f, Outside=%.1f, Bias=%.1f, heating: %d, cooling: %d",
                     (i + 1), room_temp, room_target_temp, actual_flow_temp, actual_outside_temp, setpoint_bias, is_heating_active, is_cooling_active);
            room_target_temp += setpoint_bias;

            float error = is_heating_mode ? (room_target_temp - room_temp)
                                          : (room_temp - room_target_temp);

            bool use_linear_error = (heating_type_index % 2 != 0);
            float calculated_flow = NAN;
            float error_positive = fmax(error, 0.0f);
            float error_normalized = error_positive / max_error_range;
            float x = fmin(error_normalized, 1.0f);
            float error_factor = use_linear_error ? x : x * x * (3.0f - 2.0f * x);
            float target_delta_t = dynamic_min_delta_t + error_factor * (max_delta_t - dynamic_min_delta_t);

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
                    const uint32_t DEFROST_MEMORY_S = 1 * 3600UL;

                    bool is_defrost_weather = false;
                    if (actual_outside_temp >= DEFROST_RISK_MIN_TEMP && actual_outside_temp <= DEFROST_RISK_MAX_TEMP)
                    {
                        uint32_t last_defrost = this->last_defrost_time_;
                        time_t current_time = status.timestamp();
                        if (last_defrost > 0 && current_time > 0)
                        {
                            if ((current_time - last_defrost) < DEFROST_MEMORY_S)
                            {
                                is_defrost_weather = true;
                            }
                            ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Defrost handling: %d, last_defrost_ts: %d, current_ts: %d", is_defrost_weather, last_defrost, current_time);
                        }
                    }

                    if (is_defrost_weather)
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

                        // if there was a boost adjustment, check if it's still needed an clear if needed
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
                        ESP_LOGD(OPTIMIZER_TAG, "Z%d HEATING (Delta T): calculated_flow: %.2f°C (Return: %.1f, Scaled  delta T: %.2f, error factor: %.2f, lineair: %d, boost: %.1f)",
                                 (i + 1), calculated_flow, actual_return_temp, target_delta_t, error_factor, use_linear_error, short_cycle_prevention_adjustment);
                    }
                }

                // step down limit to avoid compressor halt (it seems to be triggered when delta actual_flow_temp - calculated_flow >= 2.0)
                const float MAX_FEED_STEP_DOWN = 1.5f;
                if ((actual_flow_temp - calculated_flow) > MAX_FEED_STEP_DOWN)
                {
                    ESP_LOGW(OPTIMIZER_TAG, "Z%d HEATING: Flow limited to %.2f°C to prevent compressor stop! (Delta %.2f°C below actual feed temp), original calc: %.2f°C",
                             (i + 1), actual_flow_temp - MAX_FEED_STEP_DOWN, (actual_flow_temp - calculated_flow), calculated_flow);

                    calculated_flow = actual_flow_temp - MAX_FEED_STEP_DOWN;
                }
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

            if (isnan(this->state_.hp_feed_temp->state) || isnan(this->state_.outside_temp->state))
            {
                ESP_LOGW(OPTIMIZER_TAG, "Sensor data unavailable. Exiting.");
                return;
            }

            float actual_outside_temp = this->state_.outside_temp->state;

            if (isnan(actual_outside_temp))
                return;

            auto heating_type_index = this->state_.heating_system_type->active_index().value_or(0);
            float base_min_delta_t, max_delta_t, max_error_range, min_delta_cold_limit;

            if (heating_type_index <= 1)
            {
                // UFH
                base_min_delta_t = 2.25f;
                min_delta_cold_limit = 4.0f;
                max_delta_t = 6.0f;
                max_error_range = 2.25f;
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
            float dynamic_min_delta_t = base_min_delta_t + (cold_factor * (min_delta_cold_limit - base_min_delta_t));

            ESP_LOGD(OPTIMIZER_TAG, "[*] Starting auto-adaptive cycle, z2 independent: %d, has_cooling: %d, cold factor: %.2f, min delta T: %.2f, max delta T: %.2f", 
                status.has_independent_z2(), status.has_cooling(), cold_factor, dynamic_min_delta_t, max_delta_t);

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
                    base_min_delta_t,
                    max_delta_t,
                    max_error_range,
                    dynamic_min_delta_t,
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
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z1 Heat Flow -> %.1f°C", calculated_flows_heat[0]);
                        this->state_.ecodan_instance->set_flow_target_temperature(calculated_flows_heat[0], esphome::ecodan::Zone::ZONE_1);
                    }
                    if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != calculated_flows_heat[1])
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z2 Heat Flow -> %.1f°C", calculated_flows_heat[1]);
                        this->state_.ecodan_instance->set_flow_target_temperature(calculated_flows_heat[1], esphome::ecodan::Zone::ZONE_2);
                    }
                }
                else if (is_cooling_demand)
                {
                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_1) && status.Zone1FlowTemperatureSetPoint != calculated_flows_cool[0])
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z1 Cool Flow -> %.1f°C", calculated_flows_cool[0]);
                        this->state_.ecodan_instance->set_flow_target_temperature(calculated_flows_cool[0], esphome::ecodan::Zone::ZONE_1);
                    }
                    if (status.is_auto_adaptive_cooling(esphome::ecodan::Zone::ZONE_2) && status.Zone2FlowTemperatureSetPoint != calculated_flows_cool[1])
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Z2 Cool Flow -> %.1f°C", calculated_flows_cool[1]);
                        this->state_.ecodan_instance->set_flow_target_temperature(calculated_flows_cool[1], esphome::ecodan::Zone::ZONE_2);
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
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Dependent Heat Flow -> %.1f°C (max of Z1:%.1f, Z2:%.1f)", final_flow, calculated_flows_heat[0], calculated_flows_heat[1]);
                        this->state_.ecodan_instance->set_flow_target_temperature(final_flow, esphome::ecodan::Zone::ZONE_1);
                    }
                }
                else if (is_cooling_demand)
                {
                    float final_flow = fmin(calculated_flows_cool[0], calculated_flows_cool[1]);
                    if (status.Zone1FlowTemperatureSetPoint != final_flow)
                    {
                        ESP_LOGD(OPTIMIZER_TAG, "CMD: Set Dependent Cool Flow -> %.1f°C (min of Z1:%.1f, Z2:%.1f)", final_flow, calculated_flows_cool[0], calculated_flows_cool[1]);
                        this->state_.ecodan_instance->set_flow_target_temperature(final_flow, esphome::ecodan::Zone::ZONE_1);
                    }
                }
            }
        }

        void Optimizer::predictive_short_cycle_check()
        {
            if (!this->state_.predictive_short_cycle_control_enabled->state)
                return;

            auto &status = this->state_.ecodan_instance->get_status();
            auto start_time = this->predictive_delta_start_time_;

            if (this->is_system_hands_off(status) || !this->state_.status_compressor->state)
            {
                if (status.DefrostActive)
                {
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Defrost detected, updating timestamp.");
                    this->last_defrost_time_ = status.timestamp();
                }

                if (start_time > 0)
                    this->predictive_delta_start_time_ = 0;
                return;
            }

            if (!status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) && !status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2))
            {
                if (start_time > 0)
                    this->predictive_delta_start_time_ = 0;
                return;
            }

            if (status.Operation != esphome::ecodan::Status::OperationMode::HEAT_ON)
            {
                if (start_time > 0)
                    this->predictive_delta_start_time_ = 0;
                return;
            }

            float requested_flow = status.Zone1FlowTemperatureSetPoint;
            if (status.has_2zones())
            {
                requested_flow = fmax(status.Zone1FlowTemperatureSetPoint, status.Zone2FlowTemperatureSetPoint);
            }

            float actual_flow = status.has_2zones()
                                    ? fmax(status.Z1FeedTemperature, status.Z2FeedTemperature)
                                    : this->state_.hp_feed_temp->state;

            if (isnan(requested_flow) || isnan(actual_flow))
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Requested or Actual feed temperature unavailable. Exiting.");
                if (start_time > 0)
                    this->predictive_delta_start_time_ = 0;
                return;
            }

            float predictive_short_cycle_high_delta_threshold = this->state_.predictive_short_cycle_high_delta_threshold->state;
            if (isnan(predictive_short_cycle_high_delta_threshold) || predictive_short_cycle_high_delta_threshold < 0.5f)
            {
                predictive_short_cycle_high_delta_threshold = 1.0f;
            }

            float time_window_setting = this->state_.predictive_short_cycle_high_delta_time_window->state;
            if (isnan(time_window_setting) || time_window_setting < 1.0f || time_window_setting > 5.0f)
            {
                ESP_LOGE(OPTIMIZER_CYCLE_TAG, "Corrupt value for time_window: %.2f. Reverting to 4.0", time_window_setting);
                time_window_setting = 4.0f;
            }
            uint32_t trigger_duration_ms = time_window_setting * 60000UL;

            float delta = actual_flow - requested_flow;
            const float adjustment_factor = 0.5f;

            if (delta >= predictive_short_cycle_high_delta_threshold)
            {
                if (this->predictive_delta_start_time_ == 0)
                {
                    this->predictive_delta_start_time_ = millis();
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "High Delta T detected (%.1f°C). Starting timer.", delta);
                }
                else
                {
                    if ((millis() - this->predictive_delta_start_time_) >= trigger_duration_ms)
                    {
                        ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Short-cycle predicted! Increasing Feed temps to force a longer cycle.");
                        this->predictive_delta_start_time_ = 0;

                        if (this->state_.auto_adaptive_control_enabled->state)
                        {
                            bool was_boosted = false;
                            auto multizone_status = status.MultiZoneStatus;
                            if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) && (!status.has_independent_z2() || (status.has_independent_z2() && (multizone_status == 1 || multizone_status == 2))))
                            {
                                float adjusted_flow_z1 = status.Zone1FlowTemperatureSetPoint + adjustment_factor;
                                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "(Delta T) CMD: Increase Z1 Heat Flow to -> %.1f°C", adjusted_flow_z1);
                                this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow_z1, esphome::ecodan::Zone::ZONE_1);
                                was_boosted = true;
                            }
                            if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) && (!status.has_independent_z2() || (status.has_independent_z2() && (multizone_status == 1 || multizone_status == 3))))
                            {
                                float adjusted_flow_z2 = status.Zone2FlowTemperatureSetPoint + adjustment_factor;
                                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "(Delta T) CMD: Increase Z2 Heat Flow to -> %.1f°C", adjusted_flow_z2);
                                this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow_z2, esphome::ecodan::Zone::ZONE_2);
                                was_boosted = true;
                            }
                            if (was_boosted)
                                this->predictive_short_cycle_total_adjusted_ += adjustment_factor;
                        }
                        else if (this->state_.predictive_short_cycle_control_enabled->state)
                        {
                            this->predictive_short_cycle_total_adjusted_ += adjustment_factor;

                            float adjusted_flow_z1 = status.Zone1FlowTemperatureSetPoint + adjustment_factor;
                            ESP_LOGD(OPTIMIZER_CYCLE_TAG, "CMD: Increase Z1 Heat Flow to -> %.1f°C", adjusted_flow_z1);
                            this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow_z1, esphome::ecodan::Zone::ZONE_1);

                            if (status.has_independent_z2())
                            {
                                float adjusted_flow_z2 = status.Zone2FlowTemperatureSetPoint + adjustment_factor;
                                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "CMD: Increase Z2 Heat Flow to -> %.1f°C", adjusted_flow_z2);
                                this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow_z2, esphome::ecodan::Zone::ZONE_2);
                            }
                        }
                    }
                }
            }
            else
            {
                if (start_time != 0)
                {
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "High Delta T has disappeared. Resetting timer.");
                    this->predictive_delta_start_time_ = 0;
                }
            }
        }

        void Optimizer::restore_svc_state()
        {
            auto flag_before_lockout = this->state_.ecodan_instance->get_svc_state_before_lockout();

            if (flag_before_lockout.has_value())
            {
                auto flag = *flag_before_lockout;

                auto &status = this->state_.ecodan_instance->get_status();
                auto current_flag = status.get_svc_flags();
                auto dhw_mask = esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_DHW;
                flag = (flag & ~dhw_mask) | (current_flag & dhw_mask);

                flag |= esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL;
                this->state_.ecodan_instance->set_controller_mode(flag, true);
            }
            else
            {
                this->state_.ecodan_instance->set_controller_mode(esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL, false);
            }
            *(this->state_.lockout_expiration_timestamp) = 0;
            this->state_.status_short_cycle_lockout->publish_state(false);
        }

        void Optimizer::start_lockout()
        {
            auto &status = this->state_.ecodan_instance->get_status();
            auto flag = status.get_svc_flags();
            time_t current_timestamp = status.timestamp();

            if (current_timestamp == -1)
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Cannot start lockout: Ecodan controller time is not valid!");
                return;
            }

            if (status.ServerControl)
                this->state_.ecodan_instance->set_svc_state_before_lockout(flag);
            else
                this->state_.ecodan_instance->reset_svc_state_before_lockout();

            if (status.is_heating(esphome::ecodan::Zone::ZONE_1))
                flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z1_HEATING;
            else if (status.is_cooling(esphome::ecodan::Zone::ZONE_1))
                flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z1_COOLING;

            if (status.is_heating(esphome::ecodan::Zone::ZONE_2))
                flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z2_HEATING;
            else if (status.is_cooling(esphome::ecodan::Zone::ZONE_2))
                flag |= esphome::ecodan::CONTROLLER_FLAG::PROHIBIT_Z2_COOLING;

            flag |= esphome::ecodan::CONTROLLER_FLAG::SERVER_CONTROL;
            this->state_.ecodan_instance->set_controller_mode(flag, true);

            uint32_t duration_s = atoi(this->state_.lockout_duration->state.c_str()) * 60UL;
            if (duration_s > 0)
            {
                *(this->state_.lockout_expiration_timestamp) = (uint32_t)(current_timestamp + duration_s);
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Lockout active. Expiration timestamp set to: %u", *(this->state_.lockout_expiration_timestamp));
            }
        }

        void Optimizer::check_lockout_expiration()
        {
            uint32_t expiration = *(this->state_.lockout_expiration_timestamp);

            if (expiration == 0)
                return;

            auto &status = this->state_.ecodan_instance->get_status();
            time_t current_time_signed = status.timestamp();

            if (current_time_signed == -1)
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Cannot check lockout: Ecodan time is not valid. Retrying in 30s.");
                return;
            }

            uint32_t current_time = (uint32_t)current_time_signed;

            if (current_time >= expiration)
            {
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Lockout period has expired (Ecodan Time: %u, Expiration: %u). Restoring operations.", current_time, expiration);
                this->restore_svc_state();
            }
            else
            {
                if (!this->state_.status_short_cycle_lockout->state)
                {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Booted during active lockout. Re-enabling lockout sensor. (Ecodan Time: %u, Expiration: %u)", current_time, expiration);
                    this->state_.status_short_cycle_lockout->publish_state(true);
                }
            }
        }

        void Optimizer::on_compressor_stop()
        {
            ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Running compressor stop logic...");
            auto &status = this->state_.ecodan_instance->get_status();

            bool stand_alone_predictive_active = !this->state_.auto_adaptive_control_enabled->state && this->state_.predictive_short_cycle_control_enabled->state;
            float adjustment = this->predictive_short_cycle_total_adjusted_;

            if (stand_alone_predictive_active && adjustment > 0.0f)
            {
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Restoring flow setpoint after predictive boost.");

                float restored_flow_z1 = status.Zone1FlowTemperatureSetPoint - adjustment;
                this->state_.ecodan_instance->set_flow_target_temperature(restored_flow_z1, esphome::ecodan::Zone::ZONE_1);

                if (status.has_independent_z2())
                {
                    float restored_flow_z2 = status.Zone2FlowTemperatureSetPoint - adjustment;
                    this->state_.ecodan_instance->set_flow_target_temperature(restored_flow_z2, esphome::ecodan::Zone::ZONE_2);
                }
                this->predictive_short_cycle_total_adjusted_ = 0.0f;
            }

            if (this->state_.lockout_duration->active_index().value_or(0) == 0)
            {
                this->compressor_start_time_ = 0;
                return;
            }

            if (this->compressor_start_time_ == 0 || this->state_.status_short_cycle_lockout->state || status.DefrostActive)
                return;

            uint32_t min_duration_ms = this->state_.minimum_compressor_on_time->state * 60000UL;
            if (min_duration_ms == 0)
                return;

            if (status.is_heating(esphome::ecodan::Zone::ZONE_1) || status.is_cooling(esphome::ecodan::Zone::ZONE_1) || status.is_heating(esphome::ecodan::Zone::ZONE_2) || status.is_cooling(esphome::ecodan::Zone::ZONE_2))
            {

                uint32_t run_duration_ms = millis() - this->compressor_start_time_;
                if (run_duration_ms < min_duration_ms)
                {
                    this->compressor_start_time_ = 0;
                    this->state_.status_short_cycle_lockout->publish_state(true);
                    this->start_lockout();
                }
            }
        }

        void Optimizer::on_compressor_state_change(bool x, bool x_previous)
        {
            if (x_previous && !x)
            {
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Compressor STOP detected");
                this->on_compressor_stop();
            }
            else if (!x_previous && x)
            {
                auto &status = this->state_.ecodan_instance->get_status();
                if (!status.DefrostActive)
                {
                    ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Compressor START detected");
                    this->compressor_start_time_ = millis();
                }
                if (this->state_.auto_adaptive_control_enabled->state)
                {
                    ESP_LOGD(OPTIMIZER_TAG, "Compressor start: triggering auto-adaptive loop.");
                    this->run_auto_adaptive_loop();
                }
            }
        }

        void Optimizer::update_boost_sensor()
        {
            this->state_.status_predictive_boost_active->publish_state(this->get_predictive_boost_state());
        }

    } // namespace optimizer
} // namespace esphome
#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        bool Optimizer::get_predictive_boost_state()
        {
            return this->predictive_short_cycle_total_adjusted_ > 0.0f;
        }

        void Optimizer::reset_predictive_boost()
        {
            this->predictive_short_cycle_total_adjusted_ = 0.0f;
            this->update_boost_sensor();
        }

        void Optimizer::predictive_short_cycle_check()
        {
            if (!this->state_.predictive_short_cycle_control_enabled->state)
                return;

            auto &status = this->state_.ecodan_instance->get_status();

            static bool was_defrosting = false;
            if (status.DefrostActive) 
            {
                was_defrosting = true;
            }
            else if (was_defrosting) 
            {
                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Defrost cycle finished. Updating last_defrost timestamp");
                this->last_defrost_time_ = millis();
                was_defrosting = false;
            }

            auto start_time = this->predictive_delta_start_time_;

            if (this->is_system_hands_off(status) || !status.CompressorOn)
            {
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
                            if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) && (!status.has_independent_zone_temps() || (status.has_independent_zone_temps() && (multizone_status == 1 || multizone_status == 2))))
                            {
                                float adjusted_flow_z1 = status.Zone1FlowTemperatureSetPoint + adjustment_factor;
                                ESP_LOGD(OPTIMIZER_CYCLE_TAG, "(Delta T) CMD: Increase Z1 Heat Flow to -> %.1f°C", adjusted_flow_z1);
                                this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow_z1, esphome::ecodan::Zone::ZONE_1);
                                was_boosted = true;
                            }
                            if (status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) && (!status.has_independent_zone_temps() || (status.has_independent_zone_temps() && (multizone_status == 1 || multizone_status == 3))))
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

                            if (status.has_independent_zone_temps())
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
            this->state_.lockout_expiration_timestamp = 0;
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

            uint32_t duration_s = atoi(this->state_.lockout_duration->current_option().c_str()) * 60UL;
            if (duration_s > 0)
            {
                this->state_.lockout_expiration_timestamp = (uint32_t)(current_timestamp + duration_s);
                ESP_LOGI(OPTIMIZER_CYCLE_TAG, "Lockout active. Expiration timestamp set to: %u", this->state_.lockout_expiration_timestamp);
            }
        }

        void Optimizer::check_lockout_expiration()
        {
            uint32_t expiration = this->state_.lockout_expiration_timestamp;

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

        void Optimizer::update_boost_sensor()
        {
            this->state_.status_predictive_boost_active->publish_state(this->get_predictive_boost_state());
        }

    } // namespace optimizer
} // namespace esphome

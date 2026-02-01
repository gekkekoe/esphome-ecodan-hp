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
            return (!isnan(this->pcp_old_z1_setpoint_) && this->pcp_old_z1_setpoint_ > 0.0f) 
                || (!isnan(this->pcp_old_z2_setpoint_) && this->pcp_old_z2_setpoint_ > 0.0f);
        }

        void Optimizer::reset_predictive_boost()
        {
            this->pcp_old_z1_setpoint_ = NAN;
            this->pcp_old_z2_setpoint_ = NAN;
            this->update_boost_sensor();
        }

        void Optimizer::predictive_short_cycle_check_for_zone_(const ecodan::Status &status, OptimizerZone zone) {

            auto &mapped_delta_start_time_ = (zone == OptimizerZone::ZONE_2) ? this->predictive_delta_start_time_z2_ : this->predictive_delta_start_time_z1_;
            auto &mapped_pcp_old_flow_setpoint_ = (zone == OptimizerZone::ZONE_2) ? this->pcp_old_z2_setpoint_ : this->pcp_old_z1_setpoint_;
            auto &mapped_pcp_adjustment_ = (zone == OptimizerZone::ZONE_2) ? this->pcp_adjustment_z2_ : this->pcp_adjustment_z1_;
            auto ecodan_zone = (zone == OptimizerZone::ZONE_2) ? esphome::ecodan::Zone::ZONE_2 : esphome::ecodan::Zone::ZONE_1;

            auto start_time = mapped_delta_start_time_;
            if (this->is_system_hands_off(status) || !status.CompressorOn)
            {
                if (start_time > 0)
                    mapped_delta_start_time_ = 0;
                return;
            }

            float requested_flow = this->get_flow_setpoint(zone);
            float actual_flow = this->get_feed_temp(zone);
            //ESP_LOGD(OPTIMIZER_CYCLE_TAG, "PCP: Zone: %d: actual flow (%.1f°C), requested flow (%.1f°C)", static_cast<uint8_t>(zone), actual_flow, requested_flow);

            if (isnan(requested_flow) || isnan(actual_flow))
            {
                ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Requested or Actual feed temperature unavailable. Exiting.");
                if (start_time > 0)
                    mapped_delta_start_time_ = 0;
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
                if (mapped_delta_start_time_ == 0)
                {
                    mapped_delta_start_time_ = millis();
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Zone: %d: High Delta T detected (%.1f°C). Starting timer.", static_cast<uint8_t>(zone), delta);
                }
                else if (((millis() - mapped_delta_start_time_) >= trigger_duration_ms))
                {
                    ESP_LOGW(OPTIMIZER_CYCLE_TAG, "Short-cycle predicted! Increasing Feed temps to force a longer cycle. Current saved Z%d setpoin: %.1f°", static_cast<uint8_t>(zone), mapped_pcp_old_flow_setpoint_);
                    mapped_delta_start_time_ = 0;

                    if (isnan(mapped_pcp_old_flow_setpoint_)) {
                        mapped_pcp_old_flow_setpoint_ = this->get_flow_setpoint(zone);
                        ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Updating Z%d Saved setpont to:  %.1f", static_cast<uint8_t>(zone), mapped_pcp_old_flow_setpoint_);
                    }
                    mapped_pcp_adjustment_ += adjustment_factor;

                    auto limits = this->get_flow_limits(zone);
                    float adjusted_flow = this->get_flow_setpoint(zone) + mapped_pcp_adjustment_;
                    adjusted_flow = this->clamp_flow_temp(adjusted_flow, limits.min, limits.max);
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "(Delta T) CMD: Increase Z%d Heat Flow to -> %.1f°C", static_cast<uint8_t>(zone), adjusted_flow);
                    this->state_.ecodan_instance->set_flow_target_temperature(adjusted_flow, ecodan_zone);
                }
            }
            else
            {
                if (start_time != 0)
                {
                    ESP_LOGD(OPTIMIZER_CYCLE_TAG, "Zone: %d: High Delta T has disappeared. Resetting timer.", static_cast<uint8_t>(zone));
                    mapped_delta_start_time_ = 0;
                }
            }

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

            auto multizone_status = status.MultiZoneStatus;
            bool is_heating_z1 = status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_1) 
                || status.is_heating(esphome::ecodan::Zone::ZONE_1)
                || (status.has_2zones() && (multizone_status == 1 || multizone_status == 2));

            bool is_heating_z2 = status.is_auto_adaptive_heating(esphome::ecodan::Zone::ZONE_2) 
                || status.is_heating(esphome::ecodan::Zone::ZONE_2)
                || (status.has_2zones() && (multizone_status == 1 || multizone_status == 3));

            if (is_heating_z1)
                this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_1);
            if (status.has_2zones() && is_heating_z2)
                this->predictive_short_cycle_check_for_zone_(status, OptimizerZone::ZONE_2);
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

            auto val = esphome::parse_number<int>(this->state_.lockout_duration->current_option());
            uint32_t duration_s = (val.has_value() ? *val : 0) * 60UL;
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
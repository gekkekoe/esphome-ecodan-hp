#include "optimizer.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;

        bool Optimizer::is_post_dhw_window(const ecodan::Status &status) {
            time_t current_timestamp = status.timestamp();
            return (current_timestamp > 0 
                                    && this->dhw_post_run_expiration_ > 0 
                                    && current_timestamp < this->dhw_post_run_expiration_);
        }

        bool Optimizer::is_system_hands_off(const ecodan::Status &status)
        {
            if (status.DefrostActive) return true;

            if (this->state_.status_short_cycle_lockout != nullptr && 
                this->state_.status_short_cycle_lockout->state) return true;
            
            if (status.Operation == esphome::ecodan::Status::OperationMode::DHW_ON ||
                status.Operation == esphome::ecodan::Status::OperationMode::LEGIONELLA_PREVENTION)
            {
                //ESP_LOGD(OPTIMIZER_TAG, "System is busy (DHW, etc.)");
                return true;
            }
            return false;
        }

        bool Optimizer::is_dhw_active(const ecodan::Status &status) {
            if (status.Operation == esphome::ecodan::Status::OperationMode::DHW_ON ||
                status.Operation == esphome::ecodan::Status::OperationMode::LEGIONELLA_PREVENTION)
            {
                return true;
            }
            return false;
        }

        bool Optimizer::is_compressor_active(const ecodan::Status &status) {
            // both flags needs to be true to be active
            return status.CompressorOn && status.CompressorFrequency > 0;
        }

        float Optimizer::clamp_flow_temp(float calculated_flow, float min_temp, float max_temp)
        {
            // Only treat it as a real clamp (worth logging) when the overshoot exceeds
            // float-noise at the 0.1°C control resolution; otherwise clamp silently.
            const float CLAMP_LOG_EPS = 0.05f;
            if (calculated_flow > max_temp)
            {
                if (calculated_flow > max_temp + CLAMP_LOG_EPS)
                    ESP_LOGW(OPTIMIZER_TAG, "Flow limited to %.1f°C (Zone Max Limit), calculated_flow: %.1f",
                            max_temp, calculated_flow);
                return max_temp;
            }
            if (calculated_flow < min_temp)
            {
                if (calculated_flow < min_temp - CLAMP_LOG_EPS)
                    ESP_LOGW(OPTIMIZER_TAG, "Flow limited to %.1f°C (Zone Min Limit), calculated_flow: %.1f",
                            min_temp, calculated_flow);
                return min_temp;
            }
            return calculated_flow;
        }


        float Optimizer::get_feed_temp(OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();
            if (status.has_independent_zone_temps())
                return (zone == OptimizerZone::ZONE_2) ? status.Z2FeedTemperature : status.Z1FeedTemperature;        
            return status.HpFeedTemperature;
        }

        float Optimizer::get_return_temp(OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();
            if (status.has_independent_zone_temps())
                return (zone == OptimizerZone::ZONE_2) ? status.Z2ReturnTemperature : status.Z1ReturnTemperature;        
            return status.HpReturnTemperature;
        }


        float Optimizer::get_flow_setpoint(OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();
            if (status.has_independent_zone_temps())
                return (zone == OptimizerZone::ZONE_2) ? status.Zone2FlowTemperatureSetPoint : status.Zone1FlowTemperatureSetPoint;        
            return status.Zone1FlowTemperatureSetPoint;
        }

        float Optimizer::get_room_current_temp(OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();

            auto *src_cur = (zone == OptimizerZone::ZONE_2)
                ? this->state_.temperature_feedback_source_z2
                : this->state_.temperature_feedback_source_z1;
            auto temp_feedback_source = (src_cur != nullptr && src_cur->has_state())
                ? src_cur->active_index().value_or(0) : 0;

            auto current_temp = NAN;

            if (zone == OptimizerZone::ZONE_1) {
                if (temp_feedback_source == 2 && this->state_.asgard_vt_z1 != nullptr) {
                    current_temp = this->state_.asgard_vt_z1->current_temperature;
                } else if (temp_feedback_source == 1 && this->state_.temperature_feedback_z1 != nullptr) {
                    current_temp = this->state_.temperature_feedback_z1->state;
                }
            } else {
                if (temp_feedback_source == 2 && this->state_.asgard_vt_z2 != nullptr) {
                    current_temp = this->state_.asgard_vt_z2->current_temperature;
                } else if (temp_feedback_source == 1 && this->state_.temperature_feedback_z2 != nullptr) {
                    current_temp = this->state_.temperature_feedback_z2->state;
                }
            }

            if (isnan(current_temp))
                current_temp = (zone == OptimizerZone::ZONE_1) ? status.Zone1RoomTemperature : status.Zone2RoomTemperature;

            return current_temp;
        }

        float Optimizer::get_room_target_temp(OptimizerZone zone) {
            auto &status = this->state_.ecodan_instance->get_status();
            
            auto *src_tgt = (zone == OptimizerZone::ZONE_2)
                ? this->state_.temperature_feedback_source_z2
                : this->state_.temperature_feedback_source_z1;
            auto temp_feedback_source = (src_tgt != nullptr && src_tgt->has_state())
                ? src_tgt->active_index().value_or(0) : 0;

            auto target_temp = NAN;

            if (zone == OptimizerZone::ZONE_1) {
                if (temp_feedback_source == 2 && this->state_.asgard_vt_z1 != nullptr) {
                    target_temp = this->state_.asgard_vt_z1->target_temperature;
                } 
            } else {
                if (temp_feedback_source == 2 && this->state_.asgard_vt_z2 != nullptr) {
                    target_temp = this->state_.asgard_vt_z2->target_temperature;
                } 
            }

            if (isnan(target_temp))
                target_temp = (zone == OptimizerZone::ZONE_1) ? status.Zone1SetTemperature : status.Zone2SetTemperature;

            return target_temp;
        }

        FlowLimits Optimizer::get_flow_limits(OptimizerZone zone) {
            float min_flow, max_flow;
            if (zone == OptimizerZone::ZONE_2) {
                max_flow = this->state_.maximum_heating_flow_temp_z2->state;
                min_flow = this->state_.minimum_heating_flow_temp_z2->state;
                if (min_flow > max_flow)
                {
                    min_flow = max_flow;
                }
            }
            else {
                max_flow = this->state_.maximum_heating_flow_temp->state;
                min_flow = this->state_.minimum_heating_flow_temp->state;
                if (min_flow > max_flow)
                {
                    min_flow = max_flow;
                }   
            }

            return {min_flow, max_flow};
        }

        FlowLimits Optimizer::get_cool_flow_limits(OptimizerZone zone) {
            float min_flow = 18.0f;
            if (zone == OptimizerZone::ZONE_2) {
                if (this->state_.minimum_cooling_flow_temp_z2 != nullptr)
                    min_flow = this->state_.minimum_cooling_flow_temp_z2->state;
            } else {
                if (this->state_.minimum_cooling_flow_temp_z1 != nullptr)
                    min_flow = this->state_.minimum_cooling_flow_temp_z1->state;
            }

            float max_flow = 30.0f;
            if (min_flow > max_flow)
            {
                max_flow = min_flow;
            }

            return {min_flow, max_flow};
        }

        float Optimizer::enforce_step_limit(const ecodan::Status &status, float actual_flow_temp, float calculated_flow, bool is_cooling_mode) 
        {
            const float MAX_FEED_STEP_CHANGE = 1.0f;
            const float MAX_FEED_STEP_ADJUSTMENT = 0.5f;

            if (is_cooling_mode) 
            {
                if ((calculated_flow - actual_flow_temp) > MAX_FEED_STEP_CHANGE)
                {
                    float adjusted_target = actual_flow_temp + MAX_FEED_STEP_ADJUSTMENT;
                    
                    ESP_LOGW(OPTIMIZER_TAG, "Cooling flow adjust: %.2f°C to prevent compressor stop! (setpoint: %.2f°C is %.2f°C above actual feed temp)",
                            adjusted_target, calculated_flow, (calculated_flow - actual_flow_temp));

                    return adjusted_target;
                }
            }
            else 
            {
                if ((actual_flow_temp - calculated_flow) > MAX_FEED_STEP_CHANGE)
                {
                    float adjusted_target = actual_flow_temp - MAX_FEED_STEP_ADJUSTMENT;
                    
                    ESP_LOGW(OPTIMIZER_TAG, "Heating/DHW flow adjust: %.2f°C to prevent compressor stop! (setpoint: %.2f°C is %.2f°C below actual feed temp)",
                            adjusted_target, calculated_flow, (actual_flow_temp - calculated_flow));

                    return adjusted_target;
                }
            }

            return calculated_flow;
        }

        float Optimizer::apply_flow_limits(OptimizerZone zone, float calculated, float target_delta_t, float guard_return_temp)
        {
            auto &status = this->state_.ecodan_instance->get_status();
            bool cooling = this->is_cooling_mode(status, zone);
            float feed = this->get_feed_temp(zone);
            float ret = guard_return_temp;

            bool guard_enabled = status.has_independent_zone_temps() && !isnan(feed) && !isnan(ret);

            if (cooling)
            {
                if (guard_enabled && (ret - feed) > target_delta_t && calculated > feed)
                {
                    ESP_LOGI(OPTIMIZER_TAG,
                        "[Buffer] Z%d Short-cycle guard (Cooling): ΔT actual %.2f > target %.2f — held %.2f → %.2f",
                        (int)zone, ret - feed, target_delta_t, calculated, feed);
                    calculated = feed;
                }

                // Step limit only protects a running compressor.
                if (this->is_compressor_active(status))
                    calculated = this->enforce_step_limit(status, feed, calculated, true);

                float min_cool_target = 18.0f;
                if (zone == OptimizerZone::ZONE_1)
                {
                    if (this->state_.minimum_cooling_flow_temp_z1 != nullptr)
                        min_cool_target = this->state_.minimum_cooling_flow_temp_z1->state;
                }
                else if (zone == OptimizerZone::ZONE_2)
                {
                    min_cool_target = this->state_.minimum_cooling_flow_temp_z2->state;
                }

                // smart_start caps the flow on startup (water still warm) to avoid a slam-start.
                bool cooling_active = status.is_cooling_active();
                if (!cooling_active)
                {
                    float smart_start = this->state_.cooling_smart_start_temp->state;
                    if (min_cool_target > smart_start)
                    {
                        ESP_LOGW(OPTIMIZER_TAG,
                            "Z%d COOLING: min_cool_target (%.1f) > smart_start (%.1f) — clamping to min_cool_target.",
                            (int)zone, min_cool_target, smart_start);
                        smart_start = min_cool_target;
                    }
                    calculated = this->clamp_flow_temp(calculated, min_cool_target, smart_start);
                }
                else
                {
                    calculated = std::max(calculated, min_cool_target);
                }
            }
            else
            {
                if (guard_enabled && (feed - ret) > target_delta_t && calculated < feed)
                {
                    ESP_LOGI(OPTIMIZER_TAG,
                        "[Buffer] Z%d Short-cycle guard (Heating): ΔT actual %.2f > target %.2f — held %.2f → %.2f",
                        (int)zone, feed - ret, target_delta_t, calculated, feed);
                    calculated = feed;
                }

                calculated = this->round_nearest(calculated);

                auto limits = this->get_flow_limits(zone);
                // Stepdown: after a DHW run, always step down during the
                // post-DHW window (5 min) — clamp first, so the setpoint
                // follows the hot feed down in 0.5° steps. Otherwise step
                // down whenever the compressor is running. Compressor off
                // and no window: only the zone clamp applies.
                if (this->is_post_dhw_window(status))
                {
                    calculated = this->clamp_flow_temp(calculated, limits.min, limits.max);
                    calculated = this->enforce_step_limit(status, feed, calculated, false);
                }
                else if (this->is_compressor_active(status))
                {
                    calculated = this->enforce_step_limit(status, feed, calculated, false);
                    calculated = this->clamp_flow_temp(calculated, limits.min, limits.max);
                }
                else
                {
                    calculated = this->clamp_flow_temp(calculated, limits.min, limits.max);
                }
            }

            return calculated;
        }

        float Optimizer::limit_external_flow(OptimizerZone zone, float requested)
        {
            auto &status = this->state_.ecodan_instance->get_status();
            if (isnan(requested))
                return requested;
            if (this->is_dhw_active(status))
                return requested;

            bool cooling = this->is_cooling_mode(status, zone);
            float ret = this->get_return_temp(zone);
            
            float target_delta_t = isnan(ret) ? 0.0f : (cooling ? (ret - requested) : (requested - ret));

            float limited = this->apply_flow_limits(zone, requested, target_delta_t, ret);
            if (limited != requested)
            {
                ESP_LOGD(OPTIMIZER_TAG,
                    "Z%d External flow limited: %.2f → %.2f (feed %.2f, %s)",
                    (int)zone, requested, limited, this->get_feed_temp(zone), cooling ? "cooling" : "heating");
            }
            return limited;
        }
    } // namespace optimizer
} // namespace esphome
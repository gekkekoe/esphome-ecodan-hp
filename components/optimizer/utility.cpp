#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

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

        bool Optimizer::is_dhw_active(const ecodan::Status &status) {
            if (status.Operation == esphome::ecodan::Status::OperationMode::DHW_ON ||
                status.Operation == esphome::ecodan::Status::OperationMode::LEGIONELLA_PREVENTION)
            {
                return true;
            }
            return false;
        }
        
        bool Optimizer::is_heating_active(const ecodan::Status &status) {
            return status.Operation == esphome::ecodan::Status::OperationMode::HEAT_ON;
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
            auto temp_feedback_source = this->state_.temperature_feedback_source->active_index().value_or(0);

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
            auto temp_feedback_source = this->state_.temperature_feedback_source->active_index().value_or(0);

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

    } // namespace optimizer
} // namespace esphome
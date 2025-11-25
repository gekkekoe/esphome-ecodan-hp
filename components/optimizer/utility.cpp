#include "optimizer.h"
#include "esphome/components/ecodan/ecodan.h"

using std::isnan;

namespace esphome
{
    namespace optimizer
    {
        using namespace esphome::ecodan;
        
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

    } // namespace optimizer
} // namespace esphome
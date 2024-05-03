#pragma once
#include "Arduino.h"
#include <mutex>
#include <string>

namespace esphome {
namespace ecodan 
{    

    struct Status
    {
        bool Initialized = false;

        bool DefrostActive;
        bool DhwForcedActive;        
        bool BoosterActive;
        bool ImmersionActive;
        uint8_t HeatSource;
        bool In1ThermostatRequest;
        bool In6ThermostatRequest;
        uint8_t OutputPower;
        uint8_t Controller;

        float Zone1SetTemperature;
        float Zone1FlowTemperatureSetPoint;
        float Zone1RoomTemperature;
        float Zone2SetTemperature;
        float Zone2FlowTemperatureSetPoint;
        float Zone2RoomTemperature;
        float LegionellaPreventionSetPoint;
        float DhwTemperatureDrop;
        uint8_t MaximumFlowTemperature;
        uint8_t MinimumFlowTemperature;
        float OutsideTemperature;
        float HpFeedTemperature;
        float HpReturnTemperature;
        float HpRefrigerantLiquidTemperature;
        float DhwTemperature;
        float BoilerFlowTemperature;
        float BoilerReturnTemperature;
        uint8_t FlowRate;
        float DhwFlowTemperatureSetPoint;
        float RadiatorFlowTemperatureSetPoint;

        float Runtime;
        bool WaterPumpActive;
        bool WaterPump2Active;
        bool ThreeWayValveActive;
        bool ThreeWayValve2Active;

        enum class PowerMode : uint8_t
        {
            STANDBY = 0,
            ON = 1
        };

        enum class OperationMode : uint8_t
        {
            OFF = 0,
            DHW_ON = 1,
            SH_ON = 2, // Heating
            COOL_ON = 3, // Cooling
            FROST_PROTECT = 5,
            LEGIONELLA_PREVENTION = 6
        };

        enum class DhwMode : uint8_t
        {
            NORMAL = 0,
            ECO = 1
        };

        enum class HpMode : uint8_t
        {
            HEAT_ROOM_TEMP = 0,
            HEAT_FLOW_TEMP = 1,
            HEAT_COMPENSATION_CURVE = 2,
            COOL_ROOM_TEMP = 3,
            COOL_FLOW_TEMP = 4
        };

        // Modes
        PowerMode Power;
        OperationMode Operation;
        bool HolidayMode;
        DhwMode HotWaterMode;
        HpMode HeatingCoolingMode;

        // prohibit flags
        bool ProhibitDhw;
        bool ProhibitHeatingZ1;
        bool ProhibitCoolingZ1;
        bool ProhibitHeatingZ2;
        bool ProhibitCoolingZ2;

        // Efficiency
        uint8_t CompressorFrequency;
        float EnergyConsumedHeating;
        float EnergyConsumedCooling;
        float EnergyDeliveredHeating;
        float EnergyDeliveredCooling;
        float EnergyConsumedDhw;
        float EnergyDeliveredDhw;

        // HomeAssistant is a bit restrictive about what is let's us specify
        // as the mode/action of a climate integration.
        std::string ha_status_as_string()
        {
            switch (Power)
            {
                case PowerMode::ON:
                    switch (HeatingCoolingMode)
                    {
                      case HpMode::HEAT_ROOM_TEMP:
                          [[fallthrough]]
                      case HpMode::HEAT_FLOW_TEMP:
                          [[fallthrough]]
                      case HpMode::HEAT_COMPENSATION_CURVE:
                          return std::string("heat");
                      case HpMode::COOL_ROOM_TEMP:
                          [[fallthrough]]
                      case HpMode::COOL_FLOW_TEMP:
                          return std::string("cool");
                    }
                    [[fallthrough]]
                default:
                    return std::string("off");
            }
        }

        std::string ha_action_as_string()
        {
            switch (Operation)
            {
                case OperationMode::SH_ON:
                    [[fallthrough]]
                case OperationMode::FROST_PROTECT:
                    return std::string("heating");
                case OperationMode::COOL_ON:
                    return std::string("cooling");
                case OperationMode::OFF:
                    [[fallthrough]]
                case OperationMode::DHW_ON:
                    [[fallthrough]]
                default:
                    return std::string("idle");
            }
        }

        std::string power_as_string()
        {
            switch (Power)
            {
                case PowerMode::ON:
                    return std::string("On");
                case PowerMode::STANDBY:
                    return std::string("Standby");
                default:
                    return std::string("Unknown");
            }
        }

        std::string operation_as_string()
        {
            switch (Operation)
            {
                case OperationMode::OFF:
                    return std::string("Off");
                case OperationMode::DHW_ON:
                    return std::string("Heating Water");
                case OperationMode::SH_ON:
                    return std::string("Space Heating");
                case OperationMode::COOL_ON:
                    return std::string("Space Cooling");
                case OperationMode::FROST_PROTECT:
                    return std::string("Frost Protection");
                case OperationMode::LEGIONELLA_PREVENTION:
                    return std::string("Legionella Prevention");
                default:
                    return std::string("Unknown");
            }
        }

        std::string dhw_status_as_string()
        {
            switch (Operation)
            {
                case OperationMode::DHW_ON:
                case OperationMode::LEGIONELLA_PREVENTION:
                    break;
                default:
                    return std::string("Off");
            }

            switch (HotWaterMode)
            {
                case DhwMode::NORMAL:
                    return std::string("Normal");
                case DhwMode::ECO:
                    return std::string("Eco");
                default:
                    return std::string("Unknown");
            }
        }

        std::string hp_status_as_string()
        {
            switch (HeatingCoolingMode)
            {
                case HpMode::HEAT_ROOM_TEMP:
                    return std::string("Heat Target Temperature");
                case HpMode::HEAT_FLOW_TEMP:
                    return std::string("Heat Flow Temperature");
                case HpMode::HEAT_COMPENSATION_CURVE:
                    return std::string("Heat Compensation Curve");
                case HpMode::COOL_ROOM_TEMP:
                    return std::string("Cool Target Temperature");
                case HpMode::COOL_FLOW_TEMP:
                    return std::string("Cool Flow Temperature");
                default:
                    return std::string("Unknown");
            }
        }

        void set_power_mode(uint8_t mode)
        {
            Power = static_cast<PowerMode>(mode);
        }

        void set_operation_mode(uint8_t mode)
        {
            Operation = static_cast<OperationMode>(mode);
        }

        void set_dhw_mode(uint8_t mode)
        {
            HotWaterMode = static_cast<DhwMode>(mode);
        }

        void set_heating_cooling_mode(uint8_t mode)
        {
            HeatingCoolingMode = static_cast<HpMode>(mode);
        }

        void lock()
        {
            lock_.lock();
        }

        void unlock()
        {
            lock_.unlock();
        }

      private:
        std::mutex lock_;
    };

} // namespace ecodan
} // namespace esphome
#pragma once
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
        bool Booster2Active;
        bool ImmersionActive;
        uint8_t HeatSource;
        bool In1ThermostatRequest;
        bool In5ThermostatRequest;
        bool In6ThermostatRequest;
        uint8_t OutputPower;
        float ComputedOutputPower;
        uint8_t Controller;

        float Zone1SetTemperature{NAN};
        float Zone1FlowTemperatureSetPoint{NAN};
        float Zone1RoomTemperature{NAN};
        float Zone2SetTemperature{NAN};
        float Zone2FlowTemperatureSetPoint{NAN};
        float Zone2RoomTemperature{NAN};
        float LegionellaPreventionSetPoint;
        float DhwTemperatureDrop;
        uint8_t MaximumFlowTemperature;
        uint8_t MinimumFlowTemperature;
        float OutsideTemperature;
        float HpFeedTemperature {0.0f};
        float HpReturnTemperature {0.0f};
        float Z1FeedTemperature;
        float Z1ReturnTemperature;
        float Z2FeedTemperature;
        float Z2ReturnTemperature;
        float MixingTankTemperature;
        float HpRefrigerantLiquidTemperature;
        float HpRefrigerantCondensingTemperature;
        float DhwTemperature{NAN};
        float DhwSecondaryTemperature;
        float BoilerFlowTemperature;
        float BoilerReturnTemperature;
        uint8_t FlowRate {0};
        float DhwFlowTemperatureSetPoint{NAN};
        float RadiatorFlowTemperatureSetPoint;

        float Runtime;
        bool WaterPumpActive;
        bool WaterPump2Active;
        bool WaterPump3Active;
        bool ThreeWayValveActive;
        bool ThreeWayValve2Active;
        uint8_t MixingValveStatus;
        uint8_t MixingValveStep;
        uint8_t MultiZoneStatus;

        // error codes
        uint8_t RefrigerantErrorCode;
        uint16_t FaultCodeLetters;
        uint16_t FaultCodeNumeric;

        // Dip switches
        uint8_t DipSwitch1{0};
        uint8_t DipSwitch2{0};
        uint8_t DipSwitch3{0};
        uint8_t DipSwitch4{0};
        uint8_t DipSwitch5{0};
        uint8_t DipSwitch6{0};

        // datime of controller
        struct tm ControllerDateTime = {0, 0, 0, 0, 0, 0, 0, 0, 0};

        enum class PowerMode : uint8_t
        {
            STANDBY = 0,
            ON = 1
        };

        enum class OperationMode : uint8_t
        {
            UNAVAILABLE = 255,
            OFF = 0,
            DHW_ON = 1,
            HEAT_ON = 2, // Heating
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
            OFF = 255,
            HEAT_ROOM_TEMP = 0,
            HEAT_FLOW_TEMP = 1,
            HEAT_COMPENSATION_CURVE = 2,
            COOL_ROOM_TEMP = 3,
            COOL_FLOW_TEMP = 4
        };

        enum class MRC_FLAG : uint8_t
        {
            DISABLED = 0x0,
            FUNCTION = 0x04, 
            TEMPERATURE = 0x10, 
            RUNNING_MODE = 0x20, 
            SYSTEM_ON_OFF = 0x40
        };

        enum class REQUEST_CODE : int16_t 
        {
            COMPRESSOR_STARTS = 3,
            TH4_DISCHARGE_TEMP = 4,
            TH3_LIQUID_PIPE1_TEMP = 5,
            TH6_2_PHASE_PIPE_TEMP = 7,
            TH32_SUCTION_PIPE_TEMP = 8,
            TH8_HEAT_SINK_TEMP = 10,
            //TH33_SURFACE_TEMP = 11,
            DISCHARGE_SUPERHEAT = 12,
            SUB_COOL = 13,
            FAN_SPEED = 19,
            NONE = 0x7fff
        };

        // Modes
        PowerMode Power;
        OperationMode Operation = OperationMode::UNAVAILABLE;
        bool HolidayMode;
        DhwMode HotWaterMode;
        HpMode HeatingCoolingMode = HpMode::OFF;
        HpMode HeatingCoolingModeZone2 = HpMode::OFF;

        MRC_FLAG MRCFlag = MRC_FLAG::DISABLED;

        // prohibit flags
        bool ServerControl;
        bool ProhibitDhw;
        bool ProhibitHeatingZ1;
        bool ProhibitCoolingZ1;
        bool ProhibitHeatingZ2;
        bool ProhibitCoolingZ2;

        // Efficiency
        uint8_t CompressorFrequency {0};
        float EnergyConsumedHeating;
        float EnergyConsumedCooling;
        float EnergyDeliveredHeating;
        float EnergyDeliveredCooling;
        float EnergyConsumedDhw;
        float EnergyDeliveredDhw;
        float EnergyConsumedIncreasing{0};

        // Service codes
        uint16_t RcCompressorStarts;
        float RcDischargeTemp;
        float RcOuLiquidPipeTemp;
        float RcOuTwoPhasePipeTemp;
        float RcOuSuctionPipeTemp;
        float RcOuHeatSinkTemp;
        float RcDischargeSuperHeatTemp;
        float RcSubCoolTemp;
        uint16_t RcFanSpeedRpm;

/* polynomial fit for
Temp C	specific heat (J/Kg. K)
0.01	4.212
10	    4.191
20	    4.183
30	    4.174
40	    4.174
50	    4.174
60	    4.179
70	    4.187
80	    4.195
90	    4.208
100	    4.22
*/
        float estimate_water_constant(float temp) {
            return 4.21f + -2.04e-03 * temp + 3.09e-05 * temp * temp + -9.63e-08 * temp * temp * temp;
        }

        void update_output_power_estimation(float specific_heat_constant_override = NAN) {
            if (!std::isnan(HpFeedTemperature) && !std::isnan(HpReturnTemperature) && FlowRate > 0) {
                float specific_heat_constant = std::isnan(specific_heat_constant_override) ? estimate_water_constant(HpFeedTemperature) : specific_heat_constant_override;
                ComputedOutputPower = (FlowRate/60.0) * (HpFeedTemperature - HpReturnTemperature) * specific_heat_constant;
            }
            else {
                ComputedOutputPower = 0.0f;
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
    };

} // namespace ecodan
} // namespace esphome
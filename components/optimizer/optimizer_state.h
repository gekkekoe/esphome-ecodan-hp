#pragma once

#include "esphome.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/globals/globals_component.h"

// forward declare EcodanHeatpump
namespace esphome
{
  namespace ecodan
  {
    class EcodanHeatpump;
  }
}

namespace esphome
{
  namespace optimizer
  {

    static constexpr const char *OPTIMIZER_TAG       = "auto_adaptive";
    static constexpr const char *OPTIMIZER_CYCLE_TAG = "short_cycle";

    enum class OptimizerZone : uint8_t
    {
        ZONE_1 = 1,
        ZONE_2 = 2,
        SINGLE = 0
    };

    enum class OptimizerOperationMode : uint8_t
    {
        UNAVAILABLE = 255,
        OFF = 0,
        DHW_ON = 1,
        HEAT_ON = 2, // Heating
        COOL_ON = 3, // Cooling
        FROST_PROTECT = 5,
        LEGIONELLA_PREVENTION = 6
    }; 

    struct FlowLimits {
        float min;
        float max;
    };

    // Parameters derived from heating system profile — computed once per adaptive loop
    struct HeatingProfile {
        float base_min_delta_t;
        float min_delta_cold_limit;
        float max_delta_t;
        float max_error_range;
        float defrost_memory_ms;
    };

    struct OptimizerState
    {
        esphome::ecodan::EcodanHeatpump *ecodan_instance;

        esphome::switch_::Switch *auto_adaptive_control_enabled{nullptr};
        esphome::switch_::Switch *predictive_short_cycle_control_enabled{nullptr};
        esphome::switch_::Switch *defrost_risk_handling_enabled{nullptr};
        esphome::switch_::Switch *smart_boost_enabled{nullptr};
        esphome::switch_::Switch *relay_switch_z1{nullptr};
        esphome::switch_::Switch *relay_switch_z2{nullptr};
        esphome::switch_::Switch *sw_odin_override_z1{nullptr};
        esphome::switch_::Switch *sw_odin_override_z2{nullptr};
        esphome::switch_::Switch *sw_force_dhw{nullptr};
        esphome::switch_::Switch *sw_regular_dhw{nullptr};

        esphome::binary_sensor::BinarySensor *status_short_cycle_lockout;
        esphome::binary_sensor::BinarySensor *status_predictive_boost_active{nullptr};
        esphome::binary_sensor::BinarySensor *status_compressor;
        esphome::binary_sensor::BinarySensor *status_defrost;
        esphome::binary_sensor::BinarySensor *bin_odin_forwarder_takeover{nullptr};

        esphome::sensor::Sensor *hp_feed_temp;
        esphome::sensor::Sensor *z1_feed_temp;
        esphome::sensor::Sensor *z2_feed_temp;
        esphome::sensor::Sensor *operation_mode;


        esphome::number::Number *auto_adaptive_setpoint_bias;
        esphome::number::Number *temperature_feedback_z1;
        esphome::number::Number *temperature_feedback_z2;
        esphome::number::Number *maximum_heating_flow_temp;
        esphome::number::Number *minimum_heating_flow_temp;
        esphome::number::Number *maximum_heating_flow_temp_z2;
        esphome::number::Number *minimum_heating_flow_temp_z2;
        esphome::number::Number *minimum_cooling_flow_temp_z1;
        esphome::number::Number *minimum_cooling_flow_temp_z2;
        esphome::number::Number *cooling_smart_start_temp;
        esphome::number::Number *minimum_compressor_on_time;

        esphome::select::Select *heating_system_type;
        esphome::select::Select *temperature_feedback_source_z1;
        esphome::select::Select *temperature_feedback_source_z2;
        esphome::select::Select *lockout_duration;
        esphome::select::Select *lockout_strategy{nullptr};

        // Only base climate::Climate state (current_temperature/target_temperature)
        // is read via these pointers (see optimizer/utility.cpp), so they don't
        // need to be typed as thermostat::ThermostatClimate specifically.
        esphome::climate::Climate *asgard_vt_z1;
        esphome::climate::Climate *asgard_vt_z2;

        // Legionella DHW setpoint automation
        esphome::switch_::Switch *legionella_dhw_automation_enabled{nullptr};
        esphome::number::Number *legionella_dhw_setpoint{nullptr};
        esphome::climate::Climate *dhw_climate{nullptr};
        esphome::globals::RestoringGlobalsComponent<float> *legionella_saved_dhw_setpoint{nullptr};

        uint32_t &lockout_expiration_timestamp;
    };

  } // namespace optimizer
} // namespace esphome
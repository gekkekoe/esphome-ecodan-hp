#pragma once

#include "esphome.h"
#include "esphome/components/thermostat/thermostat_climate.h"

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

        esphome::switch_::Switch *auto_adaptive_control_enabled;
        esphome::switch_::Switch *predictive_short_cycle_control_enabled;
        esphome::switch_::Switch *defrost_risk_handling_enabled;
        esphome::switch_::Switch *smart_boost_enabled;
        esphome::switch_::Switch *sw_use_solver;
        esphome::switch_::Switch *demand_switch_z1{nullptr};
        esphome::switch_::Switch *demand_switch_z2{nullptr};

        esphome::binary_sensor::BinarySensor *status_short_cycle_lockout;
        esphome::binary_sensor::BinarySensor *status_predictive_boost_active;
        esphome::binary_sensor::BinarySensor *status_compressor;
        esphome::binary_sensor::BinarySensor *status_defrost;

        esphome::sensor::Sensor *hp_feed_temp;
        esphome::sensor::Sensor *z1_feed_temp;
        esphome::sensor::Sensor *z2_feed_temp;
        esphome::sensor::Sensor *operation_mode;
        esphome::sensor::Sensor *computed_output_power;
        esphome::sensor::Sensor *daily_heating_produced;
        esphome::sensor::Sensor *daily_heating_consumed;

        esphome::number::Number *solver_kwh_meter_feedback;
        esphome::number::Number *auto_adaptive_setpoint_bias;
        esphome::number::Number *temperature_feedback_z1;
        esphome::number::Number *temperature_feedback_z2;
        esphome::number::Number *maximum_heating_flow_temp;
        esphome::number::Number *minimum_heating_flow_temp;
        esphome::number::Number *maximum_heating_flow_temp_z2;
        esphome::number::Number *minimum_heating_flow_temp_z2;
        esphome::number::Number *minimum_cooling_flow_temp;
        esphome::number::Number *cooling_smart_start_temp;
        esphome::number::Number *minimum_compressor_on_time;
        esphome::number::Number *predictive_short_cycle_high_delta_time_window;
        esphome::number::Number *predictive_short_cycle_high_delta_threshold;
        esphome::number::Number *num_raw_heat_produced;
        esphome::number::Number *num_raw_elec_consumed;
        esphome::number::Number *num_raw_runtime_hours;
        esphome::number::Number *num_raw_avg_outside_temp;
        esphome::number::Number *num_raw_avg_room_temp;
        esphome::number::Number *num_raw_delta_room_temp;
        esphome::number::Number *num_raw_max_output;
        esphome::number::Number *num_raw_hl_tm_product;  // heat_loss × thermal_mass from night cooling
        esphome::number::Number *num_battery_soc_kwh;
        esphome::number::Number *num_battery_max_discharge_kw;

        esphome::select::Select *heating_system_type;
        esphome::select::Select *temperature_feedback_source_z1;
        esphome::select::Select *temperature_feedback_source_z2;
        esphome::select::Select *lockout_duration;
        esphome::select::Select *solver_kwh_meter_feedback_source;

        esphome::thermostat::ThermostatClimate *asgard_vt_z1;
        esphome::thermostat::ThermostatClimate *asgard_vt_z2;

        uint32_t &lockout_expiration_timestamp;
    };

  } // namespace optimizer
} // namespace esphome

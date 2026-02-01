#pragma once

#include "esphome.h"
#include "esphome/components/thermostat/thermostat_climate.h"

// forward declare EcodanHeatpump
namespace esphome
{
  namespace ecodan
  {
    class EcodanHeatpump;
  } // namespace ecodan
} // namespace esphome

namespace esphome
{
  namespace optimizer
  {

    static constexpr const char *OPTIMIZER_TAG = "auto_adaptive";
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

    struct OptimizerState
    {
      esphome::ecodan::EcodanHeatpump *ecodan_instance;

      esphome::switch_::Switch *auto_adaptive_control_enabled;
      esphome::switch_::Switch *predictive_short_cycle_control_enabled;
      esphome::switch_::Switch *defrost_risk_handling_enabled;
      esphome::switch_::Switch *smart_boost_enabled;

      esphome::binary_sensor::BinarySensor *status_short_cycle_lockout;
      esphome::binary_sensor::BinarySensor *status_predictive_boost_active;
      esphome::binary_sensor::BinarySensor *status_compressor;
      esphome::binary_sensor::BinarySensor *status_defrost;

      esphome::sensor::Sensor *hp_feed_temp;
      esphome::sensor::Sensor *z1_feed_temp;
      esphome::sensor::Sensor *z2_feed_temp;
      esphome::sensor::Sensor *operation_mode;
  
      esphome::sensor::Sensor *daily_heating_produced;
      esphome::sensor::Sensor *daily_heating_consumed;

      esphome::number::Number *solver_kwh_meter_feedback;
      esphome::select::Select *heating_system_type;
      esphome::select::Select *temperature_feedback_source;
      esphome::select::Select *lockout_duration;
      esphome::select::Select *solver_kwh_meter_feedback_source;

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

      esphome::thermostat::ThermostatClimate *asgard_vt_z1;
      esphome::thermostat::ThermostatClimate *asgard_vt_z2;

      // stats vars calculated daily
      float &learned_heat_loss_global;
      float &learned_base_cop_global;
      float &learned_thermal_output_global;
      float &learned_elec_power_global;

      uint32_t &lockout_expiration_timestamp;
    };

    class Optimizer
    {
    private:
      OptimizerState state_;

      // store last dhw end to trail feed temp
      uint32_t dhw_post_run_expiration_ = 0;
      float dhw_old_z1_setpoint_ = NAN;
      float dhw_old_z2_setpoint_ = NAN;

      // predictive prevention restore points
      float pcp_old_z1_setpoint_ = NAN;
      float pcp_old_z2_setpoint_ = NAN;
      // need to store offset set by prevention events
      float pcp_adjustment_z1_ = 0.0f;
      float pcp_adjustment_z2_ = 0.0f;

      uint32_t predictive_delta_start_time_z1_ = 0;
      uint32_t predictive_delta_start_time_z2_ = 0;
      uint32_t compressor_start_time_ = 0;
      uint32_t last_defrost_time_ = 0;

      // save last callback state, to only invoke callback on actual change
      float last_hp_feed_temp_ = NAN;
      float last_z1_feed_temp_ = NAN;
      float last_z2_feed_temp_ = NAN;

      // cast to uint8 when actually using
      float last_operation_mode_ = NAN;

      // cast to bool
      float last_defrost_status_ = 0;
      float last_compressor_status_ = 0;

      // smart boost vars
      uint32_t stagnation_start_time_ = 0;
      float last_error_ = 0.0f;
      float current_stagnation_boost_ = 1.0f;

      // Learning state variables
      float daily_temp_sum_ = 0.0f;
      int daily_temp_count_ = 0;
      int last_processed_day_ = -1;

      float daily_runtime_global = 0; 
      // helpers to track runtime
      uint32_t last_check_ms_ = 0;

      // Energy snapshots for delta calculation
      float last_total_heating_produced_ = 0.0f;
      float last_total_heating_consumed_ = 0.0f;

      void process_adaptive_zone_(
          std::size_t i,
          const ecodan::Status &status,
          float defrost_memory_ms,
          float cold_factor,
          float min_delta_cold_limit,
          float base_min_delta_t,
          float max_delta_t,
          float max_error_range,
          float actual_outside_temp,
          float zone_max_flow_temp,
          float zone_min_flow_temp,
          float &out_flow_heat,
          float &out_flow_cool);
      
      float round_nearest(float input) { return round(input * 10.0f) / 10.0f; }
      float round_nearest_half(float input) { return floor(input * 2.0) / 2.0f; }
      bool is_system_hands_off(const ecodan::Status &status);
      bool is_dhw_active(const ecodan::Status &status);
      bool is_post_dhw_window(const ecodan::Status &status);
      bool is_heating_active(const ecodan::Status &status);
      float clamp_flow_temp(float calculated_flow, float min_temp, float max_temp);
      float enforce_step_down(const ecodan::Status &status, float actual_flow_temp, float calculated_flow);
      bool set_flow_temp(float flow, OptimizerZone zone);

      void predictive_short_cycle_check_for_zone_(const ecodan::Status &status, OptimizerZone zone);

      // smart boost
      float calculate_smart_boost(int profile, float error);

      // callback handlers for important events
      void on_feed_temp_change(float actual_flow_temp, OptimizerZone zone);
      void on_operation_mode_change(uint8_t new_mode, uint8_t previous_mode);

      // stats
      void update_learning_model(int day_of_year);

    public:
      Optimizer(OptimizerState state);

      bool get_predictive_boost_state();
      void reset_predictive_boost();
      void run_auto_adaptive_loop();
      void predictive_short_cycle_check();
      void restore_svc_state();
      void start_lockout();
      void check_lockout_expiration();
      void on_compressor_stop();
      void on_compressor_state_change(bool x, bool x_previous);
      void on_defrost_state_change(bool x, bool x_previous);
      void update_boost_sensor();

      void update_heat_model();
      float get_room_current_temp(OptimizerZone zone);
      float get_room_target_temp(OptimizerZone zone);
      float get_feed_temp(OptimizerZone zone);
      float get_return_temp(OptimizerZone zone);
      float get_flow_setpoint(OptimizerZone zone);
      FlowLimits get_flow_limits(OptimizerZone zone);
    };

    // dummy, can remain empty
    class OptimizerComponent : public esphome::Component
    {
    public:
      void setup() override {}
      void loop() override {}
      void dump_config() override
      {
        ESP_LOGCONFIG("optimizer", "Optimizer Custom Component");
      }
    };

  } // namespace optimizer
} // namespace esphome
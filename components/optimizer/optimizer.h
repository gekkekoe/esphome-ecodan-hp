#pragma once

#include "esphome.h"

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

    struct OptimizerState
    {
      esphome::ecodan::EcodanHeatpump *ecodan_instance;

      esphome::switch_::Switch *auto_adaptive_control_enabled;
      esphome::switch_::Switch *predictive_short_cycle_control_enabled;

      esphome::binary_sensor::BinarySensor *status_short_cycle_lockout;
      esphome::binary_sensor::BinarySensor *status_compressor;
      esphome::binary_sensor::BinarySensor *status_predictive_boost_active;

      esphome::sensor::Sensor *hp_feed_temp;
      esphome::sensor::Sensor *hp_return_temp;
      esphome::sensor::Sensor *outside_temp;

      esphome::select::Select *heating_system_type;
      esphome::select::Select *temperature_feedback_source;
      esphome::select::Select *lockout_duration;

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

      uint32_t *lockout_expiration_timestamp;
    };

    class Optimizer
    {
    private:
      OptimizerState state_;

      uint32_t predictive_delta_start_time_ = 0;
      uint32_t compressor_start_time_ = 0;
      uint32_t last_defrost_time_ = 0;
      float predictive_short_cycle_total_adjusted_ = 0.0f;

      void process_adaptive_zone_(
          std::size_t i,
          const ecodan::Status &status,
          float base_min_delta_t,
          float max_delta_t,
          float max_error_range,
          float dynamic_min_delta_t,
          float actual_outside_temp,
          float zone_max_flow_temp,
          float zone_min_flow_temp,
          float &out_flow_heat,
          float &out_flow_cool);
      
      float round_nearest(float input) { return floor(input * 10.0f) / 10.0f; }
      float round_nearest_half(float input) { return floor(input * 2.0) / 2.0f; }
      bool is_system_hands_off(const ecodan::Status &status);
      float clamp_flow_temp(float calculated_flow, float min_temp, float max_temp);

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
      void update_boost_sensor();
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
#pragma once

#include <atomic>
#include "optimizer_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <lwip/netdb.h>
#include <lwip/sockets.h>

namespace esphome
{
  namespace optimizer
  {

    class Optimizer
    {
    private:
      OptimizerState state_;

      // DHW trailing setpoint
      uint32_t dhw_post_run_expiration_ = 0;
      float dhw_old_z1_setpoint_ = NAN;
      float dhw_old_z2_setpoint_ = NAN;

      float predictive_boost_base_z1_setpoint_ = NAN;
      float predictive_boost_base_z2_setpoint_ = NAN;

      // Short-cycle lockout strategy state
      int   active_lockout_strategy_      = 0;
      float flow_lockout_old_z1_setpoint_ = NAN;
      float flow_lockout_old_z2_setpoint_ = NAN;

      // Compressor / defrost tracking
      struct DefrostState {
        float locked_outside_temp_{NAN};
        float locked_return_temp_{NAN};
        float locked_return_temp_z1_{NAN};
        float locked_return_temp_z2_{NAN};

        float get_return_temp(bool independent_zone_temp, OptimizerZone zone) const {
            if (independent_zone_temp)
                return (zone == OptimizerZone::ZONE_2) ? locked_return_temp_z2_ : locked_return_temp_z1_;        
            return locked_return_temp_;
        }
      };

      OptimizerOperationMode to_operation_mode(float val) {
          if (std::isnan(val)) return OptimizerOperationMode::OFF;
          
          int int_val = static_cast<int>(std::round(val));
          switch (int_val) {
              case 1:   return OptimizerOperationMode::DHW_ON;
              case 2:   return OptimizerOperationMode::HEAT_ON;
              case 3:   return OptimizerOperationMode::COOL_ON;
              //case 5:   return OptimizerOperationMode::FROST_PROTECT;
              //case 6:   return OptimizerOperationMode::LEGIONELLA_PREVENTION;
              case 255: return OptimizerOperationMode::UNAVAILABLE;
              default:  return OptimizerOperationMode::OFF;
          }
      };

      bool is_cooling_mode(const ecodan::Status& status, OptimizerZone zone) {
        auto ecodan_zone = zone == OptimizerZone::ZONE_1 ? ecodan::Zone::ZONE_1 : ecodan::Zone::ZONE_2;
        return status.has_cooling() && status.is_auto_adaptive_cooling(ecodan_zone);
      }

      uint32_t compressor_start_time_ = 0;
      uint32_t last_defrost_time_     = 0;
      DefrostState state_before_defrost_;


      // Callback state (detect change before firing)
      float last_hp_feed_temp_      = NAN;
      float last_z1_feed_temp_      = NAN;
      float last_z2_feed_temp_      = NAN;
      float last_operation_mode_    = NAN;
      float last_defrost_status_    = 0;
      float last_compressor_status_ = 0;

      // Smart boost
      uint32_t stagnation_start_time_  = 0;
      float last_error_                = 0.0f;
      float current_stagnation_boost_  = 1.0f;


      bool adaptive_loop_running_ {false};

      // ── adaptive_loop.cpp ──────────────────────────────────────────────
      HeatingProfile   get_heating_profile_(int type_index);
      DefrostState resolve_defrost_state_();
      float            calculate_heating_flow_(std::size_t zone_i,
                                               const ecodan::Status &status,
                                               const HeatingProfile &prof,
                                               bool set_point_reached,
                                               float cold_factor,
                                               float actual_outside_temp,
                                               float zone_min, float zone_max,
                                               float error_factor,
                                               float smart_boost);
      float            calculate_cooling_flow_(std::size_t zone_i,
                                               const ecodan::Status &status,
                                               float target_delta_t);
      void             process_adaptive_zone_(std::size_t i,
                                              const ecodan::Status &status,
                                              const HeatingProfile &prof,
                                              float cold_factor,
                                              float actual_outside_temp,
                                              float zone_max, float zone_min,
                                              float &out_flow_heat,
                                              float &out_flow_cool);

      // ── smart_boost.cpp ───────────────────────────────────────────────
      float calculate_smart_boost(int profile, float error);


      // ── events.cpp ────────────────────────────────────────────────────
      void on_feed_temp_change(float actual_flow_temp, OptimizerZone zone);
      void handle_dhw_feed_temp_(float actual_flow_temp, OptimizerZone zone);
      void on_operation_mode_change(uint8_t new_mode, uint8_t previous_mode);
      void handle_legionella_transition_(bool entering);

      // ── prevention.cpp ────────────────────────────────────────────────
      void predictive_short_cycle_check_for_zone_(const ecodan::Status &status, OptimizerZone zone, bool is_cooling);
      void clear_predictive_boost_(OptimizerZone zone, bool restore);
      void apply_flow_lockout_setpoint_(const ecodan::Status &status, OptimizerZone zone, float actual_flow_temp, bool initial);

      // ── utility.cpp ───────────────────────────────────────────────────
      bool  is_system_hands_off(const ecodan::Status &status);
      bool  is_dhw_active(const ecodan::Status &status);
      bool  is_post_dhw_window(const ecodan::Status &status);
      bool  is_compressor_active(const ecodan::Status &status);
      float clamp_flow_temp(float flow, float min_temp, float max_temp);
      float enforce_step_limit(const ecodan::Status &status, float actual_flow, float calculated_flow, bool is_cooling_mode);
      bool  set_flow_temp(float flow, OptimizerZone zone);
      float round_nearest(float input)      { return round(input * 10.0f) / 10.0f; }
      float round_nearest_half(float input) { return floor(input * 2.0) / 2.0f; }

    public:
      Optimizer(OptimizerState state);

      // Main loops
      void run_auto_adaptive_loop();
      void predictive_short_cycle_check();

      // Compressor / defrost events
      void on_compressor_stop();
      void on_compressor_state_change(bool x, bool x_previous);
      void on_defrost_state_change(bool x, bool x_previous);

      // Lockout
      void restore_pre_lockout_state();
      void start_lockout();
      void check_lockout_expiration();

      // Predictive boost sensor
      bool get_predictive_boost_state();
      void update_boost_sensor();

      // Temperature helpers (used by YAML / dashboard)
      float get_room_current_temp(OptimizerZone zone);
      float get_room_target_temp(OptimizerZone zone);
      float get_feed_temp(OptimizerZone zone);
      float get_return_temp(OptimizerZone zone);
      float get_flow_setpoint(OptimizerZone zone);
      FlowLimits get_flow_limits(OptimizerZone zone);
      FlowLimits get_cool_flow_limits(OptimizerZone zone);

      float apply_flow_limits(OptimizerZone zone, float calculated, float target_delta_t, float guard_return_temp);
      float limit_external_flow(OptimizerZone zone, float requested);

      // Solver / ODIN
      bool aa_enabled() const;
      bool odin_forwarder_takeover_active() const;
    };

    // Dummy ESPHome component — triggers codegen, stays empty
    class OptimizerComponent : public esphome::Component
    {
    public:
      void setup() override {}
      void loop()  override {}
      void dump_config() override { ESP_LOGCONFIG("optimizer", "Optimizer Custom Component"); }
    };

  } // namespace optimizer
} // namespace esphome
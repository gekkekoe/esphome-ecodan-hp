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

      // Predictive short-cycle prevention
      float pcp_old_z1_setpoint_ = NAN;
      float pcp_old_z2_setpoint_ = NAN;
      float pcp_adjustment_z1_   = 0.0f;
      float pcp_adjustment_z2_   = 0.0f;
      uint32_t predictive_delta_start_time_z1_ = 0;
      uint32_t predictive_delta_start_time_z2_ = 0;

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

      // Stats / learning
      float    daily_outside_temp_sum_      = 0.0f;
      int      daily_outside_temp_count_    = 0;
      float    daily_room_temp_sum_         = 0.0f;
      int      daily_room_temp_count_       = 0;
      float    daily_room_temp_min_         = 99.0f;
      float    daily_room_temp_max_         = -99.0f;
      float    daily_runtime_global         = 0.0f;
      float    daily_max_output_power_      {0.0f};
      uint32_t last_check_ms_               = 0;
      int      last_processed_day_          = -1;
      int      last_processed_hour_         {-1};
      float    last_total_heating_produced_ = 0.0f;
      float    last_total_heating_consumed_ = 0.0f;

      // Free cooling window tracking (HP-off period, any time of day)
      // Measures HL×TM product from unregulated cooldown, free of solar/DHW contamination.
      bool     fc_active_        = false;
      float    fc_room_start_    = NAN;
      float    fc_outside_sum_   = 0.0f;
      int      fc_outside_count_ = 0;
      float    fc_hours_         = 0.0f;

      // ODIN solver data
      std::atomic<bool> odin_fetch_requested_{false};
      std::vector<float> odin_energy_;
      std::vector<float> odin_production_;
      int      odin_data_day_   {-1};
      bool     odin_data_ready_ {false};
      SemaphoreHandle_t odin_mutex_ = NULL;

      // Solver soft-stop state
      int  solver_stop_hour_   {-1};
      bool solver_stop_active_ {false};
      int  solver_resume_hour_ {-1};
      bool adaptive_loop_running_ {false};

      // ── adaptive_loop.cpp ──────────────────────────────────────────────
      HeatingProfile   get_heating_profile_(int type_index);
      struct SolverResult { float load_ratio; bool heating_off; };
      DefrostState resolve_defrost_state_();
      SolverResult resolve_solver_result_(float room_target_temp, float current_room_temp);
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

      // ── solver.cpp ────────────────────────────────────────────────────
      void apply_solver_soft_stop(bool should_stop);

      // ── events.cpp ────────────────────────────────────────────────────
      void on_feed_temp_change(float actual_flow_temp, OptimizerZone zone);
      void on_operation_mode_change(uint8_t new_mode, uint8_t previous_mode);

      // ── prevention.cpp ────────────────────────────────────────────────
      void predictive_short_cycle_check_for_zone_(const ecodan::Status &status, OptimizerZone zone);

      // ── stats.cpp ─────────────────────────────────────────────────────
      void update_learning_model(int day_of_year);

      // ── utility.cpp ───────────────────────────────────────────────────
      bool  is_system_hands_off(const ecodan::Status &status);
      bool  is_dhw_active(const ecodan::Status &status);
      bool  is_post_dhw_window(const ecodan::Status &status);
      bool  is_heating_active(const ecodan::Status &status);
      float clamp_flow_temp(float flow, float min_temp, float max_temp);
      float enforce_step_down(const ecodan::Status &status, float actual_flow, float calculated_flow);
      bool  set_flow_temp(float flow, OptimizerZone zone);
      float round_nearest(float input)      { return round(input * 10.0f) / 10.0f; }
      float round_nearest_half(float input) { return floor(input * 2.0) / 2.0f; }

    public:
      Optimizer(OptimizerState state);

      // Main loops
      void run_auto_adaptive_loop();
      void predictive_short_cycle_check();
      void update_heat_model();

      // Compressor / defrost events
      void on_compressor_stop();
      void on_compressor_state_change(bool x, bool x_previous);
      void on_defrost_state_change(bool x, bool x_previous);

      // Lockout
      void restore_svc_state();
      void start_lockout();
      void check_lockout_expiration();

      // Boost sensor
      bool get_predictive_boost_state();
      void reset_predictive_boost();
      void update_boost_sensor();

      // Temperature helpers (used by YAML / dashboard)
      float get_room_current_temp(OptimizerZone zone);
      float get_room_target_temp(OptimizerZone zone);
      float get_feed_temp(OptimizerZone zone);
      float get_return_temp(OptimizerZone zone);
      float get_flow_setpoint(OptimizerZone zone);
      FlowLimits get_flow_limits(OptimizerZone zone);

      // Solver / ODIN
      bool aa_enabled() const;
      bool solver_enabled() const;
      int  get_current_ecodan_hour();
      int  get_current_ecodan_day();
      bool has_old_odin_data();
      void store_odin_data(int current_hour, const std::vector<float>& prod, const std::vector<float>& energy);
      bool check_and_clear_odin_fetch_request() {
          return odin_fetch_requested_.exchange(false);
      }
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

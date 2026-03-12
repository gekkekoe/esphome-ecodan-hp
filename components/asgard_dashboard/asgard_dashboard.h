#pragma once
#include <vector> 

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/globals/globals_component.h"



namespace esphome {
namespace asgard_dashboard {

struct DashboardAction {
  std::string key;
  std::string s_value;
  float f_value;
  bool is_string;
};

// 24 bytes per record
struct HistoryRecord {
  uint32_t timestamp;
  int16_t hp_feed;
  int16_t hp_return;
  int16_t z1_sp;
  int16_t z2_sp;
  int16_t z1_curr;
  int16_t z2_curr;
  int16_t z1_flow;
  int16_t z2_flow;
  int16_t freq;
  uint16_t flags; // Bit 0-5 = booleans, Bit 6-9 = mode
  int16_t cons;
};

struct DashboardSnapshot {
  // Boolean sensors
  bool ui_use_room_z1{false};
  bool ui_use_room_z2{false};
  bool status_compressor{false};
  bool status_booster{false};
  bool status_defrost{false};
  bool status_water_pump{false};
  bool status_in1_request{false};
  bool status_in6_request{false};
  bool status_zone2_enabled{false};
  bool pred_sc_switch{false};
  bool sw_auto_adaptive{false};
  bool sw_defrost_mit{false};
  bool sw_smart_boost{false};
  bool sw_force_dhw{false};

  // Float sensors
  float hp_feed_temp{NAN};
  float hp_return_temp{NAN};
  float outside_temp{NAN};
  float compressor_frequency{NAN};
  float flow_rate{NAN};
  float computed_output_power{NAN};
  float daily_computed_output_power{NAN};
  float daily_total_energy_consumption{NAN};
  float compressor_starts{NAN};
  float runtime{NAN};
  float wifi_signal_db{NAN};

  float dhw_temp{NAN};
  float dhw_flow_temp_target{NAN};
  float dhw_flow_temp_drop{NAN};
  float dhw_consumed{NAN};
  float dhw_delivered{NAN};
  float dhw_cop{NAN};

  float heating_consumed{NAN};
  float heating_produced{NAN};
  float heating_cop{NAN};
  float cooling_consumed{NAN};
  float cooling_produced{NAN};
  float cooling_cop{NAN};

  float z1_flow_temp_target{NAN};
  float z2_flow_temp_target{NAN};

  // Number sensors with limits
  struct NumData { float val{NAN}; float min{NAN}; float max{NAN}; float step{NAN}; };
  NumData num_aa_setpoint_bias;
  NumData num_max_flow_temp;
  NumData num_min_flow_temp;
  NumData num_max_flow_temp_z2;
  NumData num_min_flow_temp_z2;
  NumData num_hysteresis_z1;
  NumData num_hysteresis_z2;
  NumData pred_sc_time;
  NumData pred_sc_delta;

  // cooling settings
  NumData num_cooling_smart_start_z1;
  NumData num_min_cooling_flow_z1;

  // Climate data
  struct ClimData { float curr{NAN}; float tar{NAN}; int action{-1}; int mode{-1}; };
  ClimData virt_z1;
  ClimData virt_z2;
  ClimData room_z1;
  ClimData room_z2;
  ClimData flow_z1;
  ClimData flow_z2;

  // Selects & modes
  float operation_mode{NAN};
  int sel_heating_system_type{-1};
  int sel_room_temp_source_z1{-1};
  int sel_room_temp_source_z2{-1};
  int sel_operating_mode_z1{-1};
  int sel_operating_mode_z2{-1};
  int sel_temp_source_z1{-1};
  int sel_temp_source_z2{-1};

  // Safe fixed-size character arrays for text sensors
  char version[32]{0};

  // solver data
  bool sw_use_solver{false};
  bool bin_solver_connected{false};
  
  NumData num_raw_heat_produced;
  NumData num_raw_elec_consumed;
  NumData num_raw_runtime_hours;
  NumData num_raw_avg_outside_temp;
  NumData num_raw_avg_room_temp;
  NumData num_raw_delta_room_temp;
  NumData num_raw_max_output;
  NumData num_raw_hl_tm_product;

  NumData num_battery_soc_kwh;
  NumData num_battery_max_discharge_kw;

  char txt_solver_ip[32]{0};
};

class EcodanDashboard : public Component, public AsyncWebHandler {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  // Web server
  void set_web_server_base(web_server_base::WebServerBase *b) { base_ = b; }

  // Sensors
  void set_hp_feed_temp(sensor::Sensor *s)                    { hp_feed_temp_ = s; }
  void set_hp_return_temp(sensor::Sensor *s)                  { hp_return_temp_ = s; }
  void set_outside_temp(sensor::Sensor *s)                    { outside_temp_ = s; }
  void set_compressor_frequency(sensor::Sensor *s)            { compressor_frequency_ = s; }
  void set_flow_rate(sensor::Sensor *s)                       { flow_rate_ = s; }
  void set_computed_output_power(sensor::Sensor *s)           { computed_output_power_ = s; }
  void set_daily_computed_output_power(sensor::Sensor *s)     { daily_computed_output_power_ = s; }
  void set_daily_total_energy_consumption(sensor::Sensor *s)  { daily_total_energy_consumption_ = s; }
  void set_compressor_starts(sensor::Sensor *s)               { compressor_starts_ = s; }
  void set_runtime(sensor::Sensor *s)                         { runtime_ = s; }
  void set_wifi_signal_db(sensor::Sensor *s)                  { wifi_signal_db_ = s; }

  void set_dhw_temp(sensor::Sensor *s)                        { dhw_temp_ = s; }
  void set_dhw_flow_temp_target(sensor::Sensor *s)            { dhw_flow_temp_target_ = s; }
  void set_dhw_flow_temp_drop(sensor::Sensor *s)              { dhw_flow_temp_drop_ = s; }
  void set_dhw_consumed(sensor::Sensor *s)                    { dhw_consumed_ = s; }
  void set_dhw_delivered(sensor::Sensor *s)                   { dhw_delivered_ = s; }
  void set_dhw_cop(sensor::Sensor *s)                         { dhw_cop_ = s; }

  void set_heating_consumed(sensor::Sensor *s)                { heating_consumed_ = s; }
  void set_heating_produced(sensor::Sensor *s)                { heating_produced_ = s; }
  void set_heating_cop(sensor::Sensor *s)                     { heating_cop_ = s; }
  void set_cooling_consumed(sensor::Sensor *s)                { cooling_consumed_ = s; }
  void set_cooling_produced(sensor::Sensor *s)                { cooling_produced_ = s; }
  void set_cooling_cop(sensor::Sensor *s)                     { cooling_cop_ = s; }
  void set_operation_mode(sensor::Sensor *s)                  { operation_mode_ = s; }

  // Flow Temp Targets
  void set_z1_flow_temp_target(sensor::Sensor *s)             { z1_flow_temp_target_ = s; }
  void set_z2_flow_temp_target(sensor::Sensor *s)             { z2_flow_temp_target_ = s; }

  // Binary sensors
  void set_status_compressor(binary_sensor::BinarySensor *b)  { status_compressor_ = b; }
  void set_status_booster(binary_sensor::BinarySensor *b)     { status_booster_ = b; }
  void set_status_defrost(binary_sensor::BinarySensor *b)     { status_defrost_ = b; }
  void set_status_water_pump(binary_sensor::BinarySensor *b)  { status_water_pump_ = b; }
  void set_status_in1_request(binary_sensor::BinarySensor *b) { status_in1_request_ = b; }
  void set_status_in6_request(binary_sensor::BinarySensor *b) { status_in6_request_ = b; }
  void set_status_zone2_enabled(binary_sensor::BinarySensor *b) { status_zone2_enabled_ = b; }

  // Text sensors
  void set_version(text_sensor::TextSensor *t)                { version_ = t; }

  // Switches
  void set_sw_auto_adaptive(switch_::Switch *s)               { sw_auto_adaptive_ = s; }
  void set_sw_defrost_mit(switch_::Switch *s)                 { sw_defrost_mit_ = s; }
  void set_sw_smart_boost(switch_::Switch *s)                 { sw_smart_boost_ = s; }
  void set_sw_force_dhw(switch_::Switch *s)                   { sw_force_dhw_ = s; } 
  void set_pred_sc_switch(switch_::Switch *s)                 { pred_sc_switch_ = s; }

  // Selects
  void set_sel_heating_system_type(select::Select *s)         { sel_heating_system_type_ = s; }
  void set_sel_room_temp_source_z1(select::Select *s)         { sel_room_temp_source_z1_ = s; }
  void set_sel_room_temp_source_z2(select::Select *s)         { sel_room_temp_source_z2_ = s; }
  void set_sel_operating_mode_z1(select::Select *s)           { sel_operating_mode_z1_ = s; }
  void set_sel_operating_mode_z2(select::Select *s)           { sel_operating_mode_z2_ = s; }
  void set_sel_temp_source_z1(select::Select *s)              { sel_temp_source_z1_ = s; }
  void set_sel_temp_source_z2(select::Select *s)              { sel_temp_source_z2_ = s; }

  // Numbers (for writing)
  void set_num_aa_setpoint_bias(number::Number *n)            { num_aa_setpoint_bias_ = n; }
  void set_num_max_flow_temp(number::Number *n)               { num_max_flow_temp_ = n; }
  void set_num_min_flow_temp(number::Number *n)               { num_min_flow_temp_ = n; }
  void set_num_max_flow_temp_z2(number::Number *n)            { num_max_flow_temp_z2_ = n; }
  void set_num_min_flow_temp_z2(number::Number *n)            { num_min_flow_temp_z2_ = n; }
  void set_num_hysteresis_z1(number::Number *n)               { num_hysteresis_z1_ = n; }
  void set_num_hysteresis_z2(number::Number *n)               { num_hysteresis_z2_ = n; }
  void set_pred_sc_time(number::Number *n)                    { pred_sc_time_ = n; }
  void set_pred_sc_delta(number::Number *n)                   { pred_sc_delta_ = n; }

  // Cooling Settings (Numbers)
  void set_num_cooling_smart_start_z1(number::Number *n)      { num_cooling_smart_start_z1_ = n; }
  void set_num_min_cooling_flow_z1(number::Number *n)         { num_min_cooling_flow_z1_ = n; }

  // Climate
  void set_dhw_climate(climate::Climate *c)                   { dhw_climate_ = c; }
  void set_virtual_climate_z1(climate::Climate *c)            { virtual_climate_z1_ = c; }
  void set_virtual_climate_z2(climate::Climate *c)            { virtual_climate_z2_ = c; }
  void set_heatpump_climate_z1(climate::Climate *c)           { heatpump_climate_z1_ = c; }
  void set_heatpump_climate_z2(climate::Climate *c)           { heatpump_climate_z2_ = c; }
  void set_flow_climate_z1(climate::Climate *c)               { flow_climate_z1_ = c; }
  void set_flow_climate_z2(climate::Climate *c)               { flow_climate_z2_ = c; }

  // Globals
  void set_ui_use_room_z1(esphome::globals::RestoringGlobalsComponent<bool> *g) { ui_use_room_z1_ = g; }
  void set_ui_use_room_z2(esphome::globals::RestoringGlobalsComponent<bool> *g) { ui_use_room_z2_ = g; }

  // Solver
  void set_sw_use_solver(switch_::Switch *s) { sw_use_solver_ = s; }
  void set_bin_solver_connected(binary_sensor::BinarySensor *b) { bin_solver_connected_ = b; }
  void set_txt_solver_ip(text_sensor::TextSensor *t) { txt_solver_ip_ = t; }
  void set_solver_kwh_meter_feedback_source(select::Select *s) { solver_kwh_meter_feedback_source_ = s; }
  void set_solver_kwh_meter_feedback(number::Number *n) { solver_kwh_meter_feedback_ = n; }

  void set_num_raw_heat_produced(number::Number *n) { num_raw_heat_produced_ = n; }
  void set_num_raw_elec_consumed(number::Number *n) { num_raw_elec_consumed_ = n; }
  void set_num_raw_runtime_hours(number::Number *n) { num_raw_runtime_hours_ = n; }
  void set_num_raw_avg_outside_temp(number::Number *n) { num_raw_avg_outside_temp_ = n; }
  void set_num_raw_avg_room_temp(number::Number *n) { num_raw_avg_room_temp_ = n; }
  void set_num_raw_delta_room_temp(number::Number *n) { num_raw_delta_room_temp_ = n; }
  void set_num_raw_max_output(number::Number *n) { num_raw_max_output_ = n; }
  void set_num_raw_hl_tm_product(number::Number *n) { num_raw_hl_tm_product_ = n; }

  void set_num_battery_soc_kwh(number::Number *n) { num_battery_soc_kwh_ = n; }
  void set_num_battery_max_discharge_kw(number::Number *n) { num_battery_max_discharge_kw_ = n; }

  // AsyncWebHandler
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

    // Solver run stats populated from YAML after each solve
  struct LastRunStats {
      uint32_t execution_ms{0};
      float heat_loss{0.0f}, base_cop{0.0f}, thermal_mass{0.0f};
      float exp_consumption{0.0f}, exp_production{0.0f}, exp_solar{0.0f};
      float total_cost{0.0f}, total_cost_tax{0.0f};
  } last_run_stats_;

  // Called from YAML after each successful solver response
  void store_odin_data(int current_hour, int current_day,
                       const std::vector<float>& sched,
                       const std::vector<float>& energy,
                       const std::vector<float>& production,
                       const std::vector<float>& exp_temp,
                       const std::vector<float>& cost,
                       const std::vector<float>& cost_tax,
                       const std::vector<float>& battery_discharge,
                       const LastRunStats& run_stats);

  // Called each hour by YAML to track actual consumption and room temp per-hour slot
  void update_actual_data(int hour, float actual_cons_kwh, float actual_room_temp);

 protected:
  void handle_root_(AsyncWebServerRequest *request);
  void handle_state_(AsyncWebServerRequest *request);
  void handle_set_(AsyncWebServerRequest *request);
  void handle_odin_request_(AsyncWebServerRequest *request);
  void dispatch_set_(const std::string &key, const std::string &sval, float fval, bool is_string);
  std::vector<DashboardAction> action_queue_;
  SemaphoreHandle_t action_lock_ = NULL;


  // Members 
  web_server_base::WebServerBase *base_{nullptr};

  // Sensors
  sensor::Sensor *hp_feed_temp_{nullptr};
  sensor::Sensor *hp_return_temp_{nullptr};
  sensor::Sensor *outside_temp_{nullptr};
  sensor::Sensor *compressor_frequency_{nullptr};
  sensor::Sensor *flow_rate_{nullptr};
  sensor::Sensor *computed_output_power_{nullptr};
  sensor::Sensor *daily_computed_output_power_{nullptr};
  sensor::Sensor *daily_total_energy_consumption_{nullptr};
  sensor::Sensor *compressor_starts_{nullptr};
  sensor::Sensor *runtime_{nullptr};
  sensor::Sensor *wifi_signal_db_{nullptr};
  sensor::Sensor *dhw_temp_{nullptr};
  sensor::Sensor *dhw_flow_temp_target_{nullptr};
  sensor::Sensor *dhw_flow_temp_drop_{nullptr};
  sensor::Sensor *dhw_consumed_{nullptr};
  sensor::Sensor *dhw_delivered_{nullptr};
  sensor::Sensor *dhw_cop_{nullptr};
  sensor::Sensor *heating_consumed_{nullptr};
  sensor::Sensor *heating_produced_{nullptr};
  sensor::Sensor *heating_cop_{nullptr};
  sensor::Sensor *cooling_consumed_{nullptr};
  sensor::Sensor *cooling_produced_{nullptr};
  sensor::Sensor *cooling_cop_{nullptr};

  sensor::Sensor *z1_flow_temp_target_{nullptr};
  sensor::Sensor *z2_flow_temp_target_{nullptr};

  sensor::Sensor *auto_adaptive_setpoint_bias_{nullptr};
  sensor::Sensor *maximum_heating_flow_temp_{nullptr};
  sensor::Sensor *minimum_heating_flow_temp_{nullptr};
  sensor::Sensor *thermostat_hysteresis_z1_{nullptr};
  sensor::Sensor *thermostat_hysteresis_z2_{nullptr};

  sensor::Sensor *operation_mode_{nullptr};

  // Binary sensors
  binary_sensor::BinarySensor *status_compressor_{nullptr};
  binary_sensor::BinarySensor *status_booster_{nullptr};
  binary_sensor::BinarySensor *status_defrost_{nullptr};
  binary_sensor::BinarySensor *status_water_pump_{nullptr};
  binary_sensor::BinarySensor *status_in1_request_{nullptr};
  binary_sensor::BinarySensor *status_in6_request_{nullptr};
  binary_sensor::BinarySensor *status_zone2_enabled_{nullptr};

  // Text sensors
  text_sensor::TextSensor *version_{nullptr};
  
  // Switches
  switch_::Switch *sw_auto_adaptive_{nullptr};
  switch_::Switch *sw_defrost_mit_{nullptr};
  switch_::Switch *sw_smart_boost_{nullptr};
  switch_::Switch *sw_force_dhw_{nullptr}; 
  switch_::Switch *pred_sc_switch_{nullptr};

  // Selects
  select::Select *sel_heating_system_type_{nullptr};
  select::Select *sel_room_temp_source_z1_{nullptr};
  select::Select *sel_room_temp_source_z2_{nullptr};
  select::Select *sel_operating_mode_z1_{nullptr};
  select::Select *sel_operating_mode_z2_{nullptr};
  select::Select *sel_temp_source_z1_{nullptr};
  select::Select *sel_temp_source_z2_{nullptr};

  // Numbers
  number::Number *num_aa_setpoint_bias_{nullptr};
  number::Number *num_max_flow_temp_{nullptr};
  number::Number *num_min_flow_temp_{nullptr};
  number::Number *num_max_flow_temp_z2_{nullptr};
  number::Number *num_min_flow_temp_z2_{nullptr};
  number::Number *num_hysteresis_z1_{nullptr};
  number::Number *num_hysteresis_z2_{nullptr};
  number::Number *pred_sc_time_{nullptr};
  number::Number *pred_sc_delta_{nullptr};

  // cooling settings
  number::Number *num_cooling_smart_start_z1_{nullptr};
  number::Number *num_min_cooling_flow_z1_{nullptr};

  // Climate
  climate::Climate *dhw_climate_{nullptr};
  climate::Climate *virtual_climate_z1_{nullptr};
  climate::Climate *virtual_climate_z2_{nullptr};
  climate::Climate *heatpump_climate_z1_{nullptr};
  climate::Climate *heatpump_climate_z2_{nullptr};
  climate::Climate *flow_climate_z1_{nullptr};
  climate::Climate *flow_climate_z2_{nullptr};

  esphome::globals::RestoringGlobalsComponent<bool> *ui_use_room_z1_{nullptr};
  esphome::globals::RestoringGlobalsComponent<bool> *ui_use_room_z2_{nullptr};

  // Solver
  switch_::Switch *sw_use_solver_{nullptr};
  binary_sensor::BinarySensor *bin_solver_connected_{nullptr};
  text_sensor::TextSensor *txt_solver_ip_{nullptr};
  select::Select *solver_kwh_meter_feedback_source_{nullptr};
  number::Number *solver_kwh_meter_feedback_{nullptr};

  number::Number *num_raw_heat_produced_{nullptr};
  number::Number *num_raw_elec_consumed_{nullptr};
  number::Number *num_raw_runtime_hours_{nullptr};
  number::Number *num_raw_avg_outside_temp_{nullptr};
  number::Number *num_raw_avg_room_temp_{nullptr};
  number::Number *num_raw_delta_room_temp_{nullptr};
  number::Number *num_raw_max_output_{nullptr};
  number::Number *num_raw_hl_tm_product_{nullptr};

  number::Number *num_battery_soc_kwh_{nullptr};
  number::Number *num_battery_max_discharge_kw_{nullptr};


private:
  static const size_t MAX_HISTORY = 1440; // 24h, 1min interval
  HistoryRecord history_buffer_[MAX_HISTORY];
  size_t history_head_{0};
  size_t history_count_{0};
  SemaphoreHandle_t history_mutex_{NULL};
  uint32_t last_history_time_{0};

  // snapshot data to avoid concurrency issues
  DashboardSnapshot current_snapshot_;
  SemaphoreHandle_t snapshot_mutex_ = NULL;
  uint32_t last_snapshot_time_{0};
  void update_snapshot_();

  // solver
  std::vector<float> odin_schedule_;
  std::vector<float> odin_energy_;
  std::vector<float> odin_production_;     // heat kWh produced per hour
  std::vector<float> odin_expected_temp_;
  std::vector<float> odin_cost_;
  std::vector<float> odin_cost_tax_;
  std::vector<float> odin_battery_discharge_;
  std::vector<float> odin_actual_cons_;    // actual kWh consumed per hour (NVS persisted)
  std::vector<float> odin_actual_room_;    // actual room temp at start of each hour (NVS persisted)
  bool odin_data_ready_{false};
  int odin_stored_day_{-1};
  bool odin_nvs_dirty_{false};
  uint32_t odin_nvs_last_write_ms_{0};

  void record_history_();
  void nvs_persist_odin_();
  void nvs_load_odin_();
  void handle_history_request_(AsyncWebServerRequest *request);
  void handle_js_(AsyncWebServerRequest *request);
  void send_chunked_(AsyncWebServerRequest *request, const char *content_type, const uint8_t *data, size_t length, const char *cache_control);
  static int16_t pack_temp_(float val);
  static bool bin_state_(binary_sensor::BinarySensor *b);
};

}  // namespace asgard_dashboard
}  // namespace esphome
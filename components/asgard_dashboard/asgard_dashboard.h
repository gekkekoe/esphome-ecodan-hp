#pragma once
#include <vector> 
#include <mutex>

#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace asgard_dashboard {

struct DashboardAction {
  std::string key;
  std::string s_value;
  float f_value;
  bool is_string;
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
  void set_status_operation(text_sensor::TextSensor *t)       { status_operation_ = t; }
  void set_heating_system_type(text_sensor::TextSensor *t)    { heating_system_type_ = t; }
  void set_room_temp_source_z1(text_sensor::TextSensor *t)    { room_temp_source_z1_ = t; }
  void set_room_temp_source_z2(text_sensor::TextSensor *t)    { room_temp_source_z2_ = t; }
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

  // Climate
  void set_dhw_climate(climate::Climate *c)                   { dhw_climate_ = c; }
  void set_virtual_climate_z1(climate::Climate *c)            { virtual_climate_z1_ = c; }
  void set_virtual_climate_z2(climate::Climate *c)            { virtual_climate_z2_ = c; }
  void set_heatpump_climate_z1(climate::Climate *c)           { heatpump_climate_z1_ = c; }
  void set_heatpump_climate_z2(climate::Climate *c)           { heatpump_climate_z2_ = c; }

  // AsyncWebHandler
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }

 protected:
  void handle_root_(AsyncWebServerRequest *request);
  void handle_state_(AsyncWebServerRequest *request);
  void handle_set_(AsyncWebServerRequest *request);
  void dispatch_set_(const std::string &key, const std::string &sval, float fval, bool is_string);

  // JSON helpers
  static std::string sensor_str_(sensor::Sensor *s);
  static std::string climate_current_str_(climate::Climate *c);
  static std::string climate_target_str_(climate::Climate *c);
  static std::string climate_action_str_(climate::Climate *c);
  static std::string number_str_(number::Number *n);
  static const char *bin_str_(binary_sensor::BinarySensor *b);
  static bool bin_state_(binary_sensor::BinarySensor *b);
  static std::string text_val_(text_sensor::TextSensor *t);
  
  static std::string select_idx_(select::Select *s); 
  static std::string select_str_(select::Select *s);

  static std::string sw_str_(switch_::Switch *sw);
  
  static std::string number_traits_(number::Number *n);
  std::vector<DashboardAction> action_queue_;
  std::mutex action_lock_;

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

  // Binary sensors
  binary_sensor::BinarySensor *status_compressor_{nullptr};
  binary_sensor::BinarySensor *status_booster_{nullptr};
  binary_sensor::BinarySensor *status_defrost_{nullptr};
  binary_sensor::BinarySensor *status_water_pump_{nullptr};
  binary_sensor::BinarySensor *status_in1_request_{nullptr};
  binary_sensor::BinarySensor *status_in6_request_{nullptr};
  binary_sensor::BinarySensor *status_zone2_enabled_{nullptr};

  // Text sensors
  text_sensor::TextSensor *status_operation_{nullptr};
  text_sensor::TextSensor *heating_system_type_{nullptr};
  text_sensor::TextSensor *room_temp_source_z1_{nullptr};
  text_sensor::TextSensor *room_temp_source_z2_{nullptr};
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

  // Climate
  climate::Climate *dhw_climate_{nullptr};
  climate::Climate *virtual_climate_z1_{nullptr};
  climate::Climate *virtual_climate_z2_{nullptr};
  climate::Climate *heatpump_climate_z1_{nullptr};
  climate::Climate *heatpump_climate_z2_{nullptr};

};

}  // namespace asgard_dashboard
}  // namespace esphome
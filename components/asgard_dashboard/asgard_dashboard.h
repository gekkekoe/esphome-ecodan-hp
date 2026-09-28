#pragma once
#include <array>
#include <vector>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
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
#include "esphome/components/text/text.h"
#include "esphome/components/button/button.h"

#include "asgard_lfs.h"

// forward decl
namespace esphome {
namespace ecodan {
  class EcodanHeatpump;
}
}

namespace esphome {
namespace asgard_dashboard {

struct DashboardAction {
  std::string key;
  std::string s_value;
  float f_value;
  bool is_string;
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
  bool sw_regular_dhw{false};
  bool status_short_cycle_lockout{false};

  // Float sensors
  float hp_feed_temp{NAN};
  float hp_return_temp{NAN};
  float outside_temp{NAN};
  float liquid_pipe_temp{NAN};
  float condensing_temp{NAN};
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

  // Legionella DHW setpoint automation
  bool sw_legionella_enable{false};
  float legionella_saved_dhw_setpoint{NAN};

  float heating_consumed{NAN};
  float heating_produced{NAN};
  float heating_cop{NAN};
  float cooling_consumed{NAN};
  float cooling_produced{NAN};
  float cooling_cop{NAN};

  float z1_flow_temp_target{NAN};
  float z2_flow_temp_target{NAN};

  bool sw_power_mode{false};

  // Number sensors with limits
  struct NumData { float val{NAN}; float min{NAN}; float max{NAN}; float step{NAN}; };
  NumData num_aa_setpoint_bias;
  NumData num_max_flow_temp;
  NumData num_min_flow_temp;
  NumData num_max_flow_temp_z2;
  NumData num_min_flow_temp_z2;
  NumData num_hysteresis_z1;
  NumData num_hysteresis_z2;
  NumData num_min_compressor_on_time;

  // Cooling settings
  NumData num_cooling_smart_start_z1;
  NumData num_min_cooling_flow_z1;
  NumData num_min_cooling_flow_z2;

  // HA / REST API room temperature feedback (per zone)
  NumData num_temperature_feedback_z1;
  NumData num_temperature_feedback_z2;


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
  int sel_lockout_duration{-1};
  int sel_lockout_strategy{-1};

  // Text sensors (fixed-size to avoid heap allocation in snapshot)
  char version[32]{0};
  char local_ip[16]{0};


  // Server control
  bool sw_server_control{false};
  bool sw_sc_prohibit_dhw{false};
  bool sw_sc_prohibit_z1_heating{false};
  bool sw_sc_prohibit_z1_cooling{false};
  bool sw_sc_prohibit_z2_heating{false};
  bool sw_sc_prohibit_z2_cooling{false};

  // Service codes & Holiday
  bool sw_service_codes_enabled{false};
  bool sw_holiday_mode{false};

  NumData num_legionella_dhw_setpoint;


  // Odin MQTT config (System tab)
  char txt_mqtt_broker_ip[32]{0};
  char txt_mqtt_broker_port[8]{0};
  char txt_mqtt_user[32]{0};
  char txt_mqtt_password[32]{0};
  char txt_mqtt_topic_prefix[32]{0};
};

class EcodanDashboard : public Component, public AsyncWebHandler {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  // Ecodan instance
  void set_ecodan(ecodan::EcodanHeatpump *e) { ecodan_ = e; }
  // Web server
  void set_web_server_base(web_server_base::WebServerBase *b) { base_ = b; }

  // Sensors
  void set_hp_feed_temp(sensor::Sensor *s)                    { hp_feed_temp_ = s; }
  void set_hp_return_temp(sensor::Sensor *s)                  { hp_return_temp_ = s; }
  void set_outside_temp(sensor::Sensor *s)                    { outside_temp_ = s; }
  void set_liquid_pipe_temp(sensor::Sensor *s)               { liquid_pipe_temp_ = s; }
  void set_condensing_temp(sensor::Sensor *s)                { condensing_temp_ = s; }
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

  // Flow temp targets
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
  void set_status_short_cycle_lockout(binary_sensor::BinarySensor *b) { status_short_cycle_lockout_ = b; }

  // Text sensors
  void set_version(text_sensor::TextSensor *t)                { version_ = t; }

  // Switches
  void set_sw_auto_adaptive(switch_::Switch *s)               { sw_auto_adaptive_ = s; }
  void set_sw_defrost_mit(switch_::Switch *s)                 { sw_defrost_mit_ = s; }
  void set_sw_smart_boost(switch_::Switch *s)                 { sw_smart_boost_ = s; }
  void set_sw_force_dhw(switch_::Switch *s)                   { sw_force_dhw_ = s; }
  void set_sw_regular_dhw(switch_::Switch *s)                 { sw_regular_dhw_ = s; }
  void set_pred_sc_switch(switch_::Switch *s)                 { pred_sc_switch_ = s; }
  void set_sw_power_mode(switch_::Switch *s)                  { sw_power_mode_ = s; }
  void set_sw_service_codes_enabled(switch_::Switch *s)        { sw_service_codes_enabled_ = s; }
  void set_sw_holiday_mode(switch_::Switch *s)                 { sw_holiday_mode_ = s; }
  void set_sw_legionella_dhw_automation(switch_::Switch *s)    { sw_legionella_dhw_automation_ = s; }

  // Server control
  void set_sw_server_control(switch_::Switch *s)              { sw_server_control_ = s; }
  void set_sw_sc_prohibit_dhw(switch_::Switch *s)             { sw_sc_prohibit_dhw_ = s; }
  void set_sw_sc_prohibit_z1_heating(switch_::Switch *s)      { sw_sc_prohibit_z1_heating_ = s; }
  void set_sw_sc_prohibit_z1_cooling(switch_::Switch *s)      { sw_sc_prohibit_z1_cooling_ = s; }
  void set_sw_sc_prohibit_z2_heating(switch_::Switch *s)      { sw_sc_prohibit_z2_heating_ = s; }
  void set_sw_sc_prohibit_z2_cooling(switch_::Switch *s)      { sw_sc_prohibit_z2_cooling_ = s; }

  // Selects
  void set_sel_heating_system_type(select::Select *s)         { sel_heating_system_type_ = s; }
  void set_sel_room_temp_source_z1(select::Select *s)         { sel_room_temp_source_z1_ = s; }
  void set_sel_room_temp_source_z2(select::Select *s)         { sel_room_temp_source_z2_ = s; }
  void set_sel_operating_mode_z1(select::Select *s)           { sel_operating_mode_z1_ = s; }
  void set_sel_operating_mode_z2(select::Select *s)           { sel_operating_mode_z2_ = s; }
  void set_sel_temp_source_z1(select::Select *s)              { sel_temp_source_z1_ = s; }
  void set_sel_temp_source_z2(select::Select *s)              { sel_temp_source_z2_ = s; }
  void set_lockout_duration(select::Select *s)                { lockout_duration_ = s; }
  void set_lockout_strategy(select::Select *s)                { lockout_strategy_ = s; }

  // Numbers
  void set_num_aa_setpoint_bias(number::Number *n)            { num_aa_setpoint_bias_ = n; }
  void set_num_max_flow_temp(number::Number *n)               { num_max_flow_temp_ = n; }
  void set_num_min_flow_temp(number::Number *n)               { num_min_flow_temp_ = n; }
  void set_num_max_flow_temp_z2(number::Number *n)            { num_max_flow_temp_z2_ = n; }
  void set_num_min_flow_temp_z2(number::Number *n)            { num_min_flow_temp_z2_ = n; }
  void set_num_hysteresis_z1(number::Number *n)               { num_hysteresis_z1_ = n; }
  void set_num_hysteresis_z2(number::Number *n)               { num_hysteresis_z2_ = n; }
  void set_num_legionella_dhw_setpoint(number::Number *n)      { num_legionella_dhw_setpoint_ = n; }
  void set_minimum_compressor_on_time(number::Number *n)      { minimum_compressor_on_time_ = n; }

  // Cooling settings (numbers)
  void set_num_cooling_smart_start_z1(number::Number *n)      { num_cooling_smart_start_z1_ = n; }
  void set_num_min_cooling_flow_z1(number::Number *n)         { num_min_cooling_flow_z1_ = n; }
  void set_num_min_cooling_flow_z2(number::Number *n)         { num_min_cooling_flow_z2_ = n; }
  void set_num_temperature_feedback_z1(number::Number *n)     { num_temperature_feedback_z1_ = n; }
  void set_num_temperature_feedback_z2(number::Number *n)     { num_temperature_feedback_z2_ = n; }

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
  void set_legionella_saved_dhw_setpoint(esphome::globals::RestoringGlobalsComponent<float> *g) { legionella_saved_dhw_setpoint_ = g; }

  // Buttons
  void set_short_cycle_mitigation_button(button::Button *b)   { short_cycle_mitigation_button_ = b; }
  void set_reboot_button(button::Button *b)                   { reboot_button_ = b; }

  void set_txt_mqtt_broker_ip(text::Text *t)                  { txt_mqtt_broker_ip_ = t; }
  void set_txt_mqtt_broker_port(text::Text *t)                { txt_mqtt_broker_port_ = t; }
  void set_txt_mqtt_user(text::Text *t)                       { txt_mqtt_user_ = t; }
  void set_txt_mqtt_password(text::Text *t)                   { txt_mqtt_password_ = t; }
  void set_txt_mqtt_topic_prefix(text::Text *t)               { txt_mqtt_topic_prefix_ = t; }
  void set_solver_kwh_meter_feedback_source(select::Select *s) { solver_kwh_meter_feedback_source_ = s; }
  void set_solver_kwh_meter_feedback(number::Number *n)       { solver_kwh_meter_feedback_ = n; }

  void set_num_raw_heat_produced(number::Number *n)           { num_raw_heat_produced_ = n; }
  void set_num_raw_elec_consumed(number::Number *n)           { num_raw_elec_consumed_ = n; }
  void set_num_raw_runtime_hours(number::Number *n)           { num_raw_runtime_hours_ = n; }
  void set_num_raw_avg_outside_temp(number::Number *n)        { num_raw_avg_outside_temp_ = n; }
  void set_num_raw_avg_room_temp(number::Number *n)           { num_raw_avg_room_temp_ = n; }
  void set_num_raw_delta_room_temp(number::Number *n)         { num_raw_delta_room_temp_ = n; }
  void set_num_raw_hl_tm_product(number::Number *n)           { num_raw_hl_tm_product_ = n; }
  void set_num_raw_solar_factor(number::Number *n)            { num_raw_solar_factor_ = n; }
  void set_num_raw_heat_produced_z2(number::Number *n)    { num_raw_heat_produced_z2_ = n; }
  void set_num_raw_elec_consumed_z2(number::Number *n)    { num_raw_elec_consumed_z2_ = n; }
  void set_num_raw_runtime_hours_z2(number::Number *n)    { num_raw_runtime_hours_z2_ = n; }
  void set_num_raw_avg_room_temp_z2(number::Number *n)    { num_raw_avg_room_temp_z2_ = n; }
  void set_num_raw_delta_room_temp_z2(number::Number *n)  { num_raw_delta_room_temp_z2_ = n; }

  void set_num_raw_cool_produced(number::Number *v)           { num_raw_cool_produced_ = v; }
  void set_num_raw_cool_elec_consumed(number::Number *v)      { num_raw_cool_elec_consumed_ = v; }
  void set_num_raw_cool_runtime_hours(number::Number *v)      { num_raw_cool_runtime_hours_ = v; }
  void set_num_raw_cool_avg_outside_temp(number::Number *v)   { num_raw_cool_avg_outside_temp_ = v; }
  void set_num_raw_cool_avg_room_temp(number::Number *v)      { num_raw_cool_avg_room_temp_ = v; }
  void set_num_raw_cool_produced_z2(number::Number *n)    { num_raw_cool_produced_z2_ = n; }
  void set_num_raw_cool_elec_consumed_z2(number::Number *n) { num_raw_cool_elec_consumed_z2_ = n; }
  void set_num_raw_cool_runtime_hours_z2(number::Number *n) { num_raw_cool_runtime_hours_z2_ = n; }
  void set_num_raw_cool_avg_room_temp_z2(number::Number *n) { num_raw_cool_avg_room_temp_z2_ = n; }


  // AsyncWebHandler
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return false; }
  // esphome's web_server_idf framework fully drains the body of any POST
  // whose Content-Type isn't application/x-www-form-urlencoded or
  // multipart/form-data (e.g. our application/json) via this callback
  // *before* calling handleRequest()
  void handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) override;


 protected:
  void handle_root_(AsyncWebServerRequest *request);
  void handle_setup_(AsyncWebServerRequest *request);
  void handle_state_(AsyncWebServerRequest *request);
  void handle_set_(AsyncWebServerRequest *request);
  void dispatch_set_(const std::string &key, const std::string &sval, float fval, bool is_string);

  std::vector<DashboardAction> action_queue_;
  SemaphoreHandle_t action_lock_ = NULL;

  // Accumulates the raw POST body across handleBody() calls (see comment on
  // handleBody() above); consumed and cleared by handle_set_().
  std::string post_body_;
  bool post_body_oversized_{false};

  // Component
  ecodan::EcodanHeatpump *ecodan_{nullptr};

  // Members
  web_server_base::WebServerBase *base_{nullptr};

  // Sensors
  sensor::Sensor *hp_feed_temp_{nullptr};
  sensor::Sensor *hp_return_temp_{nullptr};
  sensor::Sensor *outside_temp_{nullptr};
  sensor::Sensor *liquid_pipe_temp_{nullptr};
  sensor::Sensor *condensing_temp_{nullptr};
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
  binary_sensor::BinarySensor *status_short_cycle_lockout_{nullptr};

  // Text sensors
  text_sensor::TextSensor *version_{nullptr};

  // Switches
  switch_::Switch *sw_auto_adaptive_{nullptr};
  switch_::Switch *sw_defrost_mit_{nullptr};
  switch_::Switch *sw_smart_boost_{nullptr};
  switch_::Switch *sw_force_dhw_{nullptr};
  switch_::Switch *sw_regular_dhw_{nullptr};
  switch_::Switch *pred_sc_switch_{nullptr};
  switch_::Switch *sw_power_mode_{nullptr};
  switch_::Switch *sw_service_codes_enabled_{nullptr};
  switch_::Switch *sw_holiday_mode_{nullptr};
  switch_::Switch *sw_legionella_dhw_automation_{nullptr};

  // Selects
  select::Select *sel_heating_system_type_{nullptr};
  select::Select *sel_room_temp_source_z1_{nullptr};
  select::Select *sel_room_temp_source_z2_{nullptr};
  select::Select *sel_operating_mode_z1_{nullptr};
  select::Select *sel_operating_mode_z2_{nullptr};
  select::Select *sel_temp_source_z1_{nullptr};
  select::Select *sel_temp_source_z2_{nullptr};
  select::Select *lockout_duration_{nullptr};
  select::Select *lockout_strategy_{nullptr};

  // Numbers
  number::Number *num_aa_setpoint_bias_{nullptr};
  number::Number *num_max_flow_temp_{nullptr};
  number::Number *num_min_flow_temp_{nullptr};
  number::Number *num_max_flow_temp_z2_{nullptr};
  number::Number *num_min_flow_temp_z2_{nullptr};
  number::Number *num_hysteresis_z1_{nullptr};
  number::Number *num_hysteresis_z2_{nullptr};
  number::Number *num_cooling_smart_start_z1_{nullptr};
  number::Number *num_min_cooling_flow_z1_{nullptr};
  number::Number *num_min_cooling_flow_z2_{nullptr};
  number::Number *num_temperature_feedback_z1_{nullptr};
  number::Number *num_temperature_feedback_z2_{nullptr};
  number::Number *minimum_compressor_on_time_{nullptr};

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
  esphome::globals::RestoringGlobalsComponent<float> *legionella_saved_dhw_setpoint_{nullptr};


  // Server control switches
  switch_::Switch *sw_server_control_{nullptr};
  switch_::Switch *sw_sc_prohibit_dhw_{nullptr};
  switch_::Switch *sw_sc_prohibit_z1_heating_{nullptr};
  switch_::Switch *sw_sc_prohibit_z1_cooling_{nullptr};
  switch_::Switch *sw_sc_prohibit_z2_heating_{nullptr};
  switch_::Switch *sw_sc_prohibit_z2_cooling_{nullptr};
  text::Text *txt_mqtt_broker_ip_{nullptr};
  text::Text *txt_mqtt_broker_port_{nullptr};
  text::Text *txt_mqtt_user_{nullptr};
  text::Text *txt_mqtt_password_{nullptr};
  text::Text *txt_mqtt_topic_prefix_{nullptr};
  select::Select *solver_kwh_meter_feedback_source_{nullptr};
  number::Number *solver_kwh_meter_feedback_{nullptr};

  number::Number *num_raw_heat_produced_{nullptr};
  number::Number *num_raw_elec_consumed_{nullptr};
  number::Number *num_raw_runtime_hours_{nullptr};
  number::Number *num_raw_avg_outside_temp_{nullptr};
  number::Number *num_raw_avg_room_temp_{nullptr};
  number::Number *num_raw_delta_room_temp_{nullptr};
  number::Number *num_raw_hl_tm_product_{nullptr};
  number::Number *num_raw_solar_factor_{nullptr};
  number::Number *num_raw_heat_produced_z2_{nullptr};
  number::Number *num_raw_elec_consumed_z2_{nullptr};
  number::Number *num_raw_runtime_hours_z2_{nullptr};
  number::Number *num_raw_avg_room_temp_z2_{nullptr};
  number::Number *num_raw_delta_room_temp_z2_{nullptr};
  number::Number *num_raw_cool_produced_{nullptr};
  number::Number *num_raw_cool_elec_consumed_{nullptr};
  number::Number *num_raw_cool_runtime_hours_{nullptr};
  number::Number *num_raw_cool_avg_outside_temp_{nullptr};
  number::Number *num_raw_cool_avg_room_temp_{nullptr};
  number::Number *num_raw_cool_produced_z2_{nullptr};
  number::Number *num_raw_cool_elec_consumed_z2_{nullptr};
  number::Number *num_raw_cool_runtime_hours_z2_{nullptr};
  number::Number *num_raw_cool_avg_room_temp_z2_{nullptr};
  number::Number *num_legionella_dhw_setpoint_{nullptr};

  // Buttons
  button::Button *short_cycle_mitigation_button_{nullptr};
  button::Button *reboot_button_{nullptr};

 private:

  // ── LittleFS history buffers ──────────────────────────────────────────────

  // Minute-record RAM buffers (batch writes to reduce flash wear)
  MinuteRecord lfs_write_buf_[LFS_FLUSH_COUNT];
  size_t       lfs_write_buf_count_{0};

  MinuteRecord lfs_flush_snap_[LFS_FLUSH_COUNT]; // snapshot handed to lfs_task_
  size_t       lfs_flush_snap_count_{0};
  size_t       lfs_flush_write_pos_{0};

  MinuteRecord http_wb_snap_[LFS_FLUSH_COUNT];   // read-side snapshot for HTTP
  size_t       http_wb_count_{0};

  int16_t last_hist_prod_{-32768};

  size_t            history_head_{0};
  size_t            history_count_{0};
  SemaphoreHandle_t history_mutex_{NULL};
  uint32_t          last_history_time_{0};

  size_t hourly_head_{0};
  size_t hourly_count_{0};

  void setup_lfs();
  static void lfs_task_(void* arg);

  TaskHandle_t      lfs_task_handle_{nullptr};
  SemaphoreHandle_t lfs_trigger_{nullptr};

  bool              lfs_mounted_{false}; 

  // ── Snapshot (thread-safe sensor cache) ──────────────────────────────────

  DashboardSnapshot current_snapshot_;
  SemaphoreHandle_t snapshot_mutex_ = NULL;
  uint32_t          last_snapshot_time_{0};
  void update_snapshot_();



  // ── HTTP helpers ──────────────────────────────────────────────────────────

  void record_history_();
  void handle_history_request_(AsyncWebServerRequest *request);
  void send_hourly_history_(httpd_req_t *req, uint32_t from_ts, uint32_t to_ts);
  void send_minute_history_(httpd_req_t *req, uint32_t from_ts, uint32_t to_ts);
  void handle_export_hourly_(AsyncWebServerRequest *request);
  void handle_export_physics_(AsyncWebServerRequest *request);
  void handle_js_(AsyncWebServerRequest *request);
  void send_chunked_(AsyncWebServerRequest *request, const char *content_type,
                     const uint8_t *data, size_t length, const char *cache_control);
  static int16_t pack_temp_(float val);
  static bool bin_state_(binary_sensor::BinarySensor *b);

  time_t timestamp() const;
};

} // namespace asgard_dashboard
} // namespace esphome
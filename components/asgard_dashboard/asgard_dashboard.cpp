#include "asgard_dashboard.h"
#include "dashboard_html.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_http_server.h>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace esphome {
namespace asgard_dashboard {

static const char *const TAG = "asgard_dashboard";

void EcodanDashboard::setup() {
  ESP_LOGI(TAG, "Setting up Ecodan Dashboard on /dashboard");
  base_->init();
  base_->add_handler(this);
}

void EcodanDashboard::loop() {
  if (action_queue_.empty()) return;

  std::vector<DashboardAction> todo;
  {
    std::lock_guard<std::mutex> lock(action_lock_);
    todo = action_queue_;
    action_queue_.clear();
  }

  for (const auto &act : todo) {
    this->dispatch_set_(act.key, act.s_value, act.f_value, act.is_string);
  }
}

bool EcodanDashboard::canHandle(AsyncWebServerRequest *request) const {
  const auto& url = request->url();
  return (url == "/dashboard" || url == "/dashboard/" ||
          url == "/dashboard/state" || url == "/dashboard/set");
}

void EcodanDashboard::handleRequest(AsyncWebServerRequest *request) {
  const auto& url = request->url();
  if      (url == "/dashboard" || url == "/dashboard/") handle_root_(request);
  else if (url == "/dashboard/state")                   handle_state_(request);
  else if (url == "/dashboard/set")                     handle_set_(request);
  else                                                  request->send(404, "text/plain", "Not found");
}

void EcodanDashboard::handle_root_(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(
      200, "text/html", DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void EcodanDashboard::handle_state_(AsyncWebServerRequest *request) {
  std::string j;
  j.reserve(1500);
  j += "{";

  j += "\"hp_feed_temp\":"                     + sensor_str_(hp_feed_temp_) + ",";
  j += "\"hp_return_temp\":"                   + sensor_str_(hp_return_temp_) + ",";
  j += "\"outside_temp\":"                     + sensor_str_(outside_temp_) + ",";
  j += "\"compressor_frequency\":"             + sensor_str_(compressor_frequency_) + ",";
  j += "\"flow_rate\":"                        + sensor_str_(flow_rate_) + ",";
  j += "\"computed_output_power\":"            + sensor_str_(computed_output_power_) + ",";
  j += "\"daily_computed_output_power\":"      + sensor_str_(daily_computed_output_power_) + ",";
  j += "\"daily_total_energy_consumption\":"   + sensor_str_(daily_total_energy_consumption_) + ",";
  j += "\"compressor_starts\":"               + sensor_str_(compressor_starts_) + ",";
  j += "\"runtime\":"                         + sensor_str_(runtime_) + ",";
  j += "\"wifi_signal_db\":"                  + sensor_str_(wifi_signal_db_) + ",";

  j += "\"dhw_temp\":"                        + sensor_str_(dhw_temp_) + ",";
  j += "\"dhw_flow_temp_target\":"            + sensor_str_(dhw_flow_temp_target_) + ",";
  j += "\"dhw_flow_temp_drop\":"              + sensor_str_(dhw_flow_temp_drop_) + ",";
  j += "\"dhw_consumed\":"                    + sensor_str_(dhw_consumed_) + ",";
  j += "\"dhw_delivered\":"                   + sensor_str_(dhw_delivered_) + ",";
  j += "\"dhw_cop\":"                         + sensor_str_(dhw_cop_) + ",";

  j += "\"heating_consumed\":"                + sensor_str_(heating_consumed_) + ",";
  j += "\"heating_produced\":"                + sensor_str_(heating_produced_) + ",";
  j += "\"heating_cop\":"                     + sensor_str_(heating_cop_) + ",";
  j += "\"cooling_consumed\":"                + sensor_str_(cooling_consumed_) + ",";
  j += "\"cooling_produced\":"                + sensor_str_(cooling_produced_) + ",";
  j += "\"cooling_cop\":"                     + sensor_str_(cooling_cop_) + ",";

  j += "\"z1_flow_temp_target\":"             + sensor_str_(z1_flow_temp_target_) + ",";
  j += "\"z2_flow_temp_target\":"             + sensor_str_(z2_flow_temp_target_) + ",";

  j += "\"auto_adaptive_setpoint_bias\":"     + number_str_(num_aa_setpoint_bias_) + ",";
  j += "\"aa_bias_lim\":"                     + number_traits_(num_aa_setpoint_bias_) + ",";
  
  j += "\"maximum_heating_flow_temp\":"       + number_str_(num_max_flow_temp_) + ",";
  j += "\"max_flow_lim\":"                    + number_traits_(num_max_flow_temp_) + ",";
  j += "\"minimum_heating_flow_temp\":"       + number_str_(num_min_flow_temp_) + ",";
  j += "\"min_flow_lim\":"                    + number_traits_(num_min_flow_temp_) + ",";

  j += "\"maximum_heating_flow_temp_z2\":"    + number_str_(num_max_flow_temp_z2_) + ",";
  j += "\"max_flow_z2_lim\":"                 + number_traits_(num_max_flow_temp_z2_) + ",";
  j += "\"minimum_heating_flow_temp_z2\":"    + number_str_(num_min_flow_temp_z2_) + ",";
  j += "\"min_flow_z2_lim\":"                 + number_traits_(num_min_flow_temp_z2_) + ",";

  j += "\"thermostat_hysteresis_z1\":"        + number_str_(num_hysteresis_z1_) + ",";
  j += "\"hysteresis_z1_lim\":"               + number_traits_(num_hysteresis_z1_) + ",";
  
  j += "\"thermostat_hysteresis_z2\":"        + number_str_(num_hysteresis_z2_) + ",";
  j += "\"hysteresis_z2_lim\":"               + number_traits_(num_hysteresis_z2_) + ",";

  j += "\"pred_sc_time\":"                    + number_str_(pred_sc_time_) + ",";
  j += "\"pred_sc_time_lim\":"                + number_traits_(pred_sc_time_) + ",";
  
  j += "\"pred_sc_delta\":"                   + number_str_(pred_sc_delta_) + ",";
  j += "\"pred_sc_delta_lim\":"               + number_traits_(pred_sc_delta_) + ",";  

  j += "\"z1_current_temp\":"                 + climate_current_str_(virtual_climate_z1_) + ",";
  j += "\"z1_setpoint\":"                     + climate_target_str_(virtual_climate_z1_) + ",";
  j += "\"z1_action\":"                       + climate_action_str_(virtual_climate_z1_) + ",";

  j += "\"z2_current_temp\":"                 + climate_current_str_(virtual_climate_z2_) + ",";
  j += "\"z2_setpoint\":"                     + climate_target_str_(virtual_climate_z2_) + ",";
  j += "\"z2_action\":"                       + climate_action_str_(virtual_climate_z2_) + ",";

  j += "\"room_z1_current\":"                  + climate_current_str_(heatpump_climate_z1_) + ",";
  j += "\"room_z1_setpoint\":"                 + climate_target_str_(heatpump_climate_z1_) + ",";
  j += "\"room_z1_action\":"                   + climate_action_str_(heatpump_climate_z1_) + ",";

  j += "\"room_z2_current\":"                  + climate_current_str_(heatpump_climate_z2_) + ",";
  j += "\"room_z2_setpoint\":"                 + climate_target_str_(heatpump_climate_z2_) + ",";
  j += "\"room_z2_action\":"                   + climate_action_str_(heatpump_climate_z2_) + ",";

  j += "\"flow_z1_current\":"                 + climate_current_str_(flow_climate_z1_) + ",";
  j += "\"flow_z1_setpoint\":"                + climate_target_str_(flow_climate_z1_) + ",";
  j += "\"flow_z2_current\":"                 + climate_current_str_(flow_climate_z2_) + ",";
  j += "\"flow_z2_setpoint\":"                + climate_target_str_(flow_climate_z2_) + ",";

  j += "\"status_compressor\":"               + std::string(bin_str_(status_compressor_)) + ",";
  j += "\"status_booster\":"                  + std::string(bin_str_(status_booster_)) + ",";
  j += "\"status_defrost\":"                  + std::string(bin_str_(status_defrost_)) + ",";
  j += "\"status_water_pump\":"               + std::string(bin_str_(status_water_pump_)) + ",";
  j += "\"status_in1_request\":"              + std::string(bin_str_(status_in1_request_)) + ",";
  j += "\"status_in6_request\":"              + std::string(bin_str_(status_in6_request_)) + ",";
  j += "\"zone2_enabled\":"                   + std::string(bin_state_(status_zone2_enabled_) ? "true" : "false") + ",";

  j += "\"pred_sc_en\":"                      + sw_str_(pred_sc_switch_) + ",";
  j += "\"auto_adaptive_control_enabled\":"   + sw_str_(sw_auto_adaptive_) + ",";
  j += "\"defrost_risk_handling_enabled\":"   + sw_str_(sw_defrost_mit_) + ",";
  j += "\"smart_boost_enabled\":"             + sw_str_(sw_smart_boost_) + ",";
  j += "\"force_dhw\":"                       + sw_str_(sw_force_dhw_) + ","; 
  j += "\"latest_version\":\""                + text_val_(version_) + "\",";
  j += "\"status_operation\":\""              + text_val_(status_operation_) + "\",";
  
  j += "\"heating_system_type\":\""           + select_idx_(sel_heating_system_type_) + "\",";
  j += "\"room_temp_source_z1\":\""           + select_idx_(sel_room_temp_source_z1_) + "\",";
  j += "\"room_temp_source_z2\":\""           + select_idx_(sel_room_temp_source_z2_) + "\",";
  j += "\"operating_mode_z1\":\""             + select_idx_(sel_operating_mode_z1_) + "\",";
  j += "\"operating_mode_z2\":\""             + select_idx_(sel_operating_mode_z2_) + "\",";
  j += "\"temp_sensor_source_z1\":\""           + select_idx_(sel_temp_source_z1_) + "\",";
  j += "\"temp_sensor_source_z2\":\""           + select_idx_(sel_temp_source_z2_) + "\"";

  j += "}";

  AsyncWebServerResponse *response =
      request->beginResponse(200, "application/json", j.c_str());
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void EcodanDashboard::handle_set_(AsyncWebServerRequest *request) {
  if (request->method() != HTTP_POST) {
    request->send(405, "text/plain", "Method Not Allowed");
    return;
  }

  httpd_req_t *req = *request;
  size_t content_len = req->content_len;
  if (content_len == 0 || content_len > 512) {
    request->send(400, "text/plain", "Bad Request");
    return;
  }

  char body[513] = {0};
  int received = httpd_req_recv(req, body, content_len);
  if (received <= 0) {
    request->send(400, "text/plain", "Read failed");
    return;
  }
  body[received] = '\0';

  ESP_LOGI(TAG, "Dashboard POST body: %s", body);

  char key[64] = {0};
  const char *kp = strstr(body, "\"key\":");
  if (kp) {
    kp += 6;
    while (*kp == ' ' || *kp == '"') kp++;
    int i = 0;
    while (*kp && *kp != '"' && i < 63) key[i++] = *kp++;
  }
  if (strlen(key) == 0) { request->send(400, "text/plain", "Missing key"); return; }

  char strval[128] = {0};
  float fval = 0.0f;
  bool is_string = false;

  const char *vp = strstr(body, "\"value\":");
  if (vp) {
    vp += 8;
    while (*vp == ' ') vp++;
    if (*vp == '"') {
      is_string = true;
      vp++;
      int i = 0;
      while (*vp && *vp != '"' && i < 127) strval[i++] = *vp++;
    } else {
      fval = static_cast<float>(atof(vp));
    }
  }

  ESP_LOGI(TAG, "Dashboard set: key=%s value=%s/%.2f", key,
           is_string ? strval : "-", is_string ? 0.0f : fval);

  {
    std::lock_guard<std::mutex> lock(action_lock_);
    action_queue_.push_back({
        std::string(key),
        std::string(strval),
        fval,
        is_string
    });
  }

  request->send(200, "application/json", "{\"ok\":true}");
}

void EcodanDashboard::dispatch_set_(const std::string &key, const std::string &sval, float fval, bool is_string) {
  auto doSwitch = [&](switch_::Switch *sw) {
    if (!sw) { ESP_LOGW(TAG, "Switch not configured"); return; }
    fval > 0.5f ? sw->turn_on() : sw->turn_off();
  };
  if (key == "auto_adaptive_control_enabled") { doSwitch(sw_auto_adaptive_); return; }
  if (key == "defrost_risk_handling_enabled") { doSwitch(sw_defrost_mit_);   return; }
  if (key == "smart_boost_enabled")           { doSwitch(sw_smart_boost_);   return; }
  if (key == "force_dhw")                     { doSwitch(sw_force_dhw_);     return; } 
  if (key == "predictive_short_cycle_control_enabled") { doSwitch(pred_sc_switch_);   return; }

  auto doSelect = [&](select::Select *sel) {
    if (!sel) { ESP_LOGW(TAG, "Select not configured"); return; }
    auto call = sel->make_call();
    if (is_string) {
      call.set_option(sval);
    } else {
      call.set_index((size_t)fval);
    }
    call.perform();
  };

  if (key == "heating_system_type")   { doSelect(sel_heating_system_type_); return; }
  if (key == "room_temp_source_z1")   { doSelect(sel_room_temp_source_z1_); return; }
  if (key == "room_temp_source_z2")   { doSelect(sel_room_temp_source_z2_); return; }
  if (key == "operating_mode_z1")     { doSelect(sel_operating_mode_z1_); return; }
  if (key == "operating_mode_z2")     { doSelect(sel_operating_mode_z2_); return; }
  if (key == "temp_sensor_source_z1") { doSelect(sel_temp_source_z1_); return; } 
  if (key == "temp_sensor_source_z2") { doSelect(sel_temp_source_z2_); return; } 

  auto doNumber = [&](number::Number *n) {
    if (!n) { ESP_LOGW(TAG, "Number not configured"); return; }
    auto call = n->make_call();
    call.set_value(fval);
    call.perform();
  };
  if (key == "auto_adaptive_setpoint_bias") { doNumber(num_aa_setpoint_bias_); return; }
  if (key == "maximum_heating_flow_temp")   { doNumber(num_max_flow_temp_);    return; }
  if (key == "minimum_heating_flow_temp")   { doNumber(num_min_flow_temp_);    return; }
  if (key == "maximum_heating_flow_temp_z2") { doNumber(num_max_flow_temp_z2_); return; }
  if (key == "minimum_heating_flow_temp_z2") { doNumber(num_min_flow_temp_z2_); return; }
  if (key == "thermostat_hysteresis_z1")    { doNumber(num_hysteresis_z1_);    return; }
  if (key == "thermostat_hysteresis_z2")    { doNumber(num_hysteresis_z2_);    return; }

  if (key == "predictive_short_cycle_high_delta_time_window")    { doNumber(pred_sc_time_);    return; }
  if (key == "predictive_short_cycle_high_delta_threshold")    { doNumber(pred_sc_delta_);    return; }

  if (key == "dhw_setpoint" && dhw_climate_ != nullptr) {
    auto call = dhw_climate_->make_call();
    call.set_target_temperature(fval);
    call.perform();
    ESP_LOGI(TAG, "DHW setpoint: %.1f", fval);
    return;
  }

  auto doClimate = [&](climate::Climate *c, const char *name) {
    if (!c) { ESP_LOGW(TAG, "%s climate not configured", name); return; }
    auto call = c->make_call();
    call.set_target_temperature(fval);
    call.perform();
    ESP_LOGI(TAG, "%s setpoint: %.1f", name, fval);
  };
  if (key == "virtual_climate_z1_setpoint") { doClimate(virtual_climate_z1_, "Z1"); return; }
  if (key == "virtual_climate_z2_setpoint") { doClimate(virtual_climate_z2_, "Z2"); return; }

  if (key == "heatpump_climate_z1_setpoint") { doClimate(heatpump_climate_z1_, "Room Z1"); return; }
  if (key == "heatpump_climate_z2_setpoint") { doClimate(heatpump_climate_z2_, "Room Z2"); return; }

  if (key == "flow_climate_z1_setpoint")     { doClimate(flow_climate_z1_, "Flow Z1"); return; }
  if (key == "flow_climate_z2_setpoint")     { doClimate(flow_climate_z2_, "Flow Z2"); return; }

  if (is_string) {
     ESP_LOGW(TAG, "Unknown string key: %s", key.c_str());
  }
}

std::string EcodanDashboard::sensor_str_(sensor::Sensor *s) {
  if (s == nullptr || !s->has_state()) return "null";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.2f", s->state);
  return std::string(buf);
}

std::string EcodanDashboard::climate_action_str_(climate::Climate *c) {
  if (c == nullptr) return "\"off\"";
  switch (c->action) {
    case climate::CLIMATE_ACTION_OFF: return "\"off\"";
    case climate::CLIMATE_ACTION_COOLING: return "\"cooling\"";
    case climate::CLIMATE_ACTION_HEATING: return "\"heating\"";
    case climate::CLIMATE_ACTION_DRYING: return "\"drying\"";
    case climate::CLIMATE_ACTION_IDLE: return "\"idle\"";
    default: return "\"idle\"";
  }
}

std::string EcodanDashboard::climate_current_str_(climate::Climate *c) {
  if (c == nullptr || std::isnan(c->current_temperature)) return "null";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", c->current_temperature);
  return std::string(buf);
}

std::string EcodanDashboard::climate_target_str_(climate::Climate *c) {
  if (c == nullptr || std::isnan(c->target_temperature)) return "null";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", c->target_temperature);
  return std::string(buf);
}

std::string EcodanDashboard::number_str_(number::Number *n) {
  if (n == nullptr || !n->has_state()) return "null";
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", n->state);
  return std::string(buf);
}

const char *EcodanDashboard::bin_str_(binary_sensor::BinarySensor *b) {
  if (b == nullptr || !b->has_state()) return "null";
  return b->state ? "true" : "false";
}

bool EcodanDashboard::bin_state_(binary_sensor::BinarySensor *b) {
  return (b != nullptr && b->has_state() && b->state);
}

std::string EcodanDashboard::text_val_(text_sensor::TextSensor *t) {
  if (t == nullptr || !t->has_state()) return "";
  std::string out;
  for (char c : t->state) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

std::string EcodanDashboard::select_idx_(select::Select *s) {
  if (s == nullptr || !s->active_index().has_value()) return "null";
  return std::to_string(s->active_index().value());
}

std::string EcodanDashboard::select_str_(select::Select *s) {
  if (s == nullptr) return "";
  auto val = s->current_option();
  std::string out;
  out.reserve(val.size() + 2);
  for (char c : val) {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

std::string EcodanDashboard::sw_str_(switch_::Switch *sw) {
  if (sw == nullptr) return "null";
  return sw->state ? "true" : "false";
}

std::string EcodanDashboard::number_traits_(number::Number *n) {
  if (n == nullptr) return "null";
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"min\":%.1f,\"max\":%.1f,\"step\":%.1f}", 
           n->traits.get_min_value(), n->traits.get_max_value(), n->traits.get_step());
  return std::string(buf);
}

}  // namespace asgard_dashboard
}  // namespace esphome
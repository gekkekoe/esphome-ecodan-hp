#include "asgard_dashboard.h"
#include "dashboard_html.h"
#include "dashboard_js.h"
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

  uint32_t now = millis();
  if (now - last_history_time_ >= 60000 || last_history_time_ == 0) {
    last_history_time_ = now;
    record_history_();
  }

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
          url == "/dashboard/state" || url == "/dashboard/set" ||
          url == "/dashboard/history" ||
          url == "/js/chart.js" || url == "/js/hammer.js" || url == "/js/zoom.js"); 
}

void EcodanDashboard::handleRequest(AsyncWebServerRequest *request) {
  const auto& url = request->url();
  if      (url == "/dashboard" || url == "/dashboard/") handle_root_(request);
  else if (url == "/dashboard/state")                   handle_state_(request);
  else if (url == "/dashboard/set")                     handle_set_(request);
  else if (url == "/dashboard/history")                 handle_history_request_(request);
  
  // JS ROUTES ---
  else if (url == "/js/chart.js") {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", CHARTJS_GZ, CHARTJS_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000"); // Let browser cache for 1 year
    request->send(response);
  }
  else if (url == "/js/hammer.js") {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", HAMMERJS_GZ, HAMMERJS_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000");
    request->send(response);
  }
  else if (url == "/js/zoom.js") {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", ZOOMJS_GZ, ZOOMJS_GZ_LEN);
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000");
    request->send(response);
  }
  
  else request->send(404, "text/plain", "Not found");
}

void EcodanDashboard::handle_root_(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(
      200, "text/html", DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void EcodanDashboard::handle_state_(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Cache-Control", "no-cache");
  response->print("{");

  // --- ZERO-ALLOCATION HELPER FUNCTIES ---
  auto p_sens = [&](const char* k, sensor::Sensor* s) {
    if (s && s->has_state() && !std::isnan(s->state)) response->printf("\"%s\":%.2f,", k, s->state);
    else response->printf("\"%s\":null,", k);
  };
  
  auto p_num = [&](const char* k, number::Number* n) {
    if (n && n->has_state() && !std::isnan(n->state)) response->printf("\"%s\":%.1f,", k, n->state);
    else response->printf("\"%s\":null,", k);
  };

  auto p_lim = [&](const char* k, number::Number* n) {
    if (n) response->printf("\"%s\":{\"min\":%.1f,\"max\":%.1f,\"step\":%.1f},", 
         k, n->traits.get_min_value(), n->traits.get_max_value(), n->traits.get_step());
    else response->printf("\"%s\":null,", k);
  };

  auto p_bin = [&](const char* k, binary_sensor::BinarySensor* b) {
    if (b && b->has_state()) response->printf("\"%s\":%s,", k, b->state ? "true" : "false");
    else response->printf("\"%s\":null,", k);
  };

  auto p_sw = [&](const char* k, switch_::Switch* sw) {
    if (sw) response->printf("\"%s\":%s,", k, sw->state ? "true" : "false");
    else response->printf("\"%s\":null,", k);
  };

  auto p_sel = [&](const char* k, select::Select* sel) {
    if (sel && sel->active_index().has_value()) 
        response->printf("\"%s\":\"%zu\",", k, sel->active_index().value());
    else response->printf("\"%s\":null,", k);
  };

  auto p_txt = [&](const char* k, text_sensor::TextSensor* t) {
    response->printf("\"%s\":\"", k);
    if (t && t->has_state()) {
      char stack_buf[128];
      
      strncpy(stack_buf, t->state.c_str(), sizeof(stack_buf) - 1);
      stack_buf[sizeof(stack_buf) - 1] = '\0'; 
      
      for (char *c = stack_buf; *c != '\0'; ++c) {
        if (*c == '"') response->print("\\\"");
        else if (*c == '\\') response->print("\\\\");
        else response->printf("%c", *c);
      }
    }
    response->print("\",");
  };

  auto p_clim_cur = [&](const char* k, climate::Climate* c) {
    if (c && !std::isnan(c->current_temperature)) response->printf("\"%s\":%.1f,", k, c->current_temperature);
    else response->printf("\"%s\":null,", k);
  };

  auto p_clim_tar = [&](const char* k, climate::Climate* c) {
    if (c && !std::isnan(c->target_temperature)) response->printf("\"%s\":%.1f,", k, c->target_temperature);
    else response->printf("\"%s\":null,", k);
  };

  auto p_clim_act = [&](const char* k, climate::Climate* c) {
    if (!c) { response->printf("\"%s\":\"off\",", k); return; }
    switch (c->action) {
      case climate::CLIMATE_ACTION_OFF: response->printf("\"%s\":\"off\",", k); break;
      case climate::CLIMATE_ACTION_COOLING: response->printf("\"%s\":\"cooling\",", k); break;
      case climate::CLIMATE_ACTION_HEATING: response->printf("\"%s\":\"heating\",", k); break;
      case climate::CLIMATE_ACTION_DRYING: response->printf("\"%s\":\"drying\",", k); break;
      case climate::CLIMATE_ACTION_IDLE: 
      default: response->printf("\"%s\":\"idle\",", k); break;
    }
  };

  auto p_clim_mode = [&](const char* k, climate::Climate* c) {
    if (!c) { response->printf("\"%s\":\"off\",", k); return; }
    switch (c->mode) {
      case climate::CLIMATE_MODE_HEAT: response->printf("\"%s\":\"heat\",", k); break;
      case climate::CLIMATE_MODE_COOL: response->printf("\"%s\":\"cool\",", k); break;
      case climate::CLIMATE_MODE_AUTO: response->printf("\"%s\":\"auto\",", k); break;
      case climate::CLIMATE_MODE_OFF:
      default: response->printf("\"%s\":\"off\",", k); break;
    }
  };

  // --- DIRECT DATA STREAMING ---
  p_sens("hp_feed_temp", hp_feed_temp_);
  p_sens("hp_return_temp", hp_return_temp_);
  p_sens("outside_temp", outside_temp_);
  p_sens("compressor_frequency", compressor_frequency_);
  p_sens("flow_rate", flow_rate_);
  p_sens("computed_output_power", computed_output_power_);
  p_sens("daily_computed_output_power", daily_computed_output_power_);
  p_sens("daily_total_energy_consumption", daily_total_energy_consumption_);
  p_sens("compressor_starts", compressor_starts_);
  p_sens("runtime", runtime_);
  p_sens("wifi_signal_db", wifi_signal_db_);

  p_sens("dhw_temp", dhw_temp_);
  p_sens("dhw_flow_temp_target", dhw_flow_temp_target_);
  p_sens("dhw_flow_temp_drop", dhw_flow_temp_drop_);
  p_sens("dhw_consumed", dhw_consumed_);
  p_sens("dhw_delivered", dhw_delivered_);
  p_sens("dhw_cop", dhw_cop_);

  p_sens("heating_consumed", heating_consumed_);
  p_sens("heating_produced", heating_produced_);
  p_sens("heating_cop", heating_cop_);
  p_sens("cooling_consumed", cooling_consumed_);
  p_sens("cooling_produced", cooling_produced_);
  p_sens("cooling_cop", cooling_cop_);

  p_sens("z1_flow_temp_target", z1_flow_temp_target_);
  p_sens("z2_flow_temp_target", z2_flow_temp_target_);

  p_num("auto_adaptive_setpoint_bias", num_aa_setpoint_bias_);
  p_lim("aa_bias_lim", num_aa_setpoint_bias_);
  
  p_num("maximum_heating_flow_temp", num_max_flow_temp_);
  p_lim("max_flow_lim", num_max_flow_temp_);
  p_num("minimum_heating_flow_temp", num_min_flow_temp_);
  p_lim("min_flow_lim", num_min_flow_temp_);

  p_num("maximum_heating_flow_temp_z2", num_max_flow_temp_z2_);
  p_lim("max_flow_z2_lim", num_max_flow_temp_z2_);
  p_num("minimum_heating_flow_temp_z2", num_min_flow_temp_z2_);
  p_lim("min_flow_z2_lim", num_min_flow_temp_z2_);

  p_num("thermostat_hysteresis_z1", num_hysteresis_z1_);
  p_lim("hysteresis_z1_lim", num_hysteresis_z1_);
  
  p_num("thermostat_hysteresis_z2", num_hysteresis_z2_);
  p_lim("hysteresis_z2_lim", num_hysteresis_z2_);

  p_num("pred_sc_time", pred_sc_time_);
  p_lim("pred_sc_time_lim", pred_sc_time_);
  p_num("pred_sc_delta", pred_sc_delta_);
  p_lim("pred_sc_delta_lim", pred_sc_delta_);  

  p_clim_cur("z1_current_temp", virtual_climate_z1_);
  p_clim_tar("z1_setpoint", virtual_climate_z1_);
  p_clim_act("z1_action", virtual_climate_z1_);
  p_clim_mode("z1_mode", virtual_climate_z1_);

  p_clim_cur("z2_current_temp", virtual_climate_z2_);
  p_clim_tar("z2_setpoint", virtual_climate_z2_);
  p_clim_act("z2_action", virtual_climate_z2_);
  p_clim_mode("z2_mode", virtual_climate_z2_);

  p_clim_cur("room_z1_current", heatpump_climate_z1_);
  p_clim_tar("room_z1_setpoint", heatpump_climate_z1_);
  p_clim_act("room_z1_action", heatpump_climate_z1_);

  p_clim_cur("room_z2_current", heatpump_climate_z2_);
  p_clim_tar("room_z2_setpoint", heatpump_climate_z2_);
  p_clim_act("room_z2_action", heatpump_climate_z2_);

  p_clim_cur("flow_z1_current", flow_climate_z1_);
  p_clim_tar("flow_z1_setpoint", flow_climate_z1_);
  p_clim_cur("flow_z2_current", flow_climate_z2_);
  p_clim_tar("flow_z2_setpoint", flow_climate_z2_);

  p_bin("status_compressor", status_compressor_);
  p_bin("status_booster", status_booster_);
  p_bin("status_defrost", status_defrost_);
  p_bin("status_water_pump", status_water_pump_);
  p_bin("status_in1_request", status_in1_request_);
  p_bin("status_in6_request", status_in6_request_);
  
  response->printf("\"zone2_enabled\":%s,", bin_state_(status_zone2_enabled_) ? "true" : "false");

  p_sw("pred_sc_en", pred_sc_switch_);
  p_sw("auto_adaptive_control_enabled", sw_auto_adaptive_);
  p_sw("defrost_risk_handling_enabled", sw_defrost_mit_);
  p_sw("smart_boost_enabled", sw_smart_boost_);
  p_sw("force_dhw", sw_force_dhw_); 
  
  p_txt("latest_version", version_);
  p_txt("status_operation", status_operation_);
  p_txt("status_dip_switch_1", status_dip_switch_1_);
  
  p_sel("heating_system_type", sel_heating_system_type_);
  p_sel("room_temp_source_z1", sel_room_temp_source_z1_);
  p_sel("room_temp_source_z2", sel_room_temp_source_z2_);
  p_sel("operating_mode_z1", sel_operating_mode_z1_);
  p_sel("operating_mode_z2", sel_operating_mode_z2_);
  p_sel("temp_sensor_source_z1", sel_temp_source_z1_);
  p_sel("temp_sensor_source_z2", sel_temp_source_z2_);

  response->printf("\"_uptime_ms\":%u}", millis());
  
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
  
  if (key == "virtual_climate_z1_mode" || key == "virtual_climate_z2_mode") {
    climate::Climate *c = (key == "virtual_climate_z1_mode") ? virtual_climate_z1_ : virtual_climate_z2_;
    if (c && is_string) {
      auto call = c->make_call();
      if (sval == "heat") call.set_mode(climate::CLIMATE_MODE_HEAT);
      else if (sval == "cool") call.set_mode(climate::CLIMATE_MODE_COOL);
      else if (sval == "auto") call.set_mode(climate::CLIMATE_MODE_AUTO);
      else call.set_mode(climate::CLIMATE_MODE_OFF);
      call.perform();
      ESP_LOGI(TAG, "%s set to %s", key.c_str(), sval.c_str());
    }
    return;
  }

  if (is_string) {
     ESP_LOGW(TAG, "Unknown string key: %s", key.c_str());
  }
}

// History handling
int16_t EcodanDashboard::pack_temp_(float val) {
  if (std::isnan(val)) return -32768; 
  return static_cast<int16_t>(val * 10.0f);
}

bool EcodanDashboard::bin_state_(binary_sensor::BinarySensor *b) {
  return (b != nullptr && b->has_state() && b->state);
}

uint8_t EcodanDashboard::encode_mode_(const std::string &mode) {
  std::string m = mode;
  for (char &c : m) c = std::tolower((unsigned char)c);
  if (m.find("legionella") != std::string::npos) return 4;
  if (m.find("hot water") != std::string::npos || m.find("dhw") != std::string::npos) return 3;
  if (m.find("cool") != std::string::npos) return 2;
  if (m.find("heat") != std::string::npos) return 1;
  if (m.find("defrost") != std::string::npos) return 5;
  return 0; // standby/off
}

void EcodanDashboard::record_history_() {
  std::lock_guard<std::mutex> lock(history_lock_);  

  HistoryRecord rec;
  rec.timestamp = time(nullptr); 

  auto get_sensor = [](sensor::Sensor *s) { return (s && s->has_state()) ? s->state : NAN; };
  auto get_curr = [](climate::Climate *c) { return (c) ? c->current_temperature : NAN; };
  auto get_targ = [](climate::Climate *c) { return (c) ? c->target_temperature : NAN; };

  rec.hp_feed   = pack_temp_(get_sensor(hp_feed_temp_));
  rec.hp_return = pack_temp_(get_sensor(hp_return_temp_));
  rec.z1_sp     = pack_temp_(get_targ(virtual_climate_z1_));
  rec.z2_sp     = pack_temp_(get_targ(virtual_climate_z2_));
  rec.z1_curr   = pack_temp_(get_curr(virtual_climate_z1_));
  rec.z2_curr   = pack_temp_(get_curr(virtual_climate_z2_));
  rec.z1_flow   = pack_temp_(get_sensor(z1_flow_temp_target_));
  rec.z2_flow   = pack_temp_(get_sensor(z2_flow_temp_target_));
  rec.freq      = pack_temp_(get_sensor(compressor_frequency_));

  rec.flags = 0;
  if (bin_state_(status_compressor_))  rec.flags |= (1 << 0);
  if (bin_state_(status_booster_))     rec.flags |= (1 << 1);
  if (bin_state_(status_defrost_))     rec.flags |= (1 << 2);
  if (bin_state_(status_water_pump_))  rec.flags |= (1 << 3);
  if (bin_state_(status_in1_request_)) rec.flags |= (1 << 4);
  if (bin_state_(status_in6_request_)) rec.flags |= (1 << 5);

  std::string mode_str = (status_operation_ && status_operation_->has_state()) ? status_operation_->state : "";
  uint8_t mode_enc = encode_mode_(mode_str);
  rec.flags |= ((mode_enc & 0x0F) << 6);

  history_buffer_[history_head_] = rec;
  history_head_ = (history_head_ + 1) % MAX_HISTORY;
  if (history_count_ < MAX_HISTORY) history_count_++;
}

void EcodanDashboard::handle_history_request_(AsyncWebServerRequest *request) {
AsyncResponseStream *response = request->beginResponseStream("application/json");
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Cache-Control", "no-cache");
  
  std::lock_guard<std::mutex> lock(history_lock_);  

  if (history_count_ == 0) {
    response->print("[]");
    request->send(response);
    return;
  }
  
  response->print("[");
  size_t start_idx = (history_count_ == MAX_HISTORY) ? history_head_ : 0;
  size_t step = (history_count_ > 360) ? (history_count_ / 360) : 1;
  if (step < 1) step = 1;
  
  bool first = true;
  for (size_t i = 0; i < history_count_; i += step) {
    size_t idx = (start_idx + i) % MAX_HISTORY;
    const HistoryRecord &rec = history_buffer_[idx];
    if (!first) response->print(",");
    first = false;
    response->printf("[%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u]", 
      rec.timestamp, rec.hp_feed, rec.hp_return, 
      rec.z1_sp, rec.z2_sp, rec.z1_curr, rec.z2_curr, 
      rec.z1_flow, rec.z2_flow, rec.freq, rec.flags
    );
  }
  response->print("]");
  request->send(response);
}

}  // namespace asgard_dashboard
}  // namespace esphome
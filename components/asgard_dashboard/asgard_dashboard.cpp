#include "asgard_dashboard.h"
#include "dashboard_html.h"
#include "setup_html.h"
#include "dashboard_js.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_http_server.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <memory>
#include <esp_littlefs.h>
#include <sys/stat.h>
#include "esphome/components/ecodan/ecodan.h"

// #include "esp_system.h"
// #include "esp_heap_caps.h"

namespace esphome {
namespace asgard_dashboard {

static const char *const TAG = "asgard_dashboard";

//#define TEST_TS
const time_t EcodanDashboard::timestamp() const {
#ifndef TEST_TS
  if (this->ecodan_ != nullptr) {
    auto status = this->ecodan_->get_status();
    return status.timestamp();
  }

  return -1; 
#else
  struct tm t = {0};
  t.tm_year = 2026 - 1900;
  t.tm_mon  = 5 - 1;       
  t.tm_mday = 22;          // (1 - 31)
  t.tm_hour = 18;          // (0 - 23)
  t.tm_min  = 54;          // mins (0 - 59)
  t.tm_sec  = 0;           // secs (0 - 59)
  time_t simulated_start_time = mktime(&t);
  auto new_time = simulated_start_time + (millis() / 1000);
  return new_time;
#endif
}

void EcodanDashboard::setup() {
  ESP_LOGI(TAG, "Setting up Ecodan Dashboard on /dashboard");
  
  action_lock_ = xSemaphoreCreateMutex();
  snapshot_mutex_ = xSemaphoreCreateMutex();
  history_mutex_ = xSemaphoreCreateMutex();
  
  this->odin_lfs_last_write_ms_ = millis();
  this->setup_lfs();

  // ── LFS task (ODIN persistence) ────────────────────────────────────────
  this->lfs_odin_trigger_ = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
    EcodanDashboard::lfs_odin_task_,
    "lfs_odin",
    8192,
    this,
    1,
    &this->lfs_odin_task_handle_,
    1
  );

  // ── LFS task (history persistence) ────────────────────────────────────
  this->lfs_trigger_ = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
    EcodanDashboard::lfs_task_,
    "lfs_hist",
    8192,
    this,
    1,
    &this->lfs_task_handle_,
    1
  );

  base_->init();
  base_->add_handler(this);
  this->load_odin_data(-1);
}

void EcodanDashboard::loop() {
  uint32_t now = millis();
  if (now - last_history_time_ >= 60000 || last_history_time_ == 0) {
    last_history_time_ = now;
    record_history_();
  }

  if (now - last_snapshot_time_ >= 1000 || last_snapshot_time_ == 0) {
    last_snapshot_time_ = now;
    update_snapshot_();
  }

  // ODIN Persistence Trigger (Moved away from NVS)
  if (this->odin_lfs_dirty_) {
    const uint32_t LFS_FLUSH_INTERVAL_MS = LFS_FLUSH_COUNT * 60 * 1000;
    if (this->odin_lfs_last_write_ms_ == 0 || (now - this->odin_lfs_last_write_ms_) >= LFS_FLUSH_INTERVAL_MS) {
      this->odin_lfs_dirty_ = false;
      this->odin_lfs_last_write_ms_ = now;
      
      this->lfs_show_tab_cache_.store(
          this->sw_show_solver_tab_ != nullptr && this->sw_show_solver_tab_->state);
          
      if (this->lfs_odin_trigger_ != nullptr) {
        xSemaphoreGive(this->lfs_odin_trigger_);
      }
    }
  }

  std::vector<DashboardAction> todo;
  
  if (action_lock_ != NULL && xSemaphoreTake(action_lock_, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (action_queue_.empty()) {
      xSemaphoreGive(action_lock_);
      return;
    }
    
    todo = action_queue_;
    action_queue_.clear();
    xSemaphoreGive(action_lock_);
  } else {
    ESP_LOGW(TAG, "Failed to acquire action_lock_ in loop (Timeout)");
    return;
  }

  for (const auto &act : todo) {
    this->dispatch_set_(act.key, act.s_value, act.f_value, act.is_string);
  }
}

bool EcodanDashboard::canHandle(AsyncWebServerRequest *request) const {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  auto url = request->url_to(url_buf);
  return (url == "/dashboard" || url == "/dashboard/" ||
          url == "/dashboard/setup" ||
          url == "/dashboard/state" || url == "/dashboard/set" ||
          url == "/dashboard/history" || url == "/dashboard/odin" ||
          url == "/js/chart.js" || url == "/js/hammer.js" || url == "/js/zoom.js"); 
}

void EcodanDashboard::handleRequest(AsyncWebServerRequest *request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  auto url = request->url_to(url_buf);
  
  if      (url == "/dashboard" || url == "/dashboard/") handle_root_(request);
  else if (url == "/dashboard/setup")                   handle_setup_(request);
  else if (url == "/dashboard/state")                   handle_state_(request);
  else if (url == "/dashboard/set")                     handle_set_(request);
  else if (url == "/dashboard/history")                 handle_history_request_(request);
  else if (url == "/dashboard/odin")                    handle_odin_request_(request);
  else if (url == "/js/chart.js" || url == "/js/hammer.js" || url == "/js/zoom.js") {
    handle_js_(request);
  }
  else {
    httpd_req_t *req_404 = *request;
    httpd_resp_set_status(req_404, "404 Not Found");
    httpd_resp_set_type(req_404, "text/plain");
    httpd_resp_send(req_404, "Not found", HTTPD_RESP_USE_STRLEN);
  }
}

void EcodanDashboard::send_chunked_(AsyncWebServerRequest *request, const char *content_type, const uint8_t *data, size_t length, const char *cache_control) {
  // Extract the native ESP-IDF request pointer
  httpd_req_t *req = *request;
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, content_type);
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  
  if (cache_control != nullptr) {
    httpd_resp_set_hdr(req, "Cache-Control", cache_control);
  }

  size_t index = 0;
  size_t remaining = length;
  const size_t chunk_size = 2048;

  while (remaining > 0) {
    size_t to_send = (remaining < chunk_size) ? remaining : chunk_size;
    
    // Prevent lwIP crash by checking if the client disconnected
    if (httpd_resp_send_chunk(req, (const char*)(data + index), to_send) != ESP_OK) {
        return; 
    }
    
    index += to_send;
    remaining -= to_send;
  }

  httpd_resp_send_chunk(req, nullptr, 0);
}

void EcodanDashboard::handle_root_(AsyncWebServerRequest *request) {
  send_chunked_(request, "text/html", DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN, "no-cache");
}

void EcodanDashboard::handle_setup_(AsyncWebServerRequest *request) {
  send_chunked_(request, "text/html", SETUP_HTML_GZ, SETUP_HTML_GZ_LEN, "no-cache");
}

void EcodanDashboard::handle_js_(AsyncWebServerRequest *request) {
  char url_buf[AsyncWebServerRequest::URL_BUF_SIZE];
  auto url = request->url_to(url_buf);
  const uint8_t *file_data = nullptr;
  size_t file_len = 0;

  // Determine which file to serve
  if (url == "/js/chart.js") {
    file_data = CHARTJS_GZ;
    file_len = CHARTJS_GZ_LEN;
  } else if (url == "/js/hammer.js") {
    file_data = HAMMERJS_GZ;
    file_len = HAMMERJS_GZ_LEN;
  } else if (url == "/js/zoom.js") {
    file_data = ZOOMJS_GZ;
    file_len = ZOOMJS_GZ_LEN;
  } else {
    httpd_req_t *req_404 = *request;
    httpd_resp_set_status(req_404, "404 Not Found");
    httpd_resp_set_type(req_404, "text/plain");
    httpd_resp_send(req_404, "File not found", HTTPD_RESP_USE_STRLEN);
    return;
  }

  send_chunked_(request, "application/javascript", file_data, file_len, "public, max-age=31536000");
}

void EcodanDashboard::handle_set_(AsyncWebServerRequest *request) {
  httpd_req_t *req = *request;

  if (request->method() != HTTP_POST) {
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Method Not Allowed", HTTPD_RESP_USE_STRLEN);
    return;
  }

  size_t content_len = req->content_len;
  if (content_len == 0 || content_len > 1024) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Bad Request", HTTPD_RESP_USE_STRLEN);
    return;
  }

  // Memory-safe allocation on the heap instead of the stack
  char *body = (char *)malloc(content_len + 1);
  if (body == nullptr) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Out of Memory", HTTPD_RESP_USE_STRLEN);
    return;
  }
  memset(body, 0, content_len + 1);

  int received = httpd_req_recv(req, body, content_len);
  if (received <= 0) {
    free(body);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Read failed", HTTPD_RESP_USE_STRLEN);
    return;
  }
  body[received] = '\0';

  ESP_LOGI(TAG, "Dashboard POST body: %s", body);

  // Restore the original JSON parsing logic for keys and values
  char key[64] = {0};
  const char *kp = strstr(body, "\"key\":");
  if (kp) {
    kp += 6;
    while (*kp == ' ' || *kp == '"') kp++;
    int i = 0;
    while (*kp && *kp != '"' && i < 63) key[i++] = *kp++;
  }

  if (strlen(key) == 0) {
    free(body);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "Missing key", HTTPD_RESP_USE_STRLEN);
    return;
  }

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

  // Always free heap memory immediately after extracting values
  free(body);

  ESP_LOGI(TAG, "Dashboard set: key=%s value=%s/%.2f", key,
           is_string ? strval : "-", is_string ? 0.0f : fval);

  // Queue the action safely to be executed on the ESPHome Main Thread
  if (action_lock_ != NULL && xSemaphoreTake(action_lock_, pdMS_TO_TICKS(100)) == pdTRUE) {
    action_queue_.push_back({
        std::string(key),
        std::string(strval),
        fval,
        is_string
    });
    xSemaphoreGive(action_lock_);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
  } else {
    ESP_LOGW(TAG, "Failed to acquire action_lock_ in handle_set_ (Timeout)");
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "System busy, try again", HTTPD_RESP_USE_STRLEN);
  }
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
  if (key == "power_mode")                    { doSwitch(sw_power_mode_);    return; }
  if (key == "predictive_short_cycle_control_enabled") { doSwitch(pred_sc_switch_);   return; }
  if (key == "use_dynamic_cost_solver")       { doSwitch(sw_use_solver_);    return; }
  if (key == "show_solver_tab_enabled")       { doSwitch(sw_show_solver_tab_); this->odin_lfs_dirty_ = true; return; }

  // Server control
  if (key == "server_control_enabled")             { doSwitch(sw_server_control_);          return; }
  if (key == "server_control_prohibit_dhw")        { doSwitch(sw_sc_prohibit_dhw_);         return; }
  if (key == "server_control_prohibit_z1_heating") { doSwitch(sw_sc_prohibit_z1_heating_);  return; }
  if (key == "server_control_prohibit_z1_cooling") { doSwitch(sw_sc_prohibit_z1_cooling_);  return; }
  if (key == "server_control_prohibit_z2_heating") { doSwitch(sw_sc_prohibit_z2_heating_);  return; }
  if (key == "server_control_prohibit_z2_cooling") { doSwitch(sw_sc_prohibit_z2_cooling_);  return; }

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
  if (key == "solver_dhw_mode")       { doSelect(solver_dhw_mode_); return; }

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
  if (key == "cooling_smart_start_z1")       { doNumber(num_cooling_smart_start_z1_); return; }
  if (key == "minimum_cooling_flow_z1")      { doNumber(num_min_cooling_flow_z1_); return; }
  if (key == "minimum_cooling_flow_z2")      { doNumber(num_min_cooling_flow_z2_); return; }

  if (key == "thermostat_hysteresis_z1")    { doNumber(num_hysteresis_z1_);    return; }
  if (key == "thermostat_hysteresis_z2")    { doNumber(num_hysteresis_z2_);    return; }
  if (key == "raw_heat_produced") { doNumber(num_raw_heat_produced_); return; }
  if (key == "raw_elec_consumed") { doNumber(num_raw_elec_consumed_); return; }
  if (key == "raw_runtime_hours") { doNumber(num_raw_runtime_hours_); return; }
  if (key == "raw_avg_outside_temp") { doNumber(num_raw_avg_outside_temp_); return; }
  if (key == "raw_avg_room_temp") { doNumber(num_raw_avg_room_temp_); return; }
  if (key == "raw_delta_room_temp") { doNumber(num_raw_delta_room_temp_); return; }
  if (key == "raw_hl_tm_product") { doNumber(num_raw_hl_tm_product_); return; }
  if (key == "raw_solar_factor") { doNumber(num_raw_solar_factor_); return; }
  if (key == "battery_soc_kwh") { doNumber(num_battery_soc_kwh_); return; }
  if (key == "battery_max_discharge_kw") { doNumber(num_battery_max_discharge_kw_); return; }
  if (key == "dhw_start_threshold") { doNumber(num_dhw_start_threshold_); return; }

  if (key == "raw_cool_produced") { doNumber(num_raw_cool_produced_); return; }
  if (key == "raw_cool_elec_consumed") { doNumber(num_raw_cool_elec_consumed_); return; }
  if (key == "raw_cool_runtime_hours") { doNumber(num_raw_cool_runtime_hours_); return; }
  if (key == "raw_cool_avg_outside_temp") { doNumber(num_raw_cool_avg_outside_temp_); return; }

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

  if (key == "ui_use_room_z1" && ui_use_room_z1_) { ui_use_room_z1_->value() = (fval > 0.5); return; }
  if (key == "ui_use_room_z2" && ui_use_room_z2_) { ui_use_room_z2_->value() = (fval > 0.5); return; }

  if (key == "solver_ip_address" && txt_solver_ip_ != nullptr && is_string) {
    auto call = txt_solver_ip_->make_call();
    call.set_value(sval);
    call.perform();
    ESP_LOGI(TAG, "Solver IP Address set and saved to: %s", sval.c_str());
    return;
  }

  if (is_string) {
     ESP_LOGW(TAG, "Unknown string key: %s", key.c_str());
  }
}

void EcodanDashboard::update_snapshot_() {
  if (snapshot_mutex_ == NULL || xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(TAG, "Failed to acquire snapshot_mutex_ in update_snapshot_ (Timeout)");
    return;
  }

  auto get_f = [](sensor::Sensor *s) { return (s && s->has_state()) ? s->state : NAN; };
  auto get_b = [](binary_sensor::BinarySensor *b) { return (b && b->has_state()) ? b->state : false; };
  auto get_sw = [](switch_::Switch *s) { return (s) ? s->state : false; };
  
  auto get_n = [](number::Number *n, DashboardSnapshot::NumData &d) {
    if (n) {
      d.val = n->has_state() ? n->state : NAN;
      d.min = n->traits.get_min_value();
      d.max = n->traits.get_max_value();
      d.step = n->traits.get_step();
    }
  };

  auto get_c = [](climate::Climate *c, DashboardSnapshot::ClimData &d) {
    if (c) {
      d.curr = c->current_temperature;
      d.tar = c->target_temperature;
      d.action = (int)c->action;
      d.mode = (int)c->mode;
    } else {
      d.curr = NAN; d.tar = NAN; d.action = -1; d.mode = -1;
    }
  };

  auto get_sel = [](select::Select *s) {
    if (s && s->active_index().has_value()) return (int)s->active_index().value();
    return -1;
  };

  // Populate Bools
  current_snapshot_.ui_use_room_z1 = (ui_use_room_z1_ != nullptr) ? ui_use_room_z1_->value() : false;
  current_snapshot_.ui_use_room_z2 = (ui_use_room_z2_ != nullptr) ? ui_use_room_z2_->value() : false;

  current_snapshot_.status_compressor = get_b(status_compressor_);
  current_snapshot_.status_booster = get_b(status_booster_);
  current_snapshot_.status_defrost = get_b(status_defrost_);
  current_snapshot_.status_water_pump = get_b(status_water_pump_);
  current_snapshot_.status_in1_request = get_b(status_in1_request_);
  current_snapshot_.status_in6_request = get_b(status_in6_request_);
  current_snapshot_.status_zone2_enabled = get_b(status_zone2_enabled_);
  current_snapshot_.bin_solver_connected = get_b(bin_solver_connected_);

  current_snapshot_.pred_sc_switch = get_sw(pred_sc_switch_);
  current_snapshot_.sw_auto_adaptive = get_sw(sw_auto_adaptive_);
  current_snapshot_.sw_defrost_mit = get_sw(sw_defrost_mit_);
  current_snapshot_.sw_smart_boost = get_sw(sw_smart_boost_);
  current_snapshot_.sw_force_dhw = get_sw(sw_force_dhw_);
  current_snapshot_.sw_use_solver = get_sw(sw_use_solver_);
  current_snapshot_.sw_show_solver_tab = get_sw(sw_show_solver_tab_);

  // Server control
  current_snapshot_.sw_server_control        = get_sw(sw_server_control_);
  current_snapshot_.sw_sc_prohibit_dhw       = get_sw(sw_sc_prohibit_dhw_);
  current_snapshot_.sw_sc_prohibit_z1_heating = get_sw(sw_sc_prohibit_z1_heating_);
  current_snapshot_.sw_sc_prohibit_z1_cooling = get_sw(sw_sc_prohibit_z1_cooling_);
  current_snapshot_.sw_sc_prohibit_z2_heating = get_sw(sw_sc_prohibit_z2_heating_);
  current_snapshot_.sw_sc_prohibit_z2_cooling = get_sw(sw_sc_prohibit_z2_cooling_);

  // Populate Floats
  current_snapshot_.hp_feed_temp = get_f(hp_feed_temp_);
  current_snapshot_.hp_return_temp = get_f(hp_return_temp_);
  current_snapshot_.outside_temp = get_f(outside_temp_);
  current_snapshot_.liquid_pipe_temp = get_f(liquid_pipe_temp_);
  current_snapshot_.condensing_temp = get_f(condensing_temp_);
  current_snapshot_.compressor_frequency = get_f(compressor_frequency_);
  current_snapshot_.flow_rate = get_f(flow_rate_);
  current_snapshot_.computed_output_power = get_f(computed_output_power_);
  current_snapshot_.daily_computed_output_power = get_f(daily_computed_output_power_);
  current_snapshot_.compressor_starts = get_f(compressor_starts_);
  current_snapshot_.runtime = get_f(runtime_);
  current_snapshot_.wifi_signal_db = get_f(wifi_signal_db_);

  current_snapshot_.dhw_temp = get_f(dhw_temp_);
  current_snapshot_.dhw_flow_temp_target = get_f(dhw_flow_temp_target_);
  current_snapshot_.dhw_flow_temp_drop = get_f(dhw_flow_temp_drop_);
  current_snapshot_.dhw_consumed = get_f(dhw_consumed_);
  current_snapshot_.dhw_delivered = get_f(dhw_delivered_);
  current_snapshot_.dhw_cop = get_f(dhw_cop_);
  current_snapshot_.solver_dhw_mode = get_sel(solver_dhw_mode_);
  current_snapshot_.sw_power_mode = get_sw(sw_power_mode_);

  current_snapshot_.heating_consumed = get_f(heating_consumed_);
  current_snapshot_.heating_produced = get_f(heating_produced_);
  current_snapshot_.heating_cop = get_f(heating_cop_);
  current_snapshot_.cooling_consumed = get_f(cooling_consumed_);
  current_snapshot_.cooling_produced = get_f(cooling_produced_);
  current_snapshot_.cooling_cop = get_f(cooling_cop_);

  current_snapshot_.z1_flow_temp_target = get_f(z1_flow_temp_target_);
  current_snapshot_.z2_flow_temp_target = get_f(z2_flow_temp_target_);

  // Populate Numbers
  get_n(num_aa_setpoint_bias_, current_snapshot_.num_aa_setpoint_bias);
  get_n(num_max_flow_temp_, current_snapshot_.num_max_flow_temp);
  get_n(num_min_flow_temp_, current_snapshot_.num_min_flow_temp);
  get_n(num_max_flow_temp_z2_, current_snapshot_.num_max_flow_temp_z2);
  get_n(num_min_flow_temp_z2_, current_snapshot_.num_min_flow_temp_z2);
  get_n(num_hysteresis_z1_, current_snapshot_.num_hysteresis_z1);
  get_n(num_hysteresis_z2_, current_snapshot_.num_hysteresis_z2);
  get_n(pred_sc_time_, current_snapshot_.pred_sc_time);
  get_n(pred_sc_delta_, current_snapshot_.pred_sc_delta);

  // cooling settings
  get_n(num_cooling_smart_start_z1_, current_snapshot_.num_cooling_smart_start_z1);
  get_n(num_min_cooling_flow_z1_, current_snapshot_.num_min_cooling_flow_z1);
  get_n(num_min_cooling_flow_z2_, current_snapshot_.num_min_cooling_flow_z2);

  // solver data
  get_n(num_raw_heat_produced_, current_snapshot_.num_raw_heat_produced);
  get_n(num_raw_elec_consumed_, current_snapshot_.num_raw_elec_consumed);
  get_n(num_raw_runtime_hours_, current_snapshot_.num_raw_runtime_hours);
  get_n(num_raw_avg_outside_temp_, current_snapshot_.num_raw_avg_outside_temp);
  get_n(num_raw_avg_room_temp_, current_snapshot_.num_raw_avg_room_temp);
  get_n(num_raw_delta_room_temp_, current_snapshot_.num_raw_delta_room_temp);
  get_n(num_raw_hl_tm_product_, current_snapshot_.num_raw_hl_tm_product);
  get_n(num_raw_solar_factor_, current_snapshot_.num_raw_solar_factor);

  get_n(num_raw_cool_produced_, current_snapshot_.num_raw_cool_produced);
  get_n(num_raw_cool_elec_consumed_, current_snapshot_.num_raw_cool_elec_consumed);
  get_n(num_raw_cool_runtime_hours_, current_snapshot_.num_raw_cool_runtime_hours);
  get_n(num_raw_cool_avg_outside_temp_, current_snapshot_.num_raw_cool_avg_outside_temp);

  get_n(num_battery_soc_kwh_, current_snapshot_.num_battery_soc_kwh);
  get_n(num_battery_max_discharge_kw_, current_snapshot_.num_battery_max_discharge_kw);

  get_n(num_dhw_start_threshold_, current_snapshot_.num_dhw_start_threshold);

  if (txt_solver_ip_ && txt_solver_ip_->has_state()) {
    strncpy(current_snapshot_.txt_solver_ip, txt_solver_ip_->state.c_str(), sizeof(current_snapshot_.txt_solver_ip) - 1);
    current_snapshot_.txt_solver_ip[sizeof(current_snapshot_.txt_solver_ip) - 1] = '\0';
  } else {
    current_snapshot_.txt_solver_ip[0] = '\0';
  }

  // Use external kWh meter if selected, otherwise fallback to internal Ecodan sensor
  if (solver_kwh_meter_feedback_source_ != nullptr && solver_kwh_meter_feedback_source_->active_index().value_or(0) != 0) {
      current_snapshot_.daily_total_energy_consumption = (solver_kwh_meter_feedback_ != nullptr && solver_kwh_meter_feedback_->has_state()) ? solver_kwh_meter_feedback_->state : NAN;
  } else {
      current_snapshot_.daily_total_energy_consumption = get_f(daily_total_energy_consumption_);
  }

  // Populate Climates
  get_c(virtual_climate_z1_, current_snapshot_.virt_z1);
  get_c(virtual_climate_z2_, current_snapshot_.virt_z2);
  get_c(heatpump_climate_z1_, current_snapshot_.room_z1);
  get_c(heatpump_climate_z2_, current_snapshot_.room_z2);
  get_c(flow_climate_z1_, current_snapshot_.flow_z1);
  get_c(flow_climate_z2_, current_snapshot_.flow_z2);

  // Populate Selects & Modes
  current_snapshot_.operation_mode = get_f(operation_mode_);
  current_snapshot_.sel_heating_system_type = get_sel(sel_heating_system_type_);
  current_snapshot_.sel_room_temp_source_z1 = get_sel(sel_room_temp_source_z1_);
  current_snapshot_.sel_room_temp_source_z2 = get_sel(sel_room_temp_source_z2_);
  current_snapshot_.sel_operating_mode_z1 = get_sel(sel_operating_mode_z1_);
  current_snapshot_.sel_operating_mode_z2 = get_sel(sel_operating_mode_z2_);
  current_snapshot_.sel_temp_source_z1 = get_sel(sel_temp_source_z1_);
  current_snapshot_.sel_temp_source_z2 = get_sel(sel_temp_source_z2_);

  // Safely copy string into fixed char array
  if (version_ && version_->has_state()) {
    strncpy(current_snapshot_.version, version_->state.c_str(), sizeof(current_snapshot_.version) - 1);
    current_snapshot_.version[sizeof(current_snapshot_.version) - 1] = '\0';
  } else {
    current_snapshot_.version[0] = '\0';
  }

  xSemaphoreGive(snapshot_mutex_);
}

void EcodanDashboard::handle_state_(AsyncWebServerRequest *request) {
  DashboardSnapshot snap;

  if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
    snap = current_snapshot_;
    xSemaphoreGive(snapshot_mutex_);
  } else {
    ESP_LOGW(TAG, "Failed to acquire snapshot_mutex_ in handle_state_ (Timeout)");
    httpd_req_t *req_err = *request;
    httpd_resp_set_status(req_err, "503 Service Unavailable");
    httpd_resp_set_type(req_err, "text/plain");
    httpd_resp_send(req_err, "System busy, try again", HTTPD_RESP_USE_STRLEN);
    return;
  }

  httpd_req_t *req = *request;
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "close");

  // Shared 2KB heap buffer — flushed per logical block to stay well within limits.
  // Each block is at most ~600 bytes so there is comfortable headroom.
  constexpr size_t BUF_SIZE = 2048;
  std::vector<char> buf(BUF_SIZE);
  int off = 0;

  // Send current buffer contents and reset offset. Returns false on TCP error.
  auto flush = [&]() -> bool {
    if (off <= 0) return true;
    bool ok = (httpd_resp_send_chunk(req, buf.data(), off) == ESP_OK);
    off = 0;
    return ok;
  };

  // Append helpers — write into buf[], flush automatically when >75% full.
  auto p_f = [&](const char* k, float v) {
    if (!std::isnan(v)) off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":%.2f,", k, v);
    else                off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":null,", k);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_b = [&](const char* k, bool v) {
    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":%s,", k, v ? "true" : "false");
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_n = [&](const char* k, float v) {
    if (!std::isnan(v)) off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":%.4g,", k, v);
    else                off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":null,", k);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_lim = [&](const char* k, const DashboardSnapshot::NumData& d) {
    if (!std::isnan(d.min)) off += snprintf(buf.data() + off, BUF_SIZE - off,
                                "\"%s\":{\"min\":%.4g,\"max\":%.4g,\"step\":%.4g},", k, d.min, d.max, d.step);
    else                    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":null,", k);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_sel = [&](const char* k, int v) {
    if (v >= 0) off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":\"%d\",", k, v);
    else        off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":null,", k);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_act = [&](const char* k, int a) {
    const char* s = "idle";
    if      (a < 0)                                  s = "off";
    else if (a == climate::CLIMATE_ACTION_OFF)        s = "off";
    else if (a == climate::CLIMATE_ACTION_COOLING)    s = "cooling";
    else if (a == climate::CLIMATE_ACTION_HEATING)    s = "heating";
    else if (a == climate::CLIMATE_ACTION_DRYING)     s = "drying";
    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":\"%s\",", k, s);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };
  auto p_mod = [&](const char* k, int m) {
    const char* s = "off";
    if      (m == climate::CLIMATE_MODE_HEAT) s = "heat";
    else if (m == climate::CLIMATE_MODE_COOL) s = "cool";
    else if (m == climate::CLIMATE_MODE_AUTO) s = "auto";
    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"%s\":\"%s\",", k, s);
    if (off > (int)(BUF_SIZE * 3 / 4)) flush();
  };

  // --- Opening brace + ui flags ---
  off += snprintf(buf.data() + off, BUF_SIZE - off,
    "{\"ui_use_room_z1\":%s,\"ui_use_room_z2\":%s,",
    snap.ui_use_room_z1 ? "true" : "false",
    snap.ui_use_room_z2 ? "true" : "false");

  // --- HP sensor floats ---
  p_f("hp_feed_temp",                  snap.hp_feed_temp);
  p_f("hp_return_temp",                snap.hp_return_temp);
  p_f("outside_temp",                  snap.outside_temp);
  p_f("liquid_pipe_temp",              snap.liquid_pipe_temp);
  p_f("condensing_temp",               snap.condensing_temp);
  p_f("compressor_frequency",          snap.compressor_frequency);
  p_f("flow_rate",                     snap.flow_rate);
  p_f("computed_output_power",         snap.computed_output_power);
  p_f("daily_computed_output_power",   snap.daily_computed_output_power);
  p_f("daily_total_energy_consumption",snap.daily_total_energy_consumption);
  p_f("compressor_starts",             snap.compressor_starts);
  p_f("runtime",                       snap.runtime);
  p_f("wifi_signal_db",                snap.wifi_signal_db);
  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- DHW ---
  p_f("dhw_temp",              snap.dhw_temp);
  p_f("dhw_flow_temp_target",  snap.dhw_flow_temp_target);
  p_f("dhw_flow_temp_drop",    snap.dhw_flow_temp_drop);
  p_f("dhw_consumed",          snap.dhw_consumed);
  p_f("dhw_delivered",         snap.dhw_delivered);
  p_f("dhw_cop",               snap.dhw_cop);

  // --- Heating / cooling ---
  p_f("heating_consumed",  snap.heating_consumed);
  p_f("heating_produced",  snap.heating_produced);
  p_f("heating_cop",       snap.heating_cop);
  p_f("cooling_consumed",  snap.cooling_consumed);
  p_f("cooling_produced",  snap.cooling_produced);
  p_f("cooling_cop",       snap.cooling_cop);
  p_f("z1_flow_temp_target", snap.z1_flow_temp_target);
  p_f("z2_flow_temp_target", snap.z2_flow_temp_target);
  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- Number settings ---
  p_n("auto_adaptive_setpoint_bias",   snap.num_aa_setpoint_bias.val);
  p_lim("aa_bias_lim",                 snap.num_aa_setpoint_bias);
  p_n("maximum_heating_flow_temp",     snap.num_max_flow_temp.val);
  p_lim("max_flow_lim",                snap.num_max_flow_temp);
  p_n("minimum_heating_flow_temp",     snap.num_min_flow_temp.val);
  p_lim("min_flow_lim",                snap.num_min_flow_temp);
  p_n("maximum_heating_flow_temp_z2",  snap.num_max_flow_temp_z2.val);
  p_lim("max_flow_z2_lim",             snap.num_max_flow_temp_z2);
  p_n("minimum_heating_flow_temp_z2",  snap.num_min_flow_temp_z2.val);
  p_lim("min_flow_z2_lim",             snap.num_min_flow_temp_z2);
  p_n("thermostat_hysteresis_z1",      snap.num_hysteresis_z1.val);
  p_lim("hysteresis_z1_lim",           snap.num_hysteresis_z1);
  p_n("thermostat_hysteresis_z2",      snap.num_hysteresis_z2.val);
  p_lim("hysteresis_z2_lim",           snap.num_hysteresis_z2);
  p_n("pred_sc_time",                  snap.pred_sc_time.val);
  p_lim("pred_sc_time_lim",            snap.pred_sc_time);
  p_n("pred_sc_delta",                 snap.pred_sc_delta.val);
  p_lim("pred_sc_delta_lim",           snap.pred_sc_delta);
  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- Climate zones ---
  p_n("z1_current_temp", snap.virt_z1.curr);  p_n("z1_setpoint", snap.virt_z1.tar);
  p_act("z1_action", snap.virt_z1.action);    p_mod("z1_mode", snap.virt_z1.mode);
  p_n("z2_current_temp", snap.virt_z2.curr);  p_n("z2_setpoint", snap.virt_z2.tar);
  p_act("z2_action", snap.virt_z2.action);    p_mod("z2_mode", snap.virt_z2.mode);
  p_n("room_z1_current", snap.room_z1.curr);  p_n("room_z1_setpoint", snap.room_z1.tar);
  p_act("room_z1_action", snap.room_z1.action);
  p_n("room_z2_current", snap.room_z2.curr);  p_n("room_z2_setpoint", snap.room_z2.tar);
  p_act("room_z2_action", snap.room_z2.action);
  p_n("flow_z1_current", snap.flow_z1.curr);  p_n("flow_z1_setpoint", snap.flow_z1.tar);
  p_n("flow_z2_current", snap.flow_z2.curr);  p_n("flow_z2_setpoint", snap.flow_z2.tar);
  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- Binary / switch status ---
  p_b("status_compressor",           snap.status_compressor);
  p_b("status_booster",              snap.status_booster);
  p_b("status_defrost",              snap.status_defrost);
  p_b("status_water_pump",           snap.status_water_pump);
  p_b("status_in1_request",          snap.status_in1_request);
  p_b("status_in6_request",          snap.status_in6_request);
  p_b("zone2_enabled",               snap.status_zone2_enabled);
  p_b("pred_sc_en",                  snap.pred_sc_switch);
  p_b("auto_adaptive_control_enabled", snap.sw_auto_adaptive);
  p_b("defrost_risk_handling_enabled", snap.sw_defrost_mit);
  p_b("smart_boost_enabled",         snap.sw_smart_boost);
  p_b("force_dhw",                   snap.sw_force_dhw);
  p_b("power_mode", snap.sw_power_mode);

  // --- Cooling settings ---
  p_n("cooling_smart_start_z1",  snap.num_cooling_smart_start_z1.val);
  p_lim("cool_smart_z1_lim",     snap.num_cooling_smart_start_z1);
  p_n("minimum_cooling_flow_z1", snap.num_min_cooling_flow_z1.val);
  p_lim("min_cool_flow_z1_lim",  snap.num_min_cooling_flow_z1);
  p_n("minimum_cooling_flow_z2", snap.num_min_cooling_flow_z2.val);
  p_lim("min_cool_flow_z2_lim",  snap.num_min_cooling_flow_z2);

  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- Solver ---
  p_b("use_dynamic_cost_solver", snap.sw_use_solver);
  p_b("show_solver_tab",         snap.sw_show_solver_tab);
  p_b("solver_connected",        snap.bin_solver_connected);
  p_sel("solver_dhw_mode", snap.solver_dhw_mode);

  // --- Server control ---
  p_b("server_control_enabled",          snap.sw_server_control);
  p_b("server_control_prohibit_dhw",     snap.sw_sc_prohibit_dhw);
  p_b("server_control_prohibit_z1_heating", snap.sw_sc_prohibit_z1_heating);
  p_b("server_control_prohibit_z1_cooling", snap.sw_sc_prohibit_z1_cooling);
  p_b("server_control_prohibit_z2_heating", snap.sw_sc_prohibit_z2_heating);
  p_b("server_control_prohibit_z2_cooling", snap.sw_sc_prohibit_z2_cooling);

  // --- Raw physics EMA numbers ---
  p_n("raw_heat_produced",      snap.num_raw_heat_produced.val);
  p_lim("raw_heat_produced_lim",snap.num_raw_heat_produced);
  p_n("raw_elec_consumed",      snap.num_raw_elec_consumed.val);
  p_lim("raw_elec_consumed_lim",snap.num_raw_elec_consumed);
  p_n("raw_runtime_hours",      snap.num_raw_runtime_hours.val);
  p_lim("raw_runtime_hours_lim",snap.num_raw_runtime_hours);
  p_n("raw_avg_outside_temp",   snap.num_raw_avg_outside_temp.val);
  p_lim("raw_avg_outside_temp_lim", snap.num_raw_avg_outside_temp);
  p_n("raw_avg_room_temp",      snap.num_raw_avg_room_temp.val);
  p_lim("raw_avg_room_temp_lim",snap.num_raw_avg_room_temp);
  p_n("raw_delta_room_temp",    snap.num_raw_delta_room_temp.val);
  p_lim("raw_delta_room_temp_lim", snap.num_raw_delta_room_temp);
  p_n("raw_hl_tm_product",      snap.num_raw_hl_tm_product.val);
  p_n("raw_solar_factor",       snap.num_raw_solar_factor.val);
  p_lim("raw_solar_factor_lim", snap.num_raw_solar_factor);
  p_n("battery_soc_kwh",        snap.num_battery_soc_kwh.val);
  p_lim("battery_soc_kwh_lim",  snap.num_battery_soc_kwh);
  p_n("battery_max_discharge_kw",     snap.num_battery_max_discharge_kw.val);
  p_lim("battery_max_discharge_kw_lim", snap.num_battery_max_discharge_kw);
  p_n("dhw_start_threshold",    snap.num_dhw_start_threshold.val);
  p_lim("dhw_start_threshold_lim", snap.num_dhw_start_threshold);

  p_n("raw_cool_produced",      snap.num_raw_cool_produced.val);
  p_lim("raw_cool_produced_lim",snap.num_raw_cool_produced);
  p_n("raw_cool_elec_consumed", snap.num_raw_cool_elec_consumed.val);
  p_lim("raw_cool_elec_consumed_lim",snap.num_raw_cool_elec_consumed);
  p_n("raw_cool_runtime_hours", snap.num_raw_cool_runtime_hours.val);
  p_lim("raw_cool_runtime_hours_lim",snap.num_raw_cool_runtime_hours);
  p_n("raw_cool_avg_outside_temp",   snap.num_raw_cool_avg_outside_temp.val);
  p_lim("raw_cool_avg_outside_temp_lim", snap.num_raw_cool_avg_outside_temp);
  
  if (!flush()) { httpd_resp_send_chunk(req, nullptr, 0); return; }

  // --- String fields (solver IP, version) — escape inline into buf ---
  off += snprintf(buf.data() + off, BUF_SIZE - off, "\"solver_ip_address\":\"");
  for (char *c = snap.txt_solver_ip; *c != '\0'; ++c) {
    if      (*c == '"')  { buf[off++] = '\\'; buf[off++] = '"'; }
    else if (*c == '\\') { buf[off++] = '\\'; buf[off++] = '\\'; }
    else                 { buf[off++] = *c; }
  }
  off += snprintf(buf.data() + off, BUF_SIZE - off, "\",\"latest_version\":\"");
  for (char *c = snap.version; *c != '\0'; ++c) {
    if      (*c == '"')  { buf[off++] = '\\'; buf[off++] = '"'; }
    else if (*c == '\\') { buf[off++] = '\\'; buf[off++] = '\\'; }
    else                 { buf[off++] = *c; }
  }
  off += snprintf(buf.data() + off, BUF_SIZE - off, "\",");

  // --- operation_mode + selects + closing ---
  if (!std::isnan(snap.operation_mode))
    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"operation_mode\":%d,", (int)snap.operation_mode);
  else
    off += snprintf(buf.data() + off, BUF_SIZE - off, "\"operation_mode\":null,");

  p_sel("heating_system_type",   snap.sel_heating_system_type);
  p_sel("room_temp_source_z1",   snap.sel_room_temp_source_z1);
  p_sel("room_temp_source_z2",   snap.sel_room_temp_source_z2);
  p_sel("operating_mode_z1",     snap.sel_operating_mode_z1);
  p_sel("operating_mode_z2",     snap.sel_operating_mode_z2);
  p_sel("temp_sensor_source_z1", snap.sel_temp_source_z1);
  p_sel("temp_sensor_source_z2", snap.sel_temp_source_z2);

  off += snprintf(buf.data() + off, BUF_SIZE - off, "\"_uptime_ms\":%u}", millis());
  flush();
  httpd_resp_send_chunk(req, nullptr, 0);
}

// History handling
int16_t EcodanDashboard::pack_temp_(float val) {
  if (std::isnan(val)) return -32768; 
  return static_cast<int16_t>(val * 10.0f);
}

bool EcodanDashboard::bin_state_(binary_sensor::BinarySensor *b) {
  return (b != nullptr && b->has_state() && b->state);
}

void EcodanDashboard::record_history_() {
  MinuteRecord rec;
  
  auto ts = this->timestamp();
  if (ts == -1) {
    // no valid time yet
    ESP_LOGI(TAG, "record_history_: no valid timestamp");
    return;
  }

  rec.timestamp = ts;

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
  rec.outside     = pack_temp_(get_sensor(outside_temp_));
  rec.liquid_pipe = pack_temp_(get_sensor(liquid_pipe_temp_));
  rec.condensing  = pack_temp_(get_sensor(condensing_temp_));
  
  bool is_running = bin_state_(status_compressor_) || (get_sensor(compressor_frequency_) > 0.0f);
  bool is_thermal_active = false;
  if (operation_mode_ && operation_mode_->has_state() && !std::isnan(operation_mode_->state)) {
      int mode_val = (int)operation_mode_->state;
      is_thermal_active = (mode_val == 2 || mode_val == 1 || mode_val == 6); 
  }

  float current_cons = NAN;
  if (solver_kwh_meter_feedback_source_ != nullptr && solver_kwh_meter_feedback_source_->active_index().value_or(0) != 0) {
      current_cons = solver_kwh_meter_feedback_ ? solver_kwh_meter_feedback_->state : NAN;
  } else {
      current_cons = get_sensor(daily_total_energy_consumption_);
  }
  rec.cons = pack_temp_(current_cons);

  if (is_running && is_thermal_active) {
      rec.prod = pack_temp_(get_sensor(daily_computed_output_power_));
  } else {
      // Carry forward last known production value (stored in RAM, no disk read needed)
      rec.prod = (last_hist_prod_ != -32768) ? last_hist_prod_
                                             : pack_temp_(get_sensor(daily_computed_output_power_));
  }

  rec.flags = 0;
  if (bin_state_(status_compressor_))  rec.flags |= (1 << 0);
  if (bin_state_(status_booster_))     rec.flags |= (1 << 1);
  if (bin_state_(status_defrost_))     rec.flags |= (1 << 2);
  if (bin_state_(status_water_pump_))  rec.flags |= (1 << 3);
  if (bin_state_(status_in1_request_)) rec.flags |= (1 << 4);
  if (bin_state_(status_in6_request_)) rec.flags |= (1 << 5);

  uint8_t mode_enc = 0;
  if (operation_mode_ && operation_mode_->has_state() && !std::isnan(operation_mode_->state)) {
    int val = (int)operation_mode_->state;
    if (val != 255) mode_enc = val & 0x0F;
  }
  rec.flags |= ((mode_enc & 0x0F) << 6);

  // Append to RAM write buffer; flush to LFS when full
  bool do_flush = false;
  size_t flush_write_pos = 0;

  if (history_mutex_ != NULL && xSemaphoreTake(history_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
      last_hist_prod_ = rec.prod;  // update carry-forward
      lfs_write_buf_[lfs_write_buf_count_++] = rec;
      history_head_  = (history_head_ + 1) % MAX_MINUTES;
      if (history_count_ < MAX_MINUTES) history_count_++;

      if (lfs_write_buf_count_ >= LFS_FLUSH_COUNT) {
          // Snapshot for lfs_task_; compute where in the file to write
          flush_write_pos = (history_head_ + MAX_MINUTES - LFS_FLUSH_COUNT) % MAX_MINUTES;
          memcpy(lfs_flush_snap_, lfs_write_buf_, LFS_FLUSH_COUNT * sizeof(MinuteRecord));
          lfs_flush_snap_count_ = LFS_FLUSH_COUNT;
          lfs_flush_write_pos_  = flush_write_pos;
          lfs_write_buf_count_  = 0;
          do_flush = true;
      }
      xSemaphoreGive(history_mutex_);
  } else {
      ESP_LOGW(TAG, "record_history_: failed to acquire history_mutex_, record dropped");
  }

  if (do_flush && lfs_trigger_ != nullptr) {
      //ESP_LOGI(TAG, "record_history_: trigger flush lfs_trigger_");
      xSemaphoreGive(lfs_trigger_);
  }
}

void EcodanDashboard::handle_history_request_(AsyncWebServerRequest *request) {
  httpd_req_t *req = *request;
  
  bool is_hourly = false;
  if (request->hasParam("type") && request->getParam("type")->value() == "hour") {
      is_hourly = true;
  }

  // ── 1. Parse optional ?from=<unix>&to=<unix> query params ──
  uint32_t from_ts = 0, to_ts = 0;
  char query[64] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
      char val[16] = {0};
      if (httpd_query_key_value(query, "from", val, sizeof(val)) == ESP_OK) from_ts = (uint32_t)atol(val);
      memset(val, 0, sizeof(val));
      if (httpd_query_key_value(query, "to", val, sizeof(val)) == ESP_OK) to_ts = (uint32_t)atol(val);
  }
  // Default: last 24 hours based on Ecodan's clock
    if (from_ts == 0) {
        auto current_ts = this->timestamp();
        if (current_ts != -1) {
            from_ts = (uint32_t)(current_ts - 86400);
            to_ts   = (uint32_t)(current_ts + 60);
        } else {
            ESP_LOGI(TAG, "handle_history_request_: no valid timestamp");
        }
    }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "close");

// ── 2. HOURLY DATA BRANCH (Circular Buffer) ──
  if (is_hourly) {
      size_t current_h_count, current_h_head;
      if (history_mutex_ == NULL || xSemaphoreTake(history_mutex_, pdMS_TO_TICKS(200)) != pdTRUE) {
          httpd_resp_send_chunk(req, "[]", 2);
          httpd_resp_send_chunk(req, nullptr, 0);
          return;
      }
      current_h_count = hourly_count_;
      current_h_head  = hourly_head_;
      xSemaphoreGive(history_mutex_);

      FILE *f = fopen(LFS_HOURLY_PATH, "rb");
      if (!f || current_h_count == 0) {
          if (f) fclose(f);
          httpd_resp_send_chunk(req, "[]", 2);
          httpd_resp_send_chunk(req, nullptr, 0);
          return;
      }

      const size_t oldest_h = (current_h_count == MAX_HOURLY)
                                ? current_h_head
                                : (current_h_head + MAX_HOURLY - current_h_count) % MAX_HOURLY;

      // Binary search: first record with timestamp >= from_ts
      size_t lo = 0, hi = current_h_count;
      if (from_ts > 0) {
          while (lo < hi) {
              size_t mid = (lo + hi) / 2;
              size_t fidx = (oldest_h + mid) % MAX_HOURLY;
              uint32_t ts = 0;
              fseek(f, (long)(LFS_DATA_OFFSET + fidx * sizeof(HourlyRecord)), SEEK_SET);
              fread(&ts, sizeof(uint32_t), 1, f);
              if (ts < from_ts) lo = mid + 1; else hi = mid;
          }
      }
      const size_t first_h = lo;

      // Binary search: one past last record with timestamp <= to_ts
      lo = first_h; hi = current_h_count;
      if (to_ts > 0) {
          while (lo < hi) {
              size_t mid = (lo + hi) / 2;
              size_t fidx = (oldest_h + mid) % MAX_HOURLY;
              uint32_t ts = 0;
              fseek(f, (long)(LFS_DATA_OFFSET + fidx * sizeof(HourlyRecord)), SEEK_SET);
              fread(&ts, sizeof(uint32_t), 1, f);
              if (ts <= to_ts) lo = mid + 1; else hi = mid;
          }
      }
      const size_t last_h = lo;

      if (httpd_resp_send_chunk(req, "[", 1) != ESP_OK) { fclose(f); return; }

      // Heap-allocated batch buffer — 16 × 64 = 1 KB, safe for the httpd task stack.
      constexpr size_t H_BATCH = 16;
      auto hbatch_mem = std::unique_ptr<HourlyRecord[]>(new HourlyRecord[H_BATCH]);
      HourlyRecord* hbatch = hbatch_mem.get();
      bool first = true;
      char item[256];

      // Read one contiguous file segment sequentially (no per-record seek).
      auto send_h_segment = [&](size_t file_pos, size_t n) -> bool {
          if (fseek(f, (long)(LFS_DATA_OFFSET + file_pos * sizeof(HourlyRecord)), SEEK_SET) != 0)
              return false;
          size_t done = 0;
          while (done < n) {
              size_t chunk = std::min(n - done, H_BATCH);
              size_t got   = fread(hbatch, sizeof(HourlyRecord), chunk, f);
              for (size_t j = 0; j < got; j++) {
                  if (hbatch[j].timestamp == 0 || hbatch[j].timestamp <= 1000000000UL) continue;
                  if (!first && httpd_resp_send_chunk(req, ",", 1) != ESP_OK) return false;
                  first = false;
                  int len = snprintf(item, sizeof(item),
                      "[%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                      hbatch[j].timestamp, hbatch[j].avg_outside,
                      hbatch[j].total_cons, hbatch[j].total_prod,
                      hbatch[j].odin_heat_loss, hbatch[j].odin_cop,
                      hbatch[j].odin_cost, hbatch[j].odin_solar,
                      hbatch[j].exp_cons, hbatch[j].exp_prod,
                      hbatch[j].exp_room_temp, hbatch[j].actual_room_temp,
                      hbatch[j].actual_dhw_cons, hbatch[j].actual_dhw_prod,
                      hbatch[j].actual_standby_cons, hbatch[j].price,
                      hbatch[j].weather, hbatch[j].batt_discharge,
                      hbatch[j].op_mode, hbatch[j].sched_base,
                      hbatch[j].sched_min, hbatch[j].sched_max,
                      hbatch[j].exp_solar_kwh);
                  if (httpd_resp_send_chunk(req, item, len) != ESP_OK) return false;
              }
              done += got;
              if (got < chunk) break;
              vTaskDelay(0); // cooperative yield per batch, no hard delay
          }
          return true;
      };

      if (first_h < last_h) {
          const size_t count    = last_h - first_h;
          const size_t seg1_pos = (oldest_h + first_h) % MAX_HOURLY;
          const size_t seg1_len = std::min(count, MAX_HOURLY - seg1_pos);
          const size_t seg2_len = count - seg1_len;
          if (!send_h_segment(seg1_pos, seg1_len)) { fclose(f); return; }
          if (seg2_len > 0 && !send_h_segment(0, seg2_len)) { fclose(f); return; }
      }

      fclose(f);
      httpd_resp_send_chunk(req, "]", 1);
      httpd_resp_send_chunk(req, nullptr, 0);
      return;
  }

  // ── 3. MINUTE DATA BRANCH (Circular Buffer) ──
  size_t current_count, current_head;
  if (history_mutex_ == NULL || xSemaphoreTake(history_mutex_, pdMS_TO_TICKS(200)) != pdTRUE) {
      httpd_resp_send_chunk(req, "[]", 2);
      httpd_resp_send_chunk(req, nullptr, 0);
      return;
  }
  current_count  = history_count_;
  current_head   = history_head_;
  http_wb_count_ = lfs_write_buf_count_;
  if (http_wb_count_ > 0) memcpy(http_wb_snap_, lfs_write_buf_, http_wb_count_ * sizeof(MinuteRecord));
  xSemaphoreGive(history_mutex_);

  if (current_count == 0 && http_wb_count_ == 0) {
      httpd_resp_send_chunk(req, "[]", 2);
      httpd_resp_send_chunk(req, nullptr, 0);
      return;
  }

  FILE *f = fopen(LFS_MINUTES_PATH, "rb");
  if (!f && current_count > 0) {
      httpd_resp_send_chunk(req, "[]", 2);
      httpd_resp_send_chunk(req, nullptr, 0);
      return;
  }

  auto rec_offset = [](size_t idx) -> long { return (long)(LFS_DATA_OFFSET + idx * sizeof(MinuteRecord)); };
  const size_t oldest_idx = (current_count == MAX_MINUTES) ? current_head : (current_head + MAX_MINUTES - current_count) % MAX_MINUTES;

  size_t lo = 0, hi = current_count;
  if (f && current_count > 0) {
      while (lo < hi) {
          size_t mid = (lo + hi) / 2;
          size_t file_idx = (oldest_idx + mid) % MAX_MINUTES;
          uint32_t ts = 0;
          fseek(f, rec_offset(file_idx), SEEK_SET);
          fread(&ts, sizeof(uint32_t), 1, f);
          if (ts < from_ts) lo = mid + 1; else hi = mid;
      }
  }
  size_t first_idx = lo;

  size_t lo2 = first_idx, hi2 = current_count;
  if (f && current_count > first_idx) {
      while (lo2 < hi2) {
          size_t mid = (lo2 + hi2) / 2;
          size_t file_idx = (oldest_idx + mid) % MAX_MINUTES;
          uint32_t ts = 0;
          fseek(f, rec_offset(file_idx), SEEK_SET);
          fread(&ts, sizeof(uint32_t), 1, f);
          if (ts <= to_ts) lo2 = mid + 1; else hi2 = mid;
      }
  }
  size_t last_idx = lo2;

  size_t wb_in_range = 0;
  for (size_t i = 0; i < http_wb_count_; i++) {
      if (http_wb_snap_[i].timestamp >= from_ts && http_wb_snap_[i].timestamp <= to_ts) wb_in_range++;
  }
  size_t total_in_range = (last_idx - first_idx) + wb_in_range;
  size_t step = (total_in_range > 1440) ? (total_in_range / 1440) : 1;

  if (httpd_resp_send_chunk(req, "[", 1) != ESP_OK) { if (f) fclose(f); return; }

  constexpr size_t OUT_BUF_SIZE = 2048;
  auto out_buf = std::unique_ptr<char[]>(new char[OUT_BUF_SIZE]);
  int out_len = 0;

  auto flush_out = [&]() -> bool {
      if (out_len <= 0) return true;
      bool ok = (httpd_resp_send_chunk(req, out_buf.get(), out_len) == ESP_OK);
      out_len = 0;
      return ok;
  };

  bool first = true;
  auto send_record = [&](const MinuteRecord &rec) -> bool {
      if (out_len + 170 >= OUT_BUF_SIZE) {
          if (!flush_out()) return false;
      }

      if (!first) out_buf.get()[out_len++] = ',';
      first = false;

      int len = snprintf(out_buf.get() + out_len, OUT_BUF_SIZE - out_len,
          "[%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%d]",
          rec.timestamp, rec.hp_feed, rec.hp_return,
          rec.z1_sp, rec.z2_sp, rec.z1_curr, rec.z2_curr,
          rec.z1_flow, rec.z2_flow, rec.freq, rec.flags,
          rec.cons, rec.prod, rec.outside, rec.liquid_pipe, rec.condensing);
      
      out_len += len;
      return true;
  };

  if (f && first_idx < last_idx) {
      // Heap-allocated batch buffer — 16 × 64 = 1 KB, safe for the httpd task stack.
      constexpr size_t M_BATCH = 16;
      auto mbatch_mem = std::unique_ptr<MinuteRecord[]>(new MinuteRecord[M_BATCH]);
      MinuteRecord* mbatch = mbatch_mem.get();
      const size_t count    = last_idx - first_idx;
      const size_t seg1_pos = (oldest_idx + first_idx) % MAX_MINUTES;
      const size_t seg1_len = std::min(count, MAX_MINUTES - seg1_pos);
      const size_t seg2_len = count - seg1_len;

      // Read a contiguous file segment sequentially, sampling every `step` records.
      // logical_base keeps step-sampling consistent across the wrap boundary.
      auto read_m_segment = [&](size_t file_pos, size_t n, size_t logical_base) -> bool {
          if (fseek(f, (long)(LFS_DATA_OFFSET + file_pos * sizeof(MinuteRecord)), SEEK_SET) != 0)
              return false;
          size_t done = 0;
          while (done < n) {
              size_t chunk = std::min(n - done, M_BATCH);
              size_t got   = fread(mbatch, sizeof(MinuteRecord), chunk, f);
              for (size_t j = 0; j < got; j++) {
                  if ((logical_base + done + j) % step != 0) continue;
                  if (mbatch[j].timestamp < from_ts || mbatch[j].timestamp > to_ts) continue;
                  if (!send_record(mbatch[j])) return false;
              }
              done += got;
              if (got < chunk) break;
              vTaskDelay(0); // cooperative yield per batch, no hard delay
          }
          return true;
      };

      if (!read_m_segment(seg1_pos, seg1_len, 0)) { fclose(f); return; }
      if (seg2_len > 0 && !read_m_segment(0, seg2_len, seg1_len)) { fclose(f); return; }
  }
  if (f) fclose(f);

  for (size_t i = 0; i < http_wb_count_; i++) {
      if (http_wb_snap_[i].timestamp >= from_ts && http_wb_snap_[i].timestamp <= to_ts)
          if (!send_record(http_wb_snap_[i])) return;
  }

  flush_out();
  httpd_resp_send_chunk(req, "]", 1);
  httpd_resp_send_chunk(req, nullptr, 0);
}

void EcodanDashboard::align_odin_day_(int current_day) {
    // Ignore invalid days or boot state (-1)
    if (current_day < 1 || current_day > 366 || this->odin_stored_day_ < 1) return;

    if (current_day != this->odin_stored_day_) {
        int day_delta = current_day - this->odin_stored_day_;
        
        // Check for normal +1 day progression (or year wrap-around)
        if (day_delta == 1 || day_delta == -364 || day_delta == -365) {
            ESP_LOGI(TAG, "ODIN day transition (%d -> %d): shifting 72h window", this->odin_stored_day_, current_day);
            auto shift_arr = [](std::vector<float>& v, float fill_val) {
                if (v.size() != 72) return;
                // Shift today and tomorrow -> yesterday and today
                for (int i = 0; i < 48; i++) v[i] = v[i + 24];
                // Clear the new tomorrow
                for (int i = 48; i < 72; i++) v[i] = fill_val;
            };
            
            shift_arr(this->odin_expected_end_temp_, NAN);
            shift_arr(this->odin_energy_, NAN);
            shift_arr(this->odin_production_, NAN);
            shift_arr(this->odin_expected_temp_, NAN);
            shift_arr(this->odin_cost_, NAN);
            shift_arr(this->odin_battery_discharge_, 0.0f);
            shift_arr(this->odin_sched_base_, 0.0f);
            shift_arr(this->odin_sched_min_, 0.0f);
            shift_arr(this->odin_sched_max_, 0.0f);
            shift_arr(this->odin_weather_, 0.0f);
            shift_arr(this->odin_solar_, 0.0f);
            shift_arr(this->odin_prices_, 0.0f);
            shift_arr(this->odin_operation_mode_, 0.0f);
            
            shift_arr(this->odin_actual_dhw_cons_, NAN);
            shift_arr(this->odin_actual_dhw_prod_, NAN);
            shift_arr(this->odin_actual_cons_, NAN);
            shift_arr(this->odin_actual_prod_, NAN);
            shift_arr(this->odin_actual_room_, NAN);
            shift_arr(this->odin_actual_standby_cons_, NAN);
        } else {
            // Massive jump (e.g. device was off for days). Clear stale actuals.
            ESP_LOGI(TAG, "ODIN day jump (%d -> %d): clearing old actuals", this->odin_stored_day_, current_day);
            this->odin_actual_dhw_cons_.assign(72, NAN);
            this->odin_actual_dhw_prod_.assign(72, NAN);
            this->odin_actual_cons_.assign(72, NAN);
            this->odin_actual_prod_.assign(72, NAN);
            this->odin_actual_room_.assign(72, NAN);
            this->odin_actual_standby_cons_.assign(72, NAN);
        }
        
        this->odin_stored_day_ = current_day;
        this->odin_lfs_dirty_ = true;
    }
}

void EcodanDashboard::update_actual_data(int hour, int day, float actual_cons_kwh, float actual_prod_kwh, float dhw_cons, float dhw_prod, float actual_room_temp, float standby_cons) {
    if (snapshot_mutex_ == NULL || xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;

    if (!std::isnan(actual_cons_kwh)) {
        if (std::isnan(dhw_cons)) dhw_cons = 0.0f;
        if (std::isnan(standby_cons)) standby_cons = 0.0f;
    }
    if (!std::isnan(actual_prod_kwh)) {
        if (std::isnan(dhw_prod)) dhw_prod = 0.0f;
    }
    
    // 1. Ensure the 72h window is properly aligned to today BEFORE we update arrays
    align_odin_day_(day);

    int target_idx = 24 + hour; 
    float hour_cost = NAN, hour_solar = NAN;
    float exp_cons = NAN, exp_prod = NAN, exp_room = NAN, price = NAN;
    float weather = NAN, batt_dis = NAN, op_mode = NAN;
    float s_base = NAN, s_min = NAN, s_max = NAN;

    if (target_idx >= 0 && target_idx < 72) {
        this->odin_actual_cons_[target_idx] = actual_cons_kwh;
        this->odin_actual_prod_[target_idx] = actual_prod_kwh;
        this->odin_actual_dhw_cons_[target_idx] = dhw_cons;
        this->odin_actual_dhw_prod_[target_idx] = dhw_prod;
        this->odin_actual_room_[target_idx] = actual_room_temp;
        this->odin_actual_standby_cons_[target_idx] = standby_cons;

        if (this->odin_operation_mode_.size() == 72) {
            float real_mode = 0.0f; // Default OFF (0) / Standby
            if (!std::isnan(dhw_cons) && dhw_cons > 0.03f) {
                real_mode = 1.0f; // DHW / Legionella
            } 
            else if (!std::isnan(actual_cons_kwh) && actual_cons_kwh > 0.05f) {
                float inst_mode = (operation_mode_ && operation_mode_->has_state()) ? operation_mode_->state : NAN;
                
                // Modes: 1=DHW, 2=Heat, 3=Cool, 6=Legionella
                if (inst_mode == 6.0f || inst_mode == 1.0f) {
                    real_mode = 1.0f; // Legionella / DHW fallback
                } else if (inst_mode == 3.0f) {
                    real_mode = 3.0f;
                } else {
                    real_mode = 2.0f;
                }
            }
            this->odin_operation_mode_[target_idx] = real_mode;
        }

        // Extract planned data for this specific hour
        if (this->odin_cost_.size() == 72) hour_cost = this->odin_cost_[target_idx];
        if (this->odin_solar_.size() == 72) hour_solar = this->odin_solar_[target_idx];
        if (this->odin_energy_.size() == 72) exp_cons = this->odin_energy_[target_idx];
        if (this->odin_production_.size() == 72) exp_prod = this->odin_production_[target_idx];
        if (this->odin_expected_temp_.size() == 72) exp_room = this->odin_expected_temp_[target_idx];
        if (this->odin_prices_.size() == 72) price = this->odin_prices_[target_idx];
        if (this->odin_weather_.size() == 72) weather = this->odin_weather_[target_idx];
        if (this->odin_battery_discharge_.size() == 72) batt_dis = this->odin_battery_discharge_[target_idx];
        if (this->odin_operation_mode_.size() == 72) op_mode = this->odin_operation_mode_[target_idx];
        if (this->odin_sched_base_.size() == 72) s_base = this->odin_sched_base_[target_idx];
        if (this->odin_sched_min_.size() == 72) s_min = this->odin_sched_min_[target_idx];
        if (this->odin_sched_max_.size() == 72) s_max = this->odin_sched_max_[target_idx];
    }

    // --- Create and Append Hourly Record (The Odin Historical Data) ---
    auto pack = [](float v, float scale) -> int16_t {
        if (std::isnan(v)) return -32768;
        float s = v * scale;
        if (s > 32767.0f) return 32767;
        if (s < -32767.0f) return -32767;
        return static_cast<int16_t>(s);
    };

    auto ts = this->timestamp();
    if (ts == -1) {
        ESP_LOGI(TAG, "update_actual_data: no valid timestamp");
        xSemaphoreGive(snapshot_mutex_);
        return;
    }

    HourlyRecord hr{};
    hr.timestamp = ts;
    hr.avg_outside = pack_temp_(outside_temp_ && outside_temp_->has_state() ? outside_temp_->state : NAN);
    hr.total_cons = pack(actual_cons_kwh, 100.0f);
    hr.total_prod = pack(actual_prod_kwh, 100.0f);
    
    // Global solver stats
    hr.odin_heat_loss = pack(last_run_stats_.heat_loss, 100.0f);
    hr.odin_cop = pack(last_run_stats_.base_cop, 100.0f);
    
    // Extracted hour-specific data
    hr.odin_cost = pack(hour_cost, 100.0f);
    hr.odin_solar = pack(hour_solar, 10.0f);
    hr.exp_cons = pack(exp_cons, 100.0f);
    hr.exp_prod = pack(exp_prod, 100.0f);
    hr.exp_room_temp = pack(exp_room, 100.0f);
    hr.actual_room_temp = pack(actual_room_temp, 100.0f);
    hr.actual_dhw_cons = pack(dhw_cons, 100.0f);
    hr.actual_dhw_prod = pack(dhw_prod, 100.0f);
    hr.actual_standby_cons = pack(standby_cons, 100.0f);
    hr.price = pack(price, 10000.0f); // High precision for small EUR/kWh values
    hr.weather = pack(weather, 100.0f);
    hr.batt_discharge = pack(batt_dis, 100.0f);
    hr.op_mode = pack(op_mode, 1.0f);
    hr.sched_base = pack(s_base, 100.0f);
    hr.sched_min = pack(s_min, 100.0f);
    hr.sched_max = pack(s_max, 100.0f);

    {
        float exp_solar_kwh = NAN;
        if (!std::isnan(hour_solar) && last_run_stats_.used_solar_kwp > 0.0f) {
            exp_solar_kwh = (hour_solar / 1000.0f)
                            * last_run_stats_.used_solar_kwp
                            * last_run_stats_.used_solar_correction;
        }
        hr.exp_solar_kwh = pack(exp_solar_kwh, 100.0f);
    }

    memset(hr.reserved, 0, sizeof(hr.reserved));

    xSemaphoreGive(snapshot_mutex_);
    
    record_hourly_data(hr);
    this->odin_lfs_dirty_ = true; 
}

void EcodanDashboard::store_odin_data(int current_hour, int current_day,
                                      const std::vector<float>& expected_end_temp,
                                      const std::vector<float>& energy,
                                      const std::vector<float>& production,
                                      const std::vector<float>& exp_temp,
                                      const std::vector<float>& cost,
                                      const std::vector<float>& battery_discharge,
                                      const std::vector<float>& sched_base,
                                      const std::vector<float>& sched_min,
                                      const std::vector<float>& sched_max,
                                      const std::vector<float>& weather,
                                      const std::vector<float>& solar,
                                      const std::vector<float>& prices,
                                      const std::vector<float>& op_mode,
                                      const LastRunStats& run_stats) {
    if (current_hour < 0) return;

    if (this->snapshot_mutex_ == NULL ||
        xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire snapshot mutex during ODIN store.");
        return;
    }

    auto ensure_72 = [](std::vector<float>& v, float fill_val) {
        if (v.size() != 72) v.assign(72, fill_val);
    };

    ensure_72(this->odin_expected_end_temp_, NAN);
    ensure_72(this->odin_energy_, NAN);
    ensure_72(this->odin_production_, NAN);
    ensure_72(this->odin_expected_temp_, NAN);
    ensure_72(this->odin_cost_, NAN);
    ensure_72(this->odin_actual_dhw_cons_, NAN);
    ensure_72(this->odin_actual_dhw_prod_, NAN);
    ensure_72(this->odin_actual_cons_, NAN);
    ensure_72(this->odin_actual_prod_, NAN);
    ensure_72(this->odin_actual_room_, NAN);
    ensure_72(this->odin_actual_standby_cons_, NAN);

    ensure_72(this->odin_battery_discharge_, 0.0f);
    ensure_72(this->odin_sched_base_, 0.0f);
    ensure_72(this->odin_sched_min_, 0.0f);
    ensure_72(this->odin_sched_max_, 0.0f);
    ensure_72(this->odin_weather_, 0.0f);
    ensure_72(this->odin_solar_, 0.0f);
    ensure_72(this->odin_prices_, 0.0f);
    ensure_72(this->odin_operation_mode_, 0.0f);

    if (!this->odin_data_ready_) {
        this->odin_data_ready_ = true;
        this->odin_stored_day_ = current_day;
    }

    // 1. Shift the arrays if the day has changed since the last update
    align_odin_day_(current_day);
    
    // 2. Unified Data Update
    for (int i = 0; i < 48; i++) {
        int target_idx = 24 + i;   // offset into 72-slot window

        // Static data (always overwrite — comes from fresh forecast)
        if (i < (int)sched_base.size() && !std::isnan(sched_base[i]))  this->odin_sched_base_[target_idx] = sched_base[i];
        if (i < (int)sched_min.size()  && !std::isnan(sched_min[i]))   this->odin_sched_min_[target_idx]  = sched_min[i];
        if (i < (int)sched_max.size()  && !std::isnan(sched_max[i]))   this->odin_sched_max_[target_idx]  = sched_max[i];
        if (i < (int)weather.size()    && !std::isnan(weather[i]))     this->odin_weather_[target_idx]    = weather[i];
        if (i < (int)solar.size()      && !std::isnan(solar[i]))       this->odin_solar_[target_idx]      = solar[i];
        if (i < (int)prices.size()     && !std::isnan(prices[i]))      this->odin_prices_[target_idx]     = prices[i];

        // Calculated data (only overwrite future hours or empty slots to protect "past" history)
        bool is_future = (i > current_hour); 
        bool is_empty_slot = std::isnan(this->odin_production_[target_idx]);

        if (is_future || is_empty_slot) {
            if (i < (int)energy.size()     && !std::isnan(energy[i]))      this->odin_energy_[target_idx]            = energy[i];
            if (i < (int)production.size() && !std::isnan(production[i]))  this->odin_production_[target_idx]        = production[i];
            if (i < (int)cost.size()       && !std::isnan(cost[i]))        this->odin_cost_[target_idx]              = cost[i];
            if (i < (int)battery_discharge.size() && !std::isnan(battery_discharge[i])) this->odin_battery_discharge_[target_idx] = battery_discharge[i];
            if (i < (int)op_mode.size()    && !std::isnan(op_mode[i]))     this->odin_operation_mode_[target_idx]    = op_mode[i];
            if (i < (int)exp_temp.size()   && !std::isnan(exp_temp[i]))    this->odin_expected_temp_[target_idx]     = exp_temp[i];
            if (i < (int)expected_end_temp.size() && !std::isnan(expected_end_temp[i])) this->odin_expected_end_temp_[target_idx] = expected_end_temp[i];
        }
    }

    this->last_run_stats_ = run_stats;

    xSemaphoreGive(this->snapshot_mutex_);
    ESP_LOGI(TAG, "ODIN arrays stored (hour=%d, day=%d)", current_hour, current_day);
    this->odin_lfs_dirty_ = true;
}

void EcodanDashboard::handle_odin_request_(AsyncWebServerRequest *request) {
  constexpr int JSON_BUFFER_SIZE = 4096;
  constexpr size_t ODIN_HOURS = 72;

  httpd_req_t *req = *request;
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
  httpd_resp_set_hdr(req, "Connection", "close");

  bool is_ready = false;
  if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
      is_ready = this->odin_data_ready_;
      xSemaphoreGive(snapshot_mutex_);
  }

  if (!is_ready) {
      httpd_resp_send_chunk(req, "{\"success\":false}", 17);
      httpd_resp_send_chunk(req, nullptr, 0);
      return;
  }

  if (httpd_resp_send_chunk(req, "{\"success\":true,", 16) != ESP_OK) return;

  // pre allocate buffer
  std::vector<char> buffer_vec(JSON_BUFFER_SIZE);
  char* json_buf = buffer_vec.data();

  // Heap-allocate the snapshot array (19 × 72 floats = ~5.5 KB — too large for the task stack).
  constexpr int ODIN_ARRAY_COUNT = 19;
  struct OdinSnapshotData { float arrs[ODIN_ARRAY_COUNT][ODIN_HOURS]; };
  auto all_arrs_ptr = std::unique_ptr<OdinSnapshotData>(new OdinSnapshotData());
  auto& all_arrs = all_arrs_ptr->arrs;

  // Copy all ODIN arrays in a single mutex acquisition using the canonical map.
  auto map = odin_array_map_();
  bool arr_valid[ODIN_ARRAY_COUNT] = {};

  {
      if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
          for (int k = 0; k < ODIN_ARRAY_COUNT; k++) {
              if (map[k].vec->size() == ODIN_HOURS) {
                  memcpy(all_arrs[k], map[k].vec->data(), ODIN_HOURS * sizeof(float));
                  arr_valid[k] = true;
              }
          }
          xSemaphoreGive(snapshot_mutex_);
      }
  }

  // Helper: serialise one pre-copied array into json_buf and send as a chunk.
  auto send_arr_chunk = [&](int k, const char* name) __attribute__((noinline)) -> bool {
      if (!arr_valid[k]) return true; // skip silently; array had wrong size

      int offset = snprintf(json_buf, JSON_BUFFER_SIZE, "\"%s\":[", name);
      for (size_t i = 0; i < ODIN_HOURS; i++) {
          int space_left = (JSON_BUFFER_SIZE > offset) ? (JSON_BUFFER_SIZE - offset) : 0;
          if (std::isnan(all_arrs[k][i])) {
              offset += snprintf(json_buf + offset, space_left, "null");
          } else {
              offset += snprintf(json_buf + offset, space_left, "%.2f", all_arrs[k][i]);
          }
          space_left = (JSON_BUFFER_SIZE > offset) ? (JSON_BUFFER_SIZE - offset) : 0;
          if (i < ODIN_HOURS - 1) offset += snprintf(json_buf + offset, space_left, ",");
      }
      int space_left = (JSON_BUFFER_SIZE > offset) ? (JSON_BUFFER_SIZE - offset) : 0;
      offset += snprintf(json_buf + offset, space_left, "],"); // always comma; stats block follows
      if (offset >= JSON_BUFFER_SIZE) offset = JSON_BUFFER_SIZE - 1;
      return (httpd_resp_send_chunk(req, json_buf, offset) == ESP_OK);
  };

  bool success = true;
  for (int k = 0; k < ODIN_ARRAY_COUNT && success; k++)
      success = send_arr_chunk(k, map[k].name);

  if (success) {
      LastRunStats stats;
      if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
          stats = this->last_run_stats_;
          xSemaphoreGive(snapshot_mutex_);
      }

      int offset = snprintf(json_buf, JSON_BUFFER_SIZE,
          "\"current_hour\":%d,"
          "\"today_start_index\":24,"
          "\"last_run\":{\"execution_ms\":%u,\"evaluated_nodes\":%u,\"bidding_zone\":\"%s\",\"heat_loss\":%.3f,\"base_cop\":%.2f,"
          "\"thermal_mass\":%.1f,\"exp_consumption\":%.2f,\"exp_production\":%.2f,"
          "\"exp_solar\":%.2f,\"exp_solar_total\":%.2f,\"used_solar_kwp\":%.2f,"
          "\"used_solar_correction\":%.3f,\"used_battery_soc_kwh\":%.2f,\"total_cost\":%.4f}}",
          stats.current_hour,
          stats.execution_ms,
          stats.evaluated_nodes,
          stats.bidding_zone.c_str(),
          stats.heat_loss,
          stats.base_cop,
          stats.thermal_mass,
          stats.exp_consumption,
          stats.exp_production,
          stats.exp_solar,
          stats.exp_solar_total,
          stats.used_solar_kwp,
          stats.used_solar_correction,
          stats.used_battery_soc_kwh,
          stats.total_cost);

      if (offset > 0 && offset < JSON_BUFFER_SIZE) {
          httpd_resp_send_chunk(req, json_buf, offset);
      }
  }

  httpd_resp_send_chunk(req, nullptr, 0);
}

}  // namespace asgard_dashboard
}  // namespace esphome
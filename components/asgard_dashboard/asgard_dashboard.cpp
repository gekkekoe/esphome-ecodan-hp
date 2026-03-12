#include "asgard_dashboard.h"
#include "dashboard_html.h"
#include "dashboard_js.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <esp_http_server.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "nvs_flash.h"
#include "nvs.h"

// #include "esp_system.h"
// #include "esp_heap_caps.h"

namespace esphome {
namespace asgard_dashboard {

static const char *const TAG = "asgard_dashboard";

void EcodanDashboard::setup() {
  ESP_LOGI(TAG, "Setting up Ecodan Dashboard on /dashboard");
  
  action_lock_ = xSemaphoreCreateMutex();
  snapshot_mutex_ = xSemaphoreCreateMutex();

  // Restore last-known ODIN arrays from flash so charts survive reboot
  this->nvs_load_odin_();
  this->odin_nvs_last_write_ms_ = millis();

  base_->init();
  base_->add_handler(this);
}

void EcodanDashboard::loop() {

  uint32_t now = millis();
  if (now - last_history_time_ >= 60000 || last_history_time_ == 0) {
    last_history_time_ = now;
    record_history_();
  }

  // update state every second
  if (now - last_snapshot_time_ >= 1000 || last_snapshot_time_ == 0) {
    last_snapshot_time_ = now;
    update_snapshot_();
  }

  // Flush ODIN arrays to NVS if dirty, max once per 10 minutes
  if (this->odin_nvs_dirty_) {
    const uint32_t NVS_FLUSH_INTERVAL_MS = 10 * 60 * 1000;
    if (this->odin_nvs_last_write_ms_ == 0 || (now - this->odin_nvs_last_write_ms_) >= NVS_FLUSH_INTERVAL_MS) {
      this->nvs_persist_odin_();
      this->odin_nvs_dirty_ = false;
      this->odin_nvs_last_write_ms_ = now;
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
  const auto& url = request->url();
  return (url == "/dashboard" || url == "/dashboard/" ||
          url == "/dashboard/state" || url == "/dashboard/set" ||
          url == "/dashboard/history" || url == "/dashboard/odin" ||
          url == "/js/chart.js" || url == "/js/hammer.js" || url == "/js/zoom.js"); 
}

void EcodanDashboard::handleRequest(AsyncWebServerRequest *request) {
  const auto& url = request->url();
  
  if      (url == "/dashboard" || url == "/dashboard/") handle_root_(request);
  else if (url == "/dashboard/state")                   handle_state_(request);
  else if (url == "/dashboard/set")                     handle_set_(request);
  else if (url == "/dashboard/history")                 handle_history_request_(request);
  else if (url == "/dashboard/odin")                    handle_odin_request_(request);
  else if (url == "/js/chart.js" || url == "/js/hammer.js" || url == "/js/zoom.js") {
    handle_js_(request);
  }
  else {
    request->send(404, "text/plain", "Not found");
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

  // Send the compressed array in safe chunks
  size_t index = 0;
  size_t remaining = length;
  const size_t chunk_size = 2048;

  while (remaining > 0) {
    size_t to_send = (remaining < chunk_size) ? remaining : chunk_size;
    
    httpd_resp_send_chunk(req, (const char*)(data + index), to_send);
    
    index += to_send;
    remaining -= to_send;
  }

  httpd_resp_send_chunk(req, nullptr, 0);
}

void EcodanDashboard::handle_root_(AsyncWebServerRequest *request) {
  // uint32_t free_heap = esp_get_free_heap_size();
  // uint32_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  // ESP_LOGI(TAG, "handle_root_: \t\t\tMemory Stats | Total Free: %u bytes | Largest Block: %u bytes", free_heap, max_block);
  send_chunked_(request, "text/html", DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN, "no-cache");
}

void EcodanDashboard::handle_js_(AsyncWebServerRequest *request) {
  // uint32_t free_heap = esp_get_free_heap_size();
  // uint32_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  // ESP_LOGI(TAG, "handle_js_: \t\t\tMemory Stats | Total Free: %u bytes | Largest Block: %u bytes", free_heap, max_block);

  const auto& url = request->url();
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
    request->send(404, "text/plain", "File not found");
    return;
  }

  send_chunked_(request, "application/javascript", file_data, file_len, "public, max-age=31536000");
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

  if (action_lock_ != NULL && xSemaphoreTake(action_lock_, pdMS_TO_TICKS(100)) == pdTRUE) {
    action_queue_.push_back({
        std::string(key),
        std::string(strval),
        fval,
        is_string
    });
    xSemaphoreGive(action_lock_);
    request->send(200, "application/json", "{\"ok\":true}");
  } else {
    ESP_LOGW(TAG, "Failed to acquire action_lock_ in handle_set_ (Timeout)");
    request->send(503, "text/plain", "System busy, try again");
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
  if (key == "predictive_short_cycle_control_enabled") { doSwitch(pred_sc_switch_);   return; }
  if (key == "use_dynamic_cost_solver") { doSwitch(sw_use_solver_); return; }

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
  if (key == "cooling_smart_start_z1")       { doNumber(num_cooling_smart_start_z1_); return; }
  if (key == "minimum_cooling_flow_z1")      { doNumber(num_min_cooling_flow_z1_); return; }

  if (key == "thermostat_hysteresis_z1")    { doNumber(num_hysteresis_z1_);    return; }
  if (key == "thermostat_hysteresis_z2")    { doNumber(num_hysteresis_z2_);    return; }
  if (key == "raw_heat_produced") { doNumber(num_raw_heat_produced_); return; }
  if (key == "raw_elec_consumed") { doNumber(num_raw_elec_consumed_); return; }
  if (key == "raw_runtime_hours") { doNumber(num_raw_runtime_hours_); return; }
  if (key == "raw_avg_outside_temp") { doNumber(num_raw_avg_outside_temp_); return; }
  if (key == "raw_avg_room_temp") { doNumber(num_raw_avg_room_temp_); return; }
  if (key == "raw_delta_room_temp") { doNumber(num_raw_delta_room_temp_); return; }
  if (key == "raw_max_output") { doNumber(num_raw_max_output_); return; }
  if (key == "raw_hl_tm_product") { doNumber(num_raw_hl_tm_product_); return; }
  if (key == "battery_soc_kwh") { doNumber(num_battery_soc_kwh_); return; }
  if (key == "battery_max_discharge_kw") { doNumber(num_battery_max_discharge_kw_); return; }

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

  // Add support for updating TextSensors via the dashboard
  if (key == "solver_ip_address" && txt_solver_ip_ != nullptr && is_string) {
    txt_solver_ip_->publish_state(sval);
    ESP_LOGI(TAG, "Solver IP Address set to: %s", sval.c_str());
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

  // Populate Floats
  current_snapshot_.hp_feed_temp = get_f(hp_feed_temp_);
  current_snapshot_.hp_return_temp = get_f(hp_return_temp_);
  current_snapshot_.outside_temp = get_f(outside_temp_);
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

  // solver data
  get_n(num_raw_heat_produced_, current_snapshot_.num_raw_heat_produced);
  get_n(num_raw_elec_consumed_, current_snapshot_.num_raw_elec_consumed);
  get_n(num_raw_runtime_hours_, current_snapshot_.num_raw_runtime_hours);
  get_n(num_raw_avg_outside_temp_, current_snapshot_.num_raw_avg_outside_temp);
  get_n(num_raw_avg_room_temp_, current_snapshot_.num_raw_avg_room_temp);
  get_n(num_raw_delta_room_temp_, current_snapshot_.num_raw_delta_room_temp);
  get_n(num_raw_max_output_, current_snapshot_.num_raw_max_output);
  get_n(num_raw_hl_tm_product_, current_snapshot_.num_raw_hl_tm_product);

  get_n(num_battery_soc_kwh_, current_snapshot_.num_battery_soc_kwh);
  get_n(num_battery_max_discharge_kw_, current_snapshot_.num_battery_max_discharge_kw);

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
  // uint32_t free_heap = esp_get_free_heap_size();
  // uint32_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  // ESP_LOGI(TAG, "handle_state_: \t\t\tMemory Stats | Total Free: %u bytes | Largest Block: %u bytes", free_heap, max_block);

  DashboardSnapshot snap;
  
  if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
    snap = current_snapshot_;
    xSemaphoreGive(snapshot_mutex_);
  } else {
    ESP_LOGW(TAG, "Failed to acquire snapshot_mutex_ in handle_state_ (Timeout)");
    request->send(503, "text/plain", "System busy, try again");
    return;
  }

  AsyncResponseStream *response = request->beginResponseStream("application/json");
  if (response == nullptr) {
    request->send(500, "text/plain", "Stream allocation failed");
    return;
  }

  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Cache-Control", "no-cache");
  response->print("{");

  // Helper lambdas for outputting JSON using the snapshot struct
  auto p_f = [&](const char* k, float v) {
    if (!std::isnan(v)) response->printf("\"%s\":%.2f,", k, v);
    else response->printf("\"%s\":null,", k);
  };

  auto p_b = [&](const char* k, bool v) {
    response->printf("\"%s\":%s,", k, v ? "true" : "false");
  };


  auto p_n = [&](const char* k, float v) {
    if (!std::isnan(v)) response->printf("\"%s\":%.1f,", k, v);
    else response->printf("\"%s\":null,", k);
  };
  
  auto p_lim = [&](const char* k, const DashboardSnapshot::NumData& d) {
    if (!std::isnan(d.min)) response->printf("\"%s\":{\"min\":%.1f,\"max\":%.1f,\"step\":%.1f},", k, d.min, d.max, d.step);
    else response->printf("\"%s\":null,", k);
  };
  
  auto p_sel = [&](const char* k, int v) {
    if (v >= 0) response->printf("\"%s\":\"%d\",", k, v);
    else response->printf("\"%s\":null,", k);
  };
  
  auto p_act = [&](const char* k, int a) {
    if (a < 0) response->printf("\"%s\":\"off\",", k);
    else if (a == climate::CLIMATE_ACTION_OFF) response->printf("\"%s\":\"off\",", k);
    else if (a == climate::CLIMATE_ACTION_COOLING) response->printf("\"%s\":\"cooling\",", k);
    else if (a == climate::CLIMATE_ACTION_HEATING) response->printf("\"%s\":\"heating\",", k);
    else if (a == climate::CLIMATE_ACTION_DRYING) response->printf("\"%s\":\"drying\",", k);
    else response->printf("\"%s\":\"idle\",", k);
  };
  
  auto p_mod = [&](const char* k, int m) {
    if (m < 0) response->printf("\"%s\":\"off\",", k);
    else if (m == climate::CLIMATE_MODE_HEAT) response->printf("\"%s\":\"heat\",", k);
    else if (m == climate::CLIMATE_MODE_COOL) response->printf("\"%s\":\"cool\",", k);
    else if (m == climate::CLIMATE_MODE_AUTO) response->printf("\"%s\":\"auto\",", k);
    else response->printf("\"%s\":\"off\",", k);
  };

  response->printf("\"ui_use_room_z1\":%s,", snap.ui_use_room_z1 ? "true" : "false");
  response->printf("\"ui_use_room_z2\":%s,", snap.ui_use_room_z2 ? "true" : "false");

  p_f("hp_feed_temp", snap.hp_feed_temp);
  p_f("hp_return_temp", snap.hp_return_temp);
  p_f("outside_temp", snap.outside_temp);
  p_f("compressor_frequency", snap.compressor_frequency);
  p_f("flow_rate", snap.flow_rate);
  p_f("computed_output_power", snap.computed_output_power);
  p_f("daily_computed_output_power", snap.daily_computed_output_power);
  p_f("daily_total_energy_consumption", snap.daily_total_energy_consumption);
  p_f("compressor_starts", snap.compressor_starts);
  p_f("runtime", snap.runtime);
  p_f("wifi_signal_db", snap.wifi_signal_db);

  p_f("dhw_temp", snap.dhw_temp);
  p_f("dhw_flow_temp_target", snap.dhw_flow_temp_target);
  p_f("dhw_flow_temp_drop", snap.dhw_flow_temp_drop);
  p_f("dhw_consumed", snap.dhw_consumed);
  p_f("dhw_delivered", snap.dhw_delivered);
  p_f("dhw_cop", snap.dhw_cop);

  p_f("heating_consumed", snap.heating_consumed);
  p_f("heating_produced", snap.heating_produced);
  p_f("heating_cop", snap.heating_cop);
  p_f("cooling_consumed", snap.cooling_consumed);
  p_f("cooling_produced", snap.cooling_produced);
  p_f("cooling_cop", snap.cooling_cop);

  p_f("z1_flow_temp_target", snap.z1_flow_temp_target);
  p_f("z2_flow_temp_target", snap.z2_flow_temp_target);

  p_n("auto_adaptive_setpoint_bias", snap.num_aa_setpoint_bias.val);
  p_lim("aa_bias_lim", snap.num_aa_setpoint_bias);

  p_n("maximum_heating_flow_temp", snap.num_max_flow_temp.val);
  p_lim("max_flow_lim", snap.num_max_flow_temp);
  p_n("minimum_heating_flow_temp", snap.num_min_flow_temp.val);
  p_lim("min_flow_lim", snap.num_min_flow_temp);

  p_n("maximum_heating_flow_temp_z2", snap.num_max_flow_temp_z2.val);
  p_lim("max_flow_z2_lim", snap.num_max_flow_temp_z2);
  p_n("minimum_heating_flow_temp_z2", snap.num_min_flow_temp_z2.val);
  p_lim("min_flow_z2_lim", snap.num_min_flow_temp_z2);

  p_n("thermostat_hysteresis_z1", snap.num_hysteresis_z1.val);
  p_lim("hysteresis_z1_lim", snap.num_hysteresis_z1);

  p_n("thermostat_hysteresis_z2", snap.num_hysteresis_z2.val);
  p_lim("hysteresis_z2_lim", snap.num_hysteresis_z2);

  p_n("pred_sc_time", snap.pred_sc_time.val);
  p_lim("pred_sc_time_lim", snap.pred_sc_time);
  p_n("pred_sc_delta", snap.pred_sc_delta.val);
  p_lim("pred_sc_delta_lim", snap.pred_sc_delta);

  p_n("z1_current_temp", snap.virt_z1.curr);
  p_n("z1_setpoint", snap.virt_z1.tar);
  p_act("z1_action", snap.virt_z1.action);
  p_mod("z1_mode", snap.virt_z1.mode);

  p_n("z2_current_temp", snap.virt_z2.curr);
  p_n("z2_setpoint", snap.virt_z2.tar);
  p_act("z2_action", snap.virt_z2.action);
  p_mod("z2_mode", snap.virt_z2.mode);

  p_n("room_z1_current", snap.room_z1.curr);
  p_n("room_z1_setpoint", snap.room_z1.tar);
  p_act("room_z1_action", snap.room_z1.action);

  p_n("room_z2_current", snap.room_z2.curr);
  p_n("room_z2_setpoint", snap.room_z2.tar);
  p_act("room_z2_action", snap.room_z2.action);

  p_n("flow_z1_current", snap.flow_z1.curr);
  p_n("flow_z1_setpoint", snap.flow_z1.tar);

  p_n("flow_z2_current", snap.flow_z2.curr);
  p_n("flow_z2_setpoint", snap.flow_z2.tar);

  p_b("status_compressor", snap.status_compressor);
  p_b("status_booster", snap.status_booster);
  p_b("status_defrost", snap.status_defrost);
  p_b("status_water_pump", snap.status_water_pump);
  p_b("status_in1_request", snap.status_in1_request);
  p_b("status_in6_request", snap.status_in6_request);
  p_b("zone2_enabled", snap.status_zone2_enabled);

  p_b("pred_sc_en", snap.pred_sc_switch);
  p_b("auto_adaptive_control_enabled", snap.sw_auto_adaptive);
  p_b("defrost_risk_handling_enabled", snap.sw_defrost_mit);
  p_b("smart_boost_enabled", snap.sw_smart_boost);
  p_b("force_dhw", snap.sw_force_dhw);

  // cooling settings
  p_n("cooling_smart_start_z1", snap.num_cooling_smart_start_z1.val);
  p_lim("cool_smart_z1_lim", snap.num_cooling_smart_start_z1);
  p_n("minimum_cooling_flow_z1", snap.num_min_cooling_flow_z1.val);
  p_lim("min_cool_flow_z1_lim", snap.num_min_cooling_flow_z1);

  // solver switches
  p_b("use_dynamic_cost_solver", snap.sw_use_solver);
  p_b("solver_connected", snap.bin_solver_connected);

  p_n("raw_heat_produced", snap.num_raw_heat_produced.val);
  p_lim("raw_heat_produced_lim", snap.num_raw_heat_produced);

  p_n("raw_elec_consumed", snap.num_raw_elec_consumed.val);
  p_lim("raw_elec_consumed_lim", snap.num_raw_elec_consumed);

  p_n("raw_runtime_hours", snap.num_raw_runtime_hours.val);
  p_lim("raw_runtime_hours_lim", snap.num_raw_runtime_hours);

  p_n("raw_avg_outside_temp", snap.num_raw_avg_outside_temp.val);
  p_lim("raw_avg_outside_temp_lim", snap.num_raw_avg_outside_temp);

  p_n("raw_avg_room_temp", snap.num_raw_avg_room_temp.val);
  p_lim("raw_avg_room_temp_lim", snap.num_raw_avg_room_temp);

  p_n("raw_delta_room_temp", snap.num_raw_delta_room_temp.val);
  p_lim("raw_delta_room_temp_lim", snap.num_raw_delta_room_temp);

  p_n("raw_max_output", snap.num_raw_max_output.val); 
  p_lim("raw_max_output_lim", snap.num_raw_max_output);
  p_n("raw_hl_tm_product", snap.num_raw_hl_tm_product.val);

  p_n("battery_soc_kwh", snap.num_battery_soc_kwh.val);
  p_lim("battery_soc_kwh_lim", snap.num_battery_soc_kwh);

  p_n("battery_max_discharge_kw", snap.num_battery_max_discharge_kw.val);
  p_lim("battery_max_discharge_kw_lim", snap.num_battery_max_discharge_kw);

  response->print("\"solver_ip_address\":\"");
  for (char *c = snap.txt_solver_ip; *c != '\0'; ++c) {
    if (*c == '"') response->print("\\\"");
    else if (*c == '\\') response->print("\\\\");
    else response->printf("%c", *c);
  }
  response->print("\","); 

  // Output strings safely
  response->print("\"latest_version\":\"");
  for (char *c = snap.version; *c != '\0'; ++c) {
    if (*c == '"') response->print("\\\"");
    else if (*c == '\\') response->print("\\\\");
    else response->printf("%c", *c);
  }
  response->print("\",");

  if (!std::isnan(snap.operation_mode)) response->printf("\"operation_mode\":%d,", (int)snap.operation_mode);
  else response->print("\"operation_mode\":null,");

  p_sel("heating_system_type", snap.sel_heating_system_type);
  p_sel("room_temp_source_z1", snap.sel_room_temp_source_z1);
  p_sel("room_temp_source_z2", snap.sel_room_temp_source_z2);
  p_sel("operating_mode_z1", snap.sel_operating_mode_z1);
  p_sel("operating_mode_z2", snap.sel_operating_mode_z2);
  p_sel("temp_sensor_source_z1", snap.sel_temp_source_z1);
  p_sel("temp_sensor_source_z2", snap.sel_temp_source_z2);

  response->printf("\"_uptime_ms\":%u}", millis());
  request->send(response);
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
  
  // Determine if the system is actively running AND heating (Mode 2 = HEAT_ON)
  bool is_running = bin_state_(status_compressor_) || (get_sensor(compressor_frequency_) > 0.0f);
  bool is_heating = false;
  if (operation_mode_ && operation_mode_->has_state() && !std::isnan(operation_mode_->state)) {
      is_heating = ((int)operation_mode_->state == 2); 
  }

  float current_cons = NAN;
  if (solver_kwh_meter_feedback_source_ != nullptr && solver_kwh_meter_feedback_source_->active_index().value_or(0) != 0) {
      current_cons = solver_kwh_meter_feedback_ ? solver_kwh_meter_feedback_->state : NAN;
  } else {
      current_cons = get_sensor(daily_total_energy_consumption_);
  }

  if (is_running && is_heating) {
      rec.cons = pack_temp_(current_cons);
  } else {
      // Not heating: carry over the last recorded value from the history array
      if (history_count_ > 0) {
          size_t prev_idx = (history_head_ + MAX_HISTORY - 1) % MAX_HISTORY;
          rec.cons = history_buffer_[prev_idx].cons;
      } else {
          // Edge case: literally the first minute after boot
          rec.cons = pack_temp_(current_cons);
      }
  }

  rec.flags = 0;
  if (bin_state_(status_compressor_))  rec.flags |= (1 << 0);
  if (bin_state_(status_booster_))     rec.flags |= (1 << 1);
  if (bin_state_(status_defrost_))     rec.flags |= (1 << 2);
  if (bin_state_(status_water_pump_))  rec.flags |= (1 << 3);
  if (bin_state_(status_in1_request_)) rec.flags |= (1 << 4);
  if (bin_state_(status_in6_request_)) rec.flags |= (1 << 5);

  uint8_t mode_enc = 0; // Default to OFF
  if (operation_mode_ && operation_mode_->has_state() && !std::isnan(operation_mode_->state)) {
    int val = (int)operation_mode_->state;
    if (val != 255) mode_enc = val & 0x0F; // enum value
  }
  rec.flags |= ((mode_enc & 0x0F) << 6);

  history_buffer_[history_head_] = rec;
  history_head_ = (history_head_ + 1) % MAX_HISTORY;
  if (history_count_ < MAX_HISTORY) history_count_++;
}

void EcodanDashboard::handle_history_request_(AsyncWebServerRequest *request) {
  // uint32_t free_heap = esp_get_free_heap_size();
  // uint32_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  // ESP_LOGI(TAG, "history_request_: \t\tMemory Stats | Total Free: %u bytes | Largest Block: %u bytes", free_heap, max_block);

  httpd_req_t *req = *request;
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  size_t current_count = history_count_;
  size_t current_head = history_head_;

  if (current_count == 0) {
    httpd_resp_send_chunk(req, "[]", 2);
    httpd_resp_send_chunk(req, nullptr, 0); 
    return;
  }
  
  size_t start_idx = (current_count == MAX_HISTORY) ? current_head : 0;
  size_t step = (current_count > 360) ? (current_count / 360) : 1;
  if (step < 1) step = 1;
  
  // Start the JSON array
  httpd_resp_send_chunk(req, "[", 1);
  
  bool first = true;
  for (size_t i = 0; i < current_count; i += step) {
    size_t idx = (start_idx + i) % MAX_HISTORY;
    HistoryRecord rec = history_buffer_[idx];
    
    if (!first) {
      httpd_resp_send_chunk(req, ",", 1);
    }
    first = false;
    
    char item[128];
    int len = snprintf(item, sizeof(item), "[%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d]", 
      rec.timestamp, rec.hp_feed, rec.hp_return, 
      rec.z1_sp, rec.z2_sp, rec.z1_curr, rec.z2_curr, 
      rec.z1_flow, rec.z2_flow, rec.freq, rec.flags,
      rec.cons
    );
    httpd_resp_send_chunk(req, item, len);
  }
  httpd_resp_send_chunk(req, "]", 1);
  // Send 0-length chunk to signal the end of the transmission
  httpd_resp_send_chunk(req, nullptr, 0);
}

// solver
// ---------------------------------------------------------------------------
// NVS persistence for ODIN arrays
// Namespace: "odin_cache"  Keys: "day", "sched", "energy", "prod", "exp_t",
//            "cost", "cost_tax", "batt", "act_cons", "act_room"
//            (each 24 floats = 96 bytes)
// ---------------------------------------------------------------------------
static const char* NVS_ODIN_NS = "odin_cache";

void EcodanDashboard::update_actual_data(int hour, float actual_cons_kwh, float actual_room_temp) {
    if (hour < 0 || hour >= 24) return;
    if (snapshot_mutex_ == NULL || xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) return;

    if (odin_actual_cons_.size() != 24) odin_actual_cons_.assign(24, NAN);
    if (odin_actual_room_.size() != 24) odin_actual_room_.assign(24, NAN);

    odin_actual_cons_[hour] = actual_cons_kwh;
    odin_actual_room_[hour] = actual_room_temp;

    xSemaphoreGive(snapshot_mutex_);
    this->odin_nvs_dirty_ = true;
}

void EcodanDashboard::nvs_persist_odin_() {
    std::vector<float> sched, energy, prod, exp_t, cost, cost_tax, batt, act_cons, act_room;
    int32_t day = -1;

    if (this->snapshot_mutex_ != NULL && xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(200)) == pdTRUE) {
        sched    = this->odin_schedule_;
        energy   = this->odin_energy_;
        prod     = this->odin_production_;
        exp_t    = this->odin_expected_temp_;
        cost     = this->odin_cost_;
        cost_tax = this->odin_cost_tax_;
        batt     = this->odin_battery_discharge_;
        act_cons = this->odin_actual_cons_;
        act_room = this->odin_actual_room_;
        day      = (int32_t)this->odin_stored_day_;
        xSemaphoreGive(this->snapshot_mutex_);
    } else {
        ESP_LOGW(TAG, "NVS persist: failed to acquire snapshot mutex, skipping");
        return;
    }

    nvs_handle_t h;
    if (nvs_open(NVS_ODIN_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGW(TAG, "NVS: failed to open odin_cache for write");
        return;
    }

    bool has_changes = false;

    int32_t stored_day = -1;
    if (nvs_get_i32(h, "day", &stored_day) != ESP_OK || stored_day != day) {
        nvs_set_i32(h, "day", day);
        has_changes = true;
    }

    auto save_arr = [&](const char* key, const std::vector<float>& v) {
        if (v.size() != 24) return;
        size_t required_size = 0;
        bool needs_write = true;
        if (nvs_get_blob(h, key, NULL, &required_size) == ESP_OK && required_size == 24 * sizeof(float)) {
            std::vector<float> temp(24);
            if (nvs_get_blob(h, key, temp.data(), &required_size) == ESP_OK) {
                needs_write = false;
                for (int i = 0; i < 24; i++) {
                    if (std::abs(temp[i] - v[i]) > 0.001f) { needs_write = true; break; }
                }
            }
        }
        if (needs_write) { nvs_set_blob(h, key, v.data(), 24 * sizeof(float)); has_changes = true; }
    };

    save_arr("sched",    sched);
    save_arr("energy",   energy);
    save_arr("prod",     prod);
    save_arr("exp_t",    exp_t);
    save_arr("cost",     cost);
    save_arr("cost_tax", cost_tax);
    save_arr("batt",     batt);
    save_arr("act_cons", act_cons);
    save_arr("act_room", act_room);

    if (has_changes) {
        nvs_commit(h);
        ESP_LOGI(TAG, "NVS: ODIN arrays updated and persisted (day=%d)", day);
    } else {
        ESP_LOGD(TAG, "NVS: ODIN arrays match NVS exact state, skipping physical write (day=%d)", day);
    }
    nvs_close(h);
}

// NOTE: called from setup() only, before HTTP server and other tasks start — no mutex needed.
void EcodanDashboard::nvs_load_odin_() {
    nvs_handle_t h;
    if (nvs_open(NVS_ODIN_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "NVS: no odin_cache found, starting fresh");
        return;
    }

    int32_t stored_day = -1;
    if (nvs_get_i32(h, "day", &stored_day) != ESP_OK) { nvs_close(h); return; }

    auto load_arr = [&](const char* key, std::vector<float>& out, float fill = 0.0f) -> bool {
        size_t len = 24 * sizeof(float);
        out.resize(24, fill);
        return nvs_get_blob(h, key, out.data(), &len) == ESP_OK && len == 24 * sizeof(float);
    };

    bool ok = load_arr("sched",    this->odin_schedule_)
           && load_arr("energy",   this->odin_energy_)
           && load_arr("exp_t",    this->odin_expected_temp_)
           && load_arr("cost",     this->odin_cost_)
           && load_arr("cost_tax", this->odin_cost_tax_)
           && load_arr("batt",     this->odin_battery_discharge_);

    // Optional arrays — don't fail if missing (added in later versions)
    load_arr("prod",     this->odin_production_);
    load_arr("act_cons", this->odin_actual_cons_, NAN);
    load_arr("act_room", this->odin_actual_room_, NAN);

    nvs_close(h);

    if (ok) {
        this->odin_stored_day_ = (int)stored_day;
        this->odin_data_ready_ = true;
        ESP_LOGI(TAG, "NVS: ODIN arrays restored from flash (day=%d)", stored_day);

        // If the stored day doesn't match today, the actual arrays belong to a
        // previous day — clear them now so stale data never appears in the graph.
        // (The day check in store_odin_data only handles day_delta==1 rollovers;
        //  multi-day gaps after reboot are caught here.)
        int today = 0;
        {
            time_t now_t = 0;
            time(&now_t);
            struct tm ti;
            gmtime_r(&now_t, &ti);
            today = ti.tm_yday;
        }
        if (today != 0 && today != (int)stored_day) {
            ESP_LOGI(TAG, "NVS: stored day %d != today %d — clearing actual arrays on restore",
                     (int)stored_day, today);
            this->odin_actual_cons_.assign(24, NAN);
            this->odin_actual_room_.assign(24, NAN);
        }
    } else {
        ESP_LOGW(TAG, "NVS: ODIN cache incomplete, discarding");
        this->odin_data_ready_ = false;
    }
}

void EcodanDashboard::store_odin_data(int current_hour, int current_day,
                                      const std::vector<float>& sched,
                                      const std::vector<float>& energy,
                                      const std::vector<float>& production,
                                      const std::vector<float>& exp_temp,
                                      const std::vector<float>& cost,
                                      const std::vector<float>& cost_tax,
                                      const std::vector<float>& battery_discharge,
                                      const LastRunStats& run_stats) {
    if (current_hour < 0) return;

    if (this->snapshot_mutex_ == NULL ||
        xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire snapshot mutex during ODIN store.");
        return;
    }

    // First run or size mismatch — full replace
    if (this->odin_schedule_.size() != 24) {
        this->odin_schedule_          = sched;         this->odin_schedule_.resize(24);
        this->odin_energy_            = energy;         this->odin_energy_.resize(24);
        this->odin_production_        = production;     this->odin_production_.resize(24);
        this->odin_expected_temp_     = exp_temp;       this->odin_expected_temp_.resize(24);
        this->odin_cost_              = cost;           this->odin_cost_.resize(24);
        this->odin_cost_tax_          = cost_tax;       this->odin_cost_tax_.resize(24);
        this->odin_battery_discharge_ = battery_discharge; this->odin_battery_discharge_.resize(24, 0.0f);
        // actual arrays keep their NVS-loaded values
        if (this->odin_actual_cons_.size() != 24) this->odin_actual_cons_.assign(24, NAN);
        if (this->odin_actual_room_.size() != 24) this->odin_actual_room_.assign(24, NAN);
        this->odin_data_ready_  = true;
        this->odin_stored_day_  = current_day;
    }

    // Real day transition: day advanced by exactly 1 (not a reboot with stale NVS day).
    // Only clear hours >= current_hour — hours before this are measured actuals that are still valid.
    if (current_day != this->odin_stored_day_) {
        int day_delta = current_day - this->odin_stored_day_;
        if (day_delta == 1 || day_delta == -364) {
            // Normal midnight rollover: clear future actuals, keep past hours measured today
            ESP_LOGI(TAG, "ODIN day transition (%d -> %d): clearing actual data from hour %d onward",
                     this->odin_stored_day_, current_day, current_hour);
            for (int i = current_hour; i < 24; i++) {
                if (i < (int)this->odin_actual_cons_.size()) this->odin_actual_cons_[i] = NAN;
                if (i < (int)this->odin_actual_room_.size()) this->odin_actual_room_[i] = NAN;
            }
        } else {
            // Multi-day gap (reboot with stale NVS, or missed days): just adopt new day, don't wipe actuals
            ESP_LOGI(TAG, "ODIN day gap (%d -> %d, delta=%d): adopting new day without clearing actuals",
                     this->odin_stored_day_, current_day, day_delta);
        }
        this->odin_stored_day_ = current_day;
    }

    // Partial update: overwrite current_hour onward (past hours stay from NVS).
    // NAN values in exp_temp/schedule mean "don't overwrite" — the DP engine
    // pads past hours with NAN so the original morning forecast is preserved.
    if (current_hour < 24) {
        for (int i = current_hour; i < 24; i++) {
            if (i < (int)sched.size()      && !std::isnan(sched[i]))       this->odin_schedule_[i]          = sched[i];
            if (i < (int)energy.size()     && !std::isnan(energy[i]))      this->odin_energy_[i]            = energy[i];
            if (i < (int)production.size() && !std::isnan(production[i]))  this->odin_production_[i]        = production[i];
            if (i < (int)exp_temp.size()   && !std::isnan(exp_temp[i]))    this->odin_expected_temp_[i]     = exp_temp[i];
            if (i < (int)cost.size()       && !std::isnan(cost[i]))        this->odin_cost_[i]              = cost[i];
            if (i < (int)cost_tax.size()   && !std::isnan(cost_tax[i]))    this->odin_cost_tax_[i]          = cost_tax[i];
            if (i < (int)battery_discharge.size() && !std::isnan(battery_discharge[i])) this->odin_battery_discharge_[i] = battery_discharge[i];
        }
    }

    // copy runtime stats
    this->last_run_stats_ = run_stats;

    xSemaphoreGive(this->snapshot_mutex_);
    ESP_LOGI(TAG, "ODIN arrays stored (hour=%d, day=%d)", current_hour, current_day);
    this->odin_nvs_dirty_ = true;
}

void EcodanDashboard::handle_odin_request_(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  if (response == nullptr) { request->send(500, "text/plain", "Stream alloc failed"); return; }
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Cache-Control", "no-cache");

  if (snapshot_mutex_ != NULL && xSemaphoreTake(snapshot_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
    if (this->odin_data_ready_) {
      response->print("{\"success\":true,");

      auto print_arr = [&response](const char* name, const std::vector<float>& arr, bool last = false) {
          response->printf("\"%s\":[", name);
          for (size_t i = 0; i < arr.size(); i++) {
              if (std::isnan(arr[i])) response->print("null");
              else                    response->printf("%.2f", arr[i]);
              if (i != arr.size() - 1) response->print(",");
          }
          response->print(last ? "]" : "],");
      };

      print_arr("schedule",               this->odin_schedule_);
      print_arr("energy_consumption",     this->odin_energy_);
      print_arr("heat_production",        this->odin_production_);
      print_arr("expected_begin_temp",    this->odin_expected_temp_);
      print_arr("expected_cost",          this->odin_cost_);
      print_arr("expected_cost_with_tax", this->odin_cost_tax_);
      print_arr("battery_discharge",      this->odin_battery_discharge_);
      print_arr("actual_cons",            this->odin_actual_cons_);
      print_arr("actual_room_temp",       this->odin_actual_room_, /*last=*/false);

      // Run stats
      response->printf(
          "\"last_run\":{\"execution_ms\":%u,\"heat_loss\":%.3f,\"base_cop\":%.2f,"
          "\"thermal_mass\":%.1f,\"exp_consumption\":%.2f,\"exp_production\":%.2f,"
          "\"exp_solar\":%.2f,\"total_cost\":%.4f,\"total_cost_tax\":%.4f}}",
          this->last_run_stats_.execution_ms,
          this->last_run_stats_.heat_loss,
          this->last_run_stats_.base_cop,
          this->last_run_stats_.thermal_mass,
          this->last_run_stats_.exp_consumption,
          this->last_run_stats_.exp_production,
          this->last_run_stats_.exp_solar,
          this->last_run_stats_.total_cost,
          this->last_run_stats_.total_cost_tax);
    } else {
      response->print("{\"success\":false}");
    }
    xSemaphoreGive(snapshot_mutex_);
  } else {
    response->print("{\"success\":false,\"error\":\"timeout\"}");
  }
  request->send(response);
}

}  // namespace asgard_dashboard
}  // namespace esphome
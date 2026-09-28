#include "odin.h"

#include <ctime>

#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/optimizer/optimizer.h"

namespace esphome {
namespace odin {

static const char *TAG = "odin_forwarder";

void OdinForwarder::setup() {
  if (this->ecodan_ == nullptr) {
    ESP_LOGE(TAG, "No ecodan instance configured — forwarder disabled");
    this->mark_failed();
    return;
  }
  if (mqtt::global_mqtt_client == nullptr) {
    ESP_LOGE(TAG, "No MQTT client found — forwarder disabled");
    this->mark_failed();
    return;
  }

  this->subscribe_command_topic();

  if (this->optimizer_ == nullptr) {

    ESP_LOGW(TAG, "No optimizer wired: virtual-thermostat mapping and flow limits disabled");
  }

  ESP_LOGI(TAG, "Odin forwarder: hp_id=%s telemetry=%s/%s/telemetry", this->hp_id_.c_str(), this->topic_prefix_.c_str(),
           this->hp_id_.c_str());
}

void OdinForwarder::dump_config() {
  ESP_LOGCONFIG(TAG, "Odin Forwarder:");
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  HP ID: %s", this->hp_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Topic Prefix: %s", this->topic_prefix_.c_str());
}

void OdinForwarder::update() {
  this->sync_broker_settings();
  this->sync_topic_prefix();

  bool mqtt_up = (mqtt::global_mqtt_client != nullptr) && mqtt::global_mqtt_client->is_connected();
  if (millis() - this->last_status_log_ms_ > 60000) {
    this->last_status_log_ms_ = millis();
    ESP_LOGI(TAG, "Status: mqtt_connected=%d active=%d", mqtt_up ? 1 : 0, this->active_ ? 1 : 0);
  }
  if (this->active_ != mqtt_up) {
    ESP_LOGI(TAG, "Forwarder %s (MQTT %s)", mqtt_up ? "ACTIVE" : "INACTIVE", mqtt_up ? "connected" : "disconnected");
  }
  this->active_ = mqtt_up;
  if (this->active_sensor_ != nullptr) {
    this->active_sensor_->publish_state(this->active_);
  }

  this->publish_telemetry();
  this->check_watchdog();

  if (this->takeover_sensor_ != nullptr) {
    this->takeover_sensor_->publish_state(this->taken_over_);
  }

  this->update_legionella();
}

void OdinForwarder::check_watchdog() {
  if (!this->taken_over_)
    return;
  time_t now = time(nullptr);
  if (now >= static_cast<time_t>(this->valid_until_))
    this->release();
}

}
}


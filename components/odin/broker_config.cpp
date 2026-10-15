#include "odin.h"

#include <cstdlib>

#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace odin {

static const char *TAG = "odin_forwarder";

namespace {

uint16_t parse_port(text::Text *port_text) {
  static std::string warned_raw;
  if (port_text == nullptr || !port_text->has_state() || port_text->state.empty())
    return 1883;
  const std::string &raw = port_text->state;
  char *end = nullptr;
  long value = strtol(raw.c_str(), &end, 10);
  if (end == raw.c_str() || *end != '\0' || value < 1 || value > 65535) {
    if (warned_raw != raw) {
      warned_raw = raw;
      ESP_LOGW(TAG, "Invalid MQTT port '%s' — keeping default 1883", raw.c_str());
    }
    return 1883;
  }
  return static_cast<uint16_t>(value);
}

}

void OdinForwarder::sync_broker_settings() {
  auto *client = mqtt::global_mqtt_client;
  if (client == nullptr)
    return;

  const std::string ip = (this->broker_ip_ != nullptr && this->broker_ip_->has_state()) ? this->broker_ip_->state : "";
  const uint16_t port = parse_port(this->broker_port_);
  const std::string user =
      (this->mqtt_user_ != nullptr && this->mqtt_user_->has_state()) ? this->mqtt_user_->state : "";
  const std::string password =
      (this->mqtt_password_ != nullptr && this->mqtt_password_->has_state()) ? this->mqtt_password_->state : "";


  const bool want_enabled = this->broker_ip_ == nullptr || !ip.empty();

  const bool changed =
      this->broker_settings_applied_ && (want_enabled != this->mqtt_broker_enabled_ || ip != this->last_broker_ip_ ||
                                         port != this->last_broker_port_ || user != this->last_mqtt_user_ ||
                                         password != this->last_mqtt_password_);
  if (!this->broker_settings_applied_ && !changed) {
    this->broker_settings_applied_ = true;
    if (!want_enabled)
      ESP_LOGI(TAG, "No MQTT broker address (dashboard: System → Odin MQTT) — forwarder off");
  } else if (!changed) {
    return;
  } else {
    ESP_LOGI(TAG, "Broker settings changed: %s:%u user=%s (was %s:%u user=%s)",
             ip.empty() ? "(off)" : ip.c_str(), port, user.c_str(),
             this->last_broker_ip_.empty() ? "(off)" : this->last_broker_ip_.c_str(), this->last_broker_port_,
             this->last_mqtt_user_.c_str());
  }
  this->mqtt_broker_enabled_ = want_enabled;


  client->disable();
  if (want_enabled) {
    if (!ip.empty())
      client->set_broker_address(ip);
    client->set_broker_port(port);
    client->set_username(user);
    client->set_password(password);
    client->enable();
  }
  this->last_broker_ip_ = ip;
  this->last_broker_port_ = port;
  this->last_mqtt_user_ = user;
  this->last_mqtt_password_ = password;
}

void OdinForwarder::sync_topic_prefix() {
  auto *client = mqtt::global_mqtt_client;
  if (client == nullptr)
    return;

  const std::string prefix = (this->mqtt_topic_prefix_ != nullptr && this->mqtt_topic_prefix_->has_state() &&
                              !this->mqtt_topic_prefix_->state.empty())
                                 ? this->mqtt_topic_prefix_->state
                                 : this->topic_prefix_;
  if (prefix == this->topic_prefix_)
    return;

  ESP_LOGI(TAG, "Topic prefix changed: %s -> %s", this->topic_prefix_.c_str(), prefix.c_str());
  client->unsubscribe(this->topic_prefix_ + "/" + this->hp_id_ + "/commands/setpoint");
  this->topic_prefix_ = prefix;
  this->subscribe_command_topic();
}

void OdinForwarder::subscribe_command_topic() {
  auto *client = mqtt::global_mqtt_client;
  if (client == nullptr)
    return;
  const std::string command_topic = this->topic_prefix_ + "/" + this->hp_id_ + "/commands/setpoint";
  client->subscribe(
      command_topic,
      [this](const std::string & , const std::string &payload) { this->handle_command(payload); }, 1);
  ESP_LOGI(TAG, "Subscribed to '%s'", command_topic.c_str());
}

}
}


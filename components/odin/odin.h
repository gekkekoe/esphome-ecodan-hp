#pragma once

#include <string>

#include "esphome.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include "esphome/core/component.h"

namespace esphome {
namespace optimizer {
class Optimizer;
}
}
#include "esphome/components/ecodan/ecodan.h"
#include "esphome/components/globals/globals_component.h"
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace odin {

class OdinForwarder : public PollingComponent {
public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_hp_id(const std::string &hp_id) { this->hp_id_ = hp_id; }
  void set_topic_prefix(const std::string &topic_prefix) { this->topic_prefix_ = topic_prefix; }
  void set_thermal_total(sensor::Sensor *thermal_total) { this->thermal_total_ = thermal_total; }
  void set_daily_consumption(sensor::Sensor *daily_consumption) { this->daily_consumption_ = daily_consumption; }
  void set_meter_source(select::Select *s) { this->meter_source_ = s; }
  void set_meter_feedback(number::Number *n) { this->meter_feedback_ = n; }
  void set_ecodan(ecodan::EcodanHeatpump *ecodan) { this->ecodan_ = ecodan; }
  void set_relay_z1(switch_::Switch *relay) { this->relay_z1_ = relay; }
  void set_override_z1(switch_::Switch *sw) { this->override_z1_ = sw; }
  void set_relay_z2(switch_::Switch *relay) { this->relay_z2_ = relay; }

  void set_broker_ip(text::Text *broker_ip) { this->broker_ip_ = broker_ip; }
  void set_broker_port(text::Text *broker_port) { this->broker_port_ = broker_port; }
  void set_mqtt_user(text::Text *user) { this->mqtt_user_ = user; }
  void set_mqtt_password(text::Text *password) { this->mqtt_password_ = password; }
  void set_mqtt_topic_prefix(text::Text *prefix) { this->mqtt_topic_prefix_ = prefix; }
  void set_override_z2(switch_::Switch *sw) { this->override_z2_ = sw; }
  void set_active_sensor(binary_sensor::BinarySensor *active_sensor) { this->active_sensor_ = active_sensor; }

  void set_lockout_sensor(binary_sensor::BinarySensor *lockout_sensor) { this->lockout_sensor_ = lockout_sensor; }

  void set_takeover_sensor(binary_sensor::BinarySensor *takeover_sensor) { this->takeover_sensor_ = takeover_sensor; }
  void set_optimizer(optimizer::Optimizer *opt) { this->optimizer_ = opt; }

  void set_dhw_climate(climate::Climate *c) { this->dhw_climate_ = c; }
  void set_climate(climate::Climate *climate) { this->climate_ = climate; }

  void set_climate_z2(climate::Climate *c) { this->climate_z2_ = c; }
  void handle_command(const std::string &payload);
  void set_dhw_flow_temp_target(sensor::Sensor *s) { this->dhw_flow_temp_target_ = s; }
  void set_dhw_flow_temp_drop(sensor::Sensor *s) { this->dhw_flow_temp_drop_ = s; }

  void set_regular_dhw_switch(switch_::Switch *sw) { this->regular_dhw_switch_ = sw; }

  void set_legionella_saved_setpoint(globals::RestoringGlobalsComponent<float> *g) {
    this->legionella_saved_setpoint_ = g;
  }
  void set_force_dhw_switch(switch_::Switch *sw) { this->force_dhw_switch_ = sw; }

protected:

  void sync_broker_settings();
  void sync_topic_prefix();

  void subscribe_command_topic();

  void publish_telemetry();

  void take_over();
  void release();

  void apply_soft_stop(bool z1_stop, bool z2_stop, const ecodan::Status &status);
  float apply_flow_target(const ecodan::Status &status, uint8_t mode, ecodan::Zone zone, float requested);
  void apply_legionella(const JsonDocument &doc);

  void update_legionella();
  void trigger_dhw(uint8_t mode, const std::string &dhw_mode);
  void hold_override(bool hold);

  void check_watchdog();

  std::string hp_id_{};
  std::string topic_prefix_{"hp"};
  sensor::Sensor *thermal_total_{nullptr};
  sensor::Sensor *daily_consumption_{nullptr};
  select::Select *meter_source_{nullptr};
  number::Number *meter_feedback_{nullptr};
  ecodan::EcodanHeatpump *ecodan_{nullptr};
  switch_::Switch *relay_z1_{nullptr};
  switch_::Switch *relay_z2_{nullptr};
  switch_::Switch *override_z1_{nullptr};
  switch_::Switch *override_z2_{nullptr};
  text::Text *broker_ip_{nullptr};
  text::Text *broker_port_{nullptr};
  text::Text *mqtt_user_{nullptr};
  text::Text *mqtt_password_{nullptr};
  text::Text *mqtt_topic_prefix_{nullptr};
  binary_sensor::BinarySensor *active_sensor_{nullptr};
  binary_sensor::BinarySensor *takeover_sensor_{nullptr};
  binary_sensor::BinarySensor *lockout_sensor_{nullptr};
  sensor::Sensor *dhw_flow_temp_target_{nullptr};
  sensor::Sensor *dhw_flow_temp_drop_{nullptr};
  switch_::Switch *regular_dhw_switch_{nullptr};
  globals::RestoringGlobalsComponent<float> *legionella_saved_setpoint_{nullptr};
  climate::Climate *dhw_climate_{nullptr};
  switch_::Switch *force_dhw_switch_{nullptr};

  std::string last_broker_ip_{};
  uint16_t last_broker_port_{0};
  std::string last_mqtt_user_{};
  std::string last_mqtt_password_{};

  bool broker_settings_applied_{false};
  climate::Climate *climate_{nullptr};
  climate::Climate *climate_z2_{nullptr};
  optimizer::Optimizer *optimizer_{nullptr};
  bool active_{false};
  bool taken_over_{false};
  uint32_t valid_until_{0};
  bool legionella_active_{false};

  uint32_t legionella_end_epoch_{0};

  uint32_t last_status_log_ms_{0};
};

}
}


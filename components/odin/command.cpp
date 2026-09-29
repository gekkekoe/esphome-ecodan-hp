#include "odin.h"

#include <cmath>
#include <ctime>

#include "esphome/components/optimizer/optimizer.h"

namespace esphome {
namespace odin {

static const char *TAG = "odin_forwarder";

void OdinForwarder::handle_command(const std::string &payload) {
  if (this->ecodan_ == nullptr)
    return;

  ESP_LOGI(TAG, "Command received: %s", payload.c_str());
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    ESP_LOGW(TAG, "Rejected command: bad JSON");
    return;
  }
  uint32_t valid_until = doc["valid_until"] | 0;
  if (valid_until == 0)
    return;

  uint8_t mode = doc["mode"] | 0;
  bool soft_stop = doc["soft_stop"] | false;

  float z2_production = doc["z2_production"] | NAN;
  bool z2_stop = std::isnan(z2_production) ? soft_stop : (z2_production < 0.1f);
  const auto &status = this->ecodan_->get_status();

  if (!this->taken_over_) {
    this->take_over();
    this->taken_over_ = true;
  }
  this->valid_until_ = valid_until;

  float flow_req = doc["flow_target"] | NAN;
  float flow_z2_req = doc["flow_target_z2"] | NAN;
  float flow = this->apply_flow_target(status, mode, ecodan::Zone::ZONE_1, flow_req);
  float flow_z2 = this->apply_flow_target(status, mode, ecodan::Zone::ZONE_2, flow_z2_req);
  this->apply_legionella(doc);
  this->trigger_dhw(mode, doc["dhw_mode"] | "regular");

  this->apply_soft_stop(soft_stop, z2_stop, status);

  ESP_LOGI(TAG,
           "Odin command applied: z1 flow %.1f -> %.1f, z2 flow %.1f -> %.1f, mode=%u soft_stop=%d valid_until=%lu",
           flow_req, flow, flow_z2_req, flow_z2, static_cast<unsigned>(mode), static_cast<unsigned>(soft_stop),
           static_cast<unsigned long>(valid_until));
}

void OdinForwarder::take_over() {
  ESP_LOGI(TAG, "Odin takeover: relay override held; demand via soft stop only");
  this->hold_override(true);
  // Publish now, not on the next update() poll: the AA loop gates its flow
  // writes on this sensor, and a stale "no takeover" would let one local
  // write slip through right after the first ODIN command.
  if (this->takeover_sensor_ != nullptr)
    this->takeover_sensor_->publish_state(true);
}

void OdinForwarder::release() {
  this->taken_over_ = false;
  this->valid_until_ = 0;
  this->hold_override(false);
  if (this->takeover_sensor_ != nullptr)
    this->takeover_sensor_->publish_state(false);

  ESP_LOGW(TAG, "Odin command expired — released to local control");
}

bool OdinForwarder::system_hands_off(const ecodan::Status &status) {
  if (status.DefrostActive)
    return true;
  if (this->lockout_sensor_ != nullptr && this->lockout_sensor_->state)
    return true;
  return status.Operation == ecodan::Status::OperationMode::DHW_ON ||
         status.Operation == ecodan::Status::OperationMode::LEGIONELLA_PREVENTION;
}

void OdinForwarder::apply_soft_stop(bool z1_stop, bool z2_stop, const ecodan::Status &status) {

  if (this->relay_z1_ == nullptr && this->relay_z2_ == nullptr) {
    return;
  }

  if (this->system_hands_off(status))
    return;

  auto apply_zone = [](switch_::Switch *relay, bool stop, const char *zl) {
    if (relay == nullptr || relay->state == !stop)
      return;
    if (stop) {
      ESP_LOGI(TAG, "Odin soft stop: %s demand disabled (relay off)", zl);
      relay->turn_off();
    } else {
      ESP_LOGI(TAG, "Odin soft start: %s demand enabled (relay on)", zl);
      relay->turn_on();
    }
  };
  apply_zone(this->relay_z1_, z1_stop, "z1");
  if (status.has_2zones())
    apply_zone(this->relay_z2_, z2_stop, "z2");
}

float OdinForwarder::apply_flow_target(const ecodan::Status &status, uint8_t mode, ecodan::Zone zone, float requested) {
  if (std::isnan(requested))
    return requested;
  if (zone == ecodan::Zone::ZONE_2 && !status.has_2zones())
    return requested;

  if (status.DefrostActive || (this->lockout_sensor_ != nullptr && this->lockout_sensor_->state))
    return requested;

  if (!status.is_auto_adaptive_heating(zone) && !status.is_auto_adaptive_cooling(zone))
    return requested;

  if (mode == 2) {
    if (!status.is_heating_active(zone)) {
      ESP_LOGD(TAG, "Z%d flow target %.1f not applied — heating not active (mode 2).", (int)zone + 1, requested);
      return requested;
    }
  } else if (mode == 3) {
    if (!status.is_cooling_active(zone)) {
      ESP_LOGD(TAG, "Z%d flow target %.1f not applied — cooling not active (mode 3).", (int)zone + 1, requested);
      return requested;
    }
  }
  if (this->optimizer_ != nullptr) {
    auto oz = zone == ecodan::Zone::ZONE_1 ? optimizer::OptimizerZone::ZONE_1 : optimizer::OptimizerZone::ZONE_2;
    requested = this->optimizer_->limit_external_flow(oz, requested);
  }
  float current =
      (zone == ecodan::Zone::ZONE_1) ? status.Zone1FlowTemperatureSetPoint : status.Zone2FlowTemperatureSetPoint;
  if (std::isnan(current) || std::fabsf(current - requested) > 0.05f)
    this->ecodan_->set_flow_target_temperature(requested, zone);
  return requested;
}

void OdinForwarder::apply_legionella(const JsonDocument &doc) {
  if (!(doc["legionella"] | false)) {

    if (this->legionella_active_) {
      ESP_LOGI(TAG, "Legionella: cycle ended per ODIN command - restoring setpoint");
      this->legionella_end_epoch_ = 0;
      this->update_legionella();
    }
    return;
  }

  float leg_setpoint = doc["legionella_setpoint"] | 60.0f;
  int leg_duration_min = doc["legionella_estimated_duration_min"] | (doc["legionella_duration_min"] | 15);

  if (!this->legionella_active_) {

    if (this->dhw_flow_temp_target_ && this->legionella_saved_setpoint_ &&
        this->legionella_saved_setpoint_->value() <= 0.0f) {
      float current = this->dhw_flow_temp_target_->state;
      if (current > 0.0f && current < leg_setpoint - 0.05f)
        this->legionella_saved_setpoint_->value() = current;
    }
    this->legionella_active_ = true;
    this->legionella_end_epoch_ = static_cast<uint32_t>(time(nullptr)) + static_cast<uint32_t>(leg_duration_min) * 60;
    ESP_LOGI(TAG, "Legionella: setpoint=%.1fC duration=%d min (restore %.1fC, ends at epoch %lu)", leg_setpoint,
             leg_duration_min, this->legionella_saved_setpoint_ ? this->legionella_saved_setpoint_->value() : -1.0f,
             static_cast<unsigned long>(this->legionella_end_epoch_));
  }

  if (this->dhw_climate_) {
    auto call = this->dhw_climate_->make_call();
    call.set_target_temperature(leg_setpoint);
    call.perform();
  }
}

void OdinForwarder::update_legionella() {

  if (!this->legionella_active_ || static_cast<uint32_t>(time(nullptr)) < this->legionella_end_epoch_)
    return;

  this->legionella_active_ = false;
  float restore = -1.0f;
  if (this->legionella_saved_setpoint_)
    restore = this->legionella_saved_setpoint_->value();
  if (restore > 0.0f && this->dhw_climate_) {
    auto call = this->dhw_climate_->make_call();
    call.set_target_temperature(restore);
    call.perform();
  }
  if (this->legionella_saved_setpoint_)
    this->legionella_saved_setpoint_->value() = -1.0f;
  ESP_LOGI(TAG, "Legionella: cycle complete, restored setpoint to %.1fC", restore);
}

void OdinForwarder::trigger_dhw(uint8_t mode, const std::string &dhw_mode) {
  if (mode != 1)
    return;

  if (dhw_mode == "forced") {
    if (this->force_dhw_switch_ != nullptr && !this->force_dhw_switch_->state) {
      this->force_dhw_switch_->turn_on();
      ESP_LOGI(TAG, "Odin command: DHW cycle forced (force switch on)");
    }
  } else if (this->regular_dhw_switch_ != nullptr && !this->regular_dhw_switch_->state) {
    this->regular_dhw_switch_->turn_on();
    ESP_LOGI(TAG, "Odin command: DHW cycle triggered (regular switch on)");
  }
}

void OdinForwarder::hold_override(bool hold) {
  auto apply = [](switch_::Switch *sw, bool state) {
    if (sw != nullptr && sw->state != state) {
      if (state)
        sw->turn_on();
      else
        sw->turn_off();
    }
  };
  apply(this->override_z1_, hold);
  apply(this->override_z2_, hold);
}

}
}


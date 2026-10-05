#include "odin.h"

#include <cmath>

#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/optimizer/optimizer.h"

namespace esphome {
namespace odin {

void OdinForwarder::publish_telemetry() {
  if (this->ecodan_ == nullptr || mqtt::global_mqtt_client == nullptr)
    return;

  const auto &status = this->ecodan_->get_status();
  if (!status.Initialized)
    return;

  JsonDocument doc;

  auto put_float = [&doc](const char *key, float value) {
    if (std::isnan(value))
      doc[key] = nullptr;
    else
      doc[key] = value;
  };

  struct ZoneTemps {
    const char *room_key, *setpoint_key, *feed_key, *return_key;
    optimizer::OptimizerZone zone;
    float ecodan::Status::*room;
    float ecodan::Status::*setpoint;
    float ecodan::Status::*feed;
    float ecodan::Status::*ret;
  };
  auto *opt = this->optimizer_;
  auto emit_zone = [&](const ZoneTemps &z) {
    put_float(z.room_key, opt ? opt->get_room_current_temp(z.zone) : status.*(z.room));
    put_float(z.setpoint_key, opt ? opt->get_room_target_temp(z.zone) : status.*(z.setpoint));
    put_float(z.feed_key, opt ? opt->get_feed_temp(z.zone) : status.*(z.feed));
    put_float(z.return_key, opt ? opt->get_return_temp(z.zone) : status.*(z.ret));
  };

  put_float("outside_temp", status.OutsideTemperature);
  emit_zone({"room_temp", "zone1_setpoint", "flow_temp", "return_temp", optimizer::OptimizerZone::ZONE_1,
             &ecodan::Status::Zone1RoomTemperature, &ecodan::Status::Zone1SetTemperature,
             &ecodan::Status::HpFeedTemperature, &ecodan::Status::HpReturnTemperature});
  put_float("dhw_temp", status.get_tank_temperature());
  put_float("dhw_temp_bottom", status.get_lower_tank_temperature());

  // THW10 / mixing-tank temperature. 
  put_float("mixing_tank_temp",
            status.has_mixing_tank() ? status.MixingTankTemperature : NAN);

  // DHW setpoint and max drop: pump properties. The start threshold is not
  // published - Odin has that as its own setting now.
  put_float("dhw_target", this->dhw_flow_temp_target_ ? this->dhw_flow_temp_target_->state : NAN);
  put_float("dhw_drop", this->dhw_flow_temp_drop_ ? this->dhw_flow_temp_drop_->state : NAN);

  if (status.Operation != ecodan::Status::OperationMode::UNAVAILABLE)
    doc["operation_mode"] = static_cast<uint8_t>(status.Operation);

  auto put_selected_mode = [&doc](const char *key, climate::Climate *cl) {
    if (cl == nullptr)
      return;
    int8_t value = 0;
    if (cl->mode == climate::CLIMATE_MODE_HEAT)
      value = 1;
    else if (cl->mode == climate::CLIMATE_MODE_COOL)
      value = 2;
    doc[key] = value;
  };
  put_selected_mode("selected_mode", this->climate_);
  put_selected_mode("selected_mode_z2", this->climate_z2_);

  if (status.Operation == ecodan::Status::OperationMode::UNAVAILABLE)
    doc["compressor_on"] = 2;
  else
    doc["compressor_on"] = status.CompressorOn ? 1 : 0;
  doc["defrost"] = status.DefrostActive;
  doc["booster"] = status.BoosterActive;

  doc["buffer_discharging"] = status.WaterPump2Active ? 1 : 0;
  // Zone 2 only discharges when the second zone exists and its own pump
  // P3 runs; anything else reports idle
  doc["buffer_discharging_z2"] =
      (status.has_2zones() && status.WaterPump3Active) ? 1 : 0;
  doc["multi_zone_status"] = static_cast<uint8_t>(status.MultiZoneStatus);

  if (status.has_2zones()) {
    doc["zone2_enabled"] = true;
    emit_zone({"room_temp_z2", "zone2_setpoint", "flow_temp_z2", "return_temp_z2", optimizer::OptimizerZone::ZONE_2,
               &ecodan::Status::Zone2RoomTemperature, &ecodan::Status::Zone2SetTemperature,
               &ecodan::Status::Z2FeedTemperature, &ecodan::Status::Z2ReturnTemperature});
  }

  // Flow-temperature limits for ODIN-side clamping: a directly-connected pump
  // (HeishaMon) has no apply_flow_target equivalent, so ODIN must clamp the
  // flow temp it sends to the limits configured here. get_*_flow_limits already
  // applies the entity min<=max guard.
  if (opt != nullptr) {
    auto hl1 = opt->get_flow_limits(optimizer::OptimizerZone::ZONE_1);
    put_float("heating_min_flow_temp_z1", hl1.min);
    put_float("heating_max_flow_temp_z1", hl1.max);
    put_float("cooling_min_flow_temp_z1", opt->get_cool_flow_limits(optimizer::OptimizerZone::ZONE_1).min);
    if (status.has_2zones()) {
      auto hl2 = opt->get_flow_limits(optimizer::OptimizerZone::ZONE_2);
      put_float("heating_min_flow_temp_z2", hl2.min);
      put_float("heating_max_flow_temp_z2", hl2.max);
      put_float("cooling_min_flow_temp_z2", opt->get_cool_flow_limits(optimizer::OptimizerZone::ZONE_2).min);
    }
  }

  float electric_kwh_total = NAN;
  if (this->meter_source_ != nullptr && this->meter_source_->active_index().value_or(0) != 0) {
    if (this->meter_feedback_ != nullptr && this->meter_feedback_->has_state())
      electric_kwh_total = this->meter_feedback_->state;
  } else if (this->daily_consumption_ != nullptr && this->daily_consumption_->has_state()) {
    electric_kwh_total = this->daily_consumption_->state;
  }
  put_float("electric_kwh_total", electric_kwh_total);

  put_float("compressor_frequency", status.CompressorFrequency);

  put_float("flow_rate", status.FlowRate);

  put_float("thermal_w", status.ComputedOutputPower);

  put_float("runtime", status.Runtime);

  put_float("compressor_starts", status.RcCompressorStarts);
  if (this->thermal_total_ != nullptr && this->thermal_total_->has_state())
    put_float("thermal_kwh_total", this->thermal_total_->state);

  std::string out;
  serializeJson(doc, out);

  const auto topic = this->topic_prefix_ + "/" + this->hp_id_ + "/telemetry";
  mqtt::global_mqtt_client->publish(topic, out.c_str(), out.size(), 1, false);
}

}
}


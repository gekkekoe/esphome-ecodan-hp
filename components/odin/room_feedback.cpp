// The room temperature ODIN hands back to this device (docs/MQTT_DESIGN.md §4 on the
// ODIN side: <prefix>/<hp_id>/room_feedback/<zone>, retained).
#include "odin.h"

#include <cmath>
#include <cstdlib>
#include <ctime>

#include "esphome/components/optimizer/optimizer.h"

namespace esphome {
namespace odin {

static const char *TAG = "odin_forwarder";

namespace {

// <prefix>/<hp_id>/room_feedback/<zone>, the zone a 0-based index (the same numbering
// the room_temp topics use: 0 is the first zone). Anything else is not for us.
int zone_from_topic(const std::string &topic) {
  const auto slash = topic.rfind('/');
  if (slash == std::string::npos || slash + 1 >= topic.size())
    return -1;
  char *end = nullptr;
  const long zone = std::strtol(topic.c_str() + slash + 1, &end, 10);
  if (end == nullptr || *end != '\0')
    return -1;
  return (zone == 0 || zone == 1) ? static_cast<int>(zone) : -1;
}

}  // namespace

void OdinForwarder::handle_room_feedback(const std::string &topic, const std::string &payload) {
  const int zone = zone_from_topic(topic);
  if (zone < 0) {
    ESP_LOGW(TAG, "Room feedback on a topic without a usable zone: '%s'", topic.c_str());
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>()) {
    ESP_LOGW(TAG, "Room feedback z%u: not a JSON object: %s", zone + 1, payload.c_str());
    return;
  }

  if (doc["value"].isNull()) {
    if (!std::isnan(this->room_feed_[zone]))
      ESP_LOGI(TAG, "Room feedback z%u: ODIN has nothing to inherit, using our own room sensor", zone + 1);
    this->room_feed_[zone] = NAN;
    this->room_feed_ms_[zone] = 0;
    return;
  }

  const float value = doc["value"] | NAN;
  const uint32_t ts = doc["ts"] | 0u;
  if (std::isnan(value) || ts == 0) {
    ESP_LOGW(TAG, "Room feedback z%u: refused, value or fix time missing: %s", zone + 1, payload.c_str());
    return;
  }

  const char *src = doc["src"] | "?";
  const bool was_inheriting = !std::isnan(this->room_feed_[zone]);
  const uint32_t since_last = (millis() - this->room_feed_ms_[zone]) / 1000U;
  this->room_feed_[zone] = value;
  this->room_feed_ms_[zone] = millis();
  if (was_inheriting)
    ESP_LOGD(TAG, "Room feedback z%u: ODIN restates %.1f °C (src %s, %.1fs after its previous message)",
             zone + 1, value, src, static_cast<double>(since_last));
  else
    ESP_LOGI(TAG, "Room feedback z%u: inheriting %.1f °C from ODIN (src %s)", zone + 1, value, src);
}

bool OdinForwarder::room_feed(int zone, float *value, float *age_s) const {
  if (zone < 0 || zone > 1 || std::isnan(this->room_feed_[zone]))
    return false;
  if (value != nullptr)
    *value = this->room_feed_[zone];
  if (age_s != nullptr)
    // Seconds since ODIN last restated this zone. Unsigned subtraction, so it
    // survives a millis() rollover.
    *age_s = static_cast<float>(static_cast<int32_t>(millis() - this->room_feed_ms_[zone])) / 1000.0f;
  return true;
}

float OdinForwarder::inherited_room_temp(int zone) {
  if (this->optimizer_ == nullptr || zone < 0 || zone > 1)
    return NAN;
  return this->optimizer_->room_temp_from_odin(zone == 1 ? optimizer::OptimizerZone::ZONE_2
                                                         : optimizer::OptimizerZone::ZONE_1);
}

}  // namespace odin
}  // namespace esphome

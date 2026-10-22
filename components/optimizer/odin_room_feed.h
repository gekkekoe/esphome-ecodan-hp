#pragma once

namespace esphome {
namespace optimizer {

    struct OdinRoomFeed
    {
        virtual ~OdinRoomFeed() = default;
        // false when this zone (0-based) has nothing to inherit. age_s is the number of
        // seconds since ODIN last restated it, on this device's monotonic clock.
        virtual bool room_feed(int zone, float *value, float *age_s) const = 0;
    };

} // namespace optimizer
} // namespace esphome

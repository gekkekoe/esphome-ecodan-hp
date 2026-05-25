#pragma once
#include <cstddef>
#include <cstdint>

namespace esphome {
namespace asgard_dashboard {

// ── File paths ────────────────────────────────────────────────────────────────
inline constexpr const char* LFS_MINUTES_PATH = "/lfs/history_mins.bin";
inline constexpr const char* LFS_HOURLY_PATH  = "/lfs/history_hour.bin";
inline constexpr const char* LFS_ODIN_PATH    = "/lfs/odin_data.bin";

// ── Magic numbers / versioning ────────────────────────────────────────────────
inline constexpr uint32_t HISTORY_MAGIC   = 0xDA741001UL;
inline constexpr uint16_t HISTORY_VERSION = 1;
inline constexpr uint32_t ODIN_MAGIC      = 0x0D100001UL;

// ── Buffer / capacity constants ───────────────────────────────────────────────
inline constexpr size_t MAX_MINUTES     = 10080; // 7 days  × 24h × 60min
inline constexpr size_t MAX_HOURLY      = 26280; // 3 years × 365d × 24h
inline constexpr size_t LFS_FLUSH_COUNT = 5;    // flush every 5 minute-records
inline constexpr size_t LFS_DATA_OFFSET = 64;    // file header is exactly 64 bytes

// ── Minute record (64 bytes) ──────────────────────────────────────────────────
struct MinuteRecord {
    uint32_t timestamp;
    int16_t  hp_feed;
    int16_t  hp_return;
    int16_t  z1_sp;
    int16_t  z2_sp;
    int16_t  z1_curr;
    int16_t  z2_curr;
    int16_t  z1_flow;
    int16_t  z2_flow;
    int16_t  freq;
    uint16_t flags;        // bits 0-5: booleans, bits 6-9: operation mode
    int16_t  cons;
    int16_t  prod;
    int16_t  outside;
    int16_t  liquid_pipe;
    int16_t  condensing;
    int16_t  reserved[15]; // 30 bytes reserved for future expansion
};
static_assert(sizeof(MinuteRecord) == 64, "MinuteRecord must be 64 bytes");

// ── Hourly record (64 bytes) ──────────────────────────────────────────────────
struct HourlyRecord {
    uint32_t timestamp;
    int16_t  avg_outside;
    int16_t  total_cons;
    int16_t  total_prod;
    int16_t  odin_heat_loss;
    int16_t  odin_cop;
    int16_t  odin_cost;
    int16_t  odin_solar;

    int16_t  exp_cons;
    int16_t  exp_prod;
    int16_t  exp_room_temp;
    int16_t  actual_room_temp;
    int16_t  actual_dhw_cons;
    int16_t  actual_dhw_prod;
    int16_t  actual_standby_cons;
    int16_t  price;
    int16_t  weather;
    int16_t  batt_discharge;
    int16_t  op_mode;
    int16_t  sched_base;
    int16_t  sched_min;
    int16_t  sched_max;

    int16_t  exp_solar_kwh; // expected solar yield this hour kWh × 100
    int16_t  reserved[8];   // 16 bytes reserved for future expansion
};
static_assert(sizeof(HourlyRecord) == 64, "HourlyRecord must be 64 bytes");

// ── Circular-buffer file header (64 bytes) ────────────────────────────────────
// Shared by both the minute and hourly history files. The record_size and
// max_records fields identify which file this header belongs to at runtime.
struct CircularFileHeader {
    uint32_t magic;        // HISTORY_MAGIC
    uint16_t version;      // HISTORY_VERSION
    uint16_t record_size;  // sizeof(MinuteRecord) or sizeof(HourlyRecord)
    uint32_t max_records;  // MAX_MINUTES or MAX_HOURLY
    uint32_t head;         // write-head index (circular buffer)
    uint32_t count;        // number of valid records currently stored
    uint8_t  reserved[44]; // pad to exactly 64 bytes
};
static_assert(sizeof(CircularFileHeader) == 64, "CircularFileHeader must be 64 bytes");

// ── ODIN 72-hour forecast cache (~9.2 KB) ─────────────────────────────────────
// 32 slots × 72 floats; slots 0-18 are active, 19-31 are reserved (stored as NaN).
#pragma pack(push, 1)
struct OdinCacheStruct {
    uint32_t magic;
    int32_t  stored_day;
    uint8_t  show_tab;
    uint8_t  padding[3];
    float    arrays[32][72];
};
#pragma pack(pop)

} // namespace asgard_dashboard
} // namespace esphome

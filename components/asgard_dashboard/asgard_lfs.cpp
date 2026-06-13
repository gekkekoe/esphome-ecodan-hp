#include "asgard_lfs.h"
#include "asgard_dashboard.h"
#include "esphome/core/log.h"
#include "freertos/task.h"
#include <esp_littlefs.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace esphome {
namespace asgard_dashboard {

static const char* const TAG_LFS = "asgard_lfs";

// ── Low-level helpers ─────────────────────────────────────────────────────────

namespace lfs_helper {

bool write_file(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = (fwrite(data, 1, size, f) == size);
    fclose(f);
    return ok;
}

bool read_file(const char* path, void* data, size_t size) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    bool ok = (fread(data, 1, size, f) == size);
    fclose(f);
    return ok;
}

bool append_file(const char* path, const void* data, size_t size) {
    FILE* f = fopen(path, "ab");
    if (!f) return false;
    bool ok = (fwrite(data, 1, size, f) == size);
    fclose(f);
    return ok;
}

bool update_header(const char* path, const void* header, size_t header_size) {
    FILE* f = fopen(path, "r+b");
    if (!f) return false;
    bool ok = (fwrite(header, header_size, 1, f) == 1);
    fclose(f);
    return ok;
}

bool write_circular(const char* path, size_t offset, size_t max_records,
                    size_t record_size, size_t write_pos,
                    const void* data, size_t count) {
    if (count == 0) return true;

    FILE* f = fopen(path, "r+b");
    if (!f) return false;

    const size_t space_to_end = max_records - write_pos;
    fseek(f, offset + write_pos * record_size, SEEK_SET);

    bool ok;
    if (count <= space_to_end) {
        ok = (fwrite(data, record_size, count, f) == count);
    } else {
        ok = (fwrite(data, record_size, space_to_end, f) == space_to_end);
        if (ok) {
            fseek(f, offset, SEEK_SET);
            ok = (fwrite(static_cast<const uint8_t*>(data) + space_to_end * record_size,
                         record_size, count - space_to_end, f) == count - space_to_end);
        }
    }
    fclose(f);
    return ok;
}

bool init_circular_file(const char* path, uint16_t record_size, uint32_t max_records,
                        size_t& head_out, size_t& count_out) {
    // Attempt to restore from an existing valid file
    CircularFileHeader hdr{};
    if (read_file(path, &hdr, sizeof(hdr)) &&
        hdr.magic       == HISTORY_MAGIC   &&
        hdr.version     == HISTORY_VERSION &&
        hdr.record_size == record_size     &&
        hdr.max_records == max_records) {
        head_out  = hdr.head % max_records;
        count_out = std::min((size_t)hdr.count, (size_t)max_records);
        ESP_LOGI(TAG_LFS, "LFS: %s restored (head=%zu, count=%zu)",
                 path, head_out, count_out);
        return true;
    }

    ESP_LOGW(TAG_LFS, "LFS: %s missing or stale — (re)creating", path);

    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG_LFS, "LFS: failed to create %s", path);
        head_out = count_out = 0;
        return false;
    }

    hdr = {};
    hdr.magic       = HISTORY_MAGIC;
    hdr.version     = HISTORY_VERSION;
    hdr.record_size = record_size;
    hdr.max_records = max_records;
    fwrite(&hdr, sizeof(hdr), 1, f);

    // Zero-fill all record slots in batches to reduce fwrite call count.
    // Yield to watchdog roughly every 1000 records.
    constexpr size_t FILL_BATCH = 64;
    std::vector<uint8_t> empty(record_size * FILL_BATCH, 0);
    uint32_t remaining = max_records;
    uint32_t written   = 0;
    uint32_t next_yield = 1000; // Setup yield threshold

    while (remaining > 0) {
        uint32_t chunk = std::min((uint32_t)FILL_BATCH, remaining);
        fwrite(empty.data(), record_size, chunk, f);
        remaining -= chunk;
        written   += chunk;
        
        if (written >= next_yield) {
            vTaskDelay(pdMS_TO_TICKS(10));
            next_yield += 1000;
        }
    }
    fclose(f);

    head_out = count_out = 0;
    return false; // new file, not restored
}

} // namespace lfs_helper

// ── LittleFS mount + file initialisation ─────────────────────────────────────
void EcodanDashboard::setup_lfs() {
    // Check if LFS partition exists
    const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "history");
    
    if (part == nullptr) {
        ESP_LOGE(TAG_LFS, "Partition 'history' NOT FOUND in partition table!");
        ESP_LOGE(TAG_LFS, "LFS is disabled. Please flash the device via USB to update the partition table.");
        return;
    }

    esp_vfs_littlefs_conf_t conf = {
        .base_path             = "/lfs",
        .partition_label       = "history",
        .format_if_mount_failed = true,
        .dont_mount            = false,
    };
    
    if (esp_vfs_littlefs_register(&conf) != ESP_OK) {
        ESP_LOGE(TAG_LFS, "LittleFS mount failed — history disabled");
        return;  // lfs_mounted_ stays false
    }
    this->lfs_mounted_ = true;

    lfs_helper::init_circular_file(LFS_MINUTES_PATH, sizeof(MinuteRecord),
                                   MAX_MINUTES, history_head_, history_count_);

    lfs_helper::init_circular_file(LFS_HOURLY_PATH, sizeof(HourlyRecord),
                                   MAX_HOURLY, hourly_head_, hourly_count_);

    // ODIN cache: create the file if it does not exist yet
    if (FILE* f = fopen(LFS_ODIN_PATH, "rb")) {
        fclose(f);
    } else {
        ESP_LOGI(TAG_LFS, "LFS: creating ODIN cache file");
        auto cache = std::make_unique<OdinCacheStruct>();
        cache->magic      = ODIN_MAGIC;
        cache->stored_day = -1;
        cache->show_tab   = 0;
        for (int k = 0; k < 32; k++)
            for (int i = 0; i < 72; i++)
                cache->arrays[k][i] = NAN;
        lfs_helper::write_file(LFS_ODIN_PATH, cache.get(), sizeof(OdinCacheStruct));
    }
}

// ── History flush task (minute records) ──────────────────────────────────────

void EcodanDashboard::lfs_task_(void* arg) {
    auto* self = static_cast<EcodanDashboard*>(arg);

    // Allocated once on the heap — keeps it off the task stack for every iteration.
    auto local_snap = std::make_unique<MinuteRecord[]>(LFS_FLUSH_COUNT);

    while (true) {
        if (xSemaphoreTake(self->lfs_trigger_, portMAX_DELAY) != pdTRUE) continue;

        // Snapshot the flush parameters under history_mutex_ to avoid a race
        // with record_history_() on the main task: it updates these shared fields
        // every LFS_FLUSH_COUNT minutes, potentially before we finish the write.
        size_t pos   = 0;
        size_t count = 0;
        if (self->history_mutex_ != NULL &&
            xSemaphoreTake(self->history_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            pos   = self->lfs_flush_write_pos_;
            count = self->lfs_flush_snap_count_;
            if (count > 0)
                memcpy(local_snap.get(), self->lfs_flush_snap_, count * sizeof(MinuteRecord));
            xSemaphoreGive(self->history_mutex_);
        } else {
            ESP_LOGW(TAG_LFS, "lfs_task_: failed to acquire history_mutex_, skipping flush");
            continue;
        }

        FILE* f = fopen(LFS_MINUTES_PATH, "r+b");
        if (!f) continue;

        const size_t space_to_end  = MAX_MINUTES - pos;

        if (count > 0) {
            fseek(f, LFS_DATA_OFFSET + pos * sizeof(MinuteRecord), SEEK_SET);
            if (count <= space_to_end) {
                fwrite(local_snap.get(), sizeof(MinuteRecord), count, f);
            } else {
                fwrite(local_snap.get(), sizeof(MinuteRecord), space_to_end, f);
                fseek(f, LFS_DATA_OFFSET, SEEK_SET);
                fwrite(local_snap.get() + space_to_end,
                       sizeof(MinuteRecord), count - space_to_end, f);
            }
        }

        // Persist updated head / count in the file header.
        // Read the current head/count under the mutex so we don't race with
        // record_history_() advancing them while we write.
        size_t cur_head = 0, cur_count = 0;
        if (self->history_mutex_ != NULL &&
            xSemaphoreTake(self->history_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
            cur_head  = self->history_head_;
            cur_count = self->history_count_;
            xSemaphoreGive(self->history_mutex_);
        }
        CircularFileHeader hdr{};
        fseek(f, 0, SEEK_SET);
        if (fread(&hdr, sizeof(hdr), 1, f) == 1) {
            hdr.head  = cur_head;
            hdr.count = cur_count;
            fseek(f, 0, SEEK_SET);
            fwrite(&hdr, sizeof(hdr), 1, f);
        }
        fclose(f);
    }
}

// ── Hourly record persistence ─────────────────────────────────────────────────

void EcodanDashboard::record_hourly_data(const HourlyRecord& rec) {
    if (!lfs_mounted_ || history_mutex_ == NULL || xSemaphoreTake(history_mutex_, portMAX_DELAY) != pdTRUE)
        return;

    // 1. Write the record — write_circular does a forward seek only.
    lfs_helper::write_circular(LFS_HOURLY_PATH, LFS_DATA_OFFSET,
                                MAX_HOURLY, sizeof(HourlyRecord),
                                hourly_head_, &rec, 1);

    hourly_head_ = (hourly_head_ + 1) % MAX_HOURLY;
    if (hourly_count_ < MAX_HOURLY) hourly_count_++;

    // 2. Update the header — built from in-memory values so no read_file needed.
    //    update_header opens a fresh handle at position 0 and writes immediately,
    //    avoiding any backwards seek on LittleFS.
    {
        CircularFileHeader hdr{};
        hdr.magic       = HISTORY_MAGIC;
        hdr.version     = HISTORY_VERSION;
        hdr.record_size = sizeof(HourlyRecord);
        hdr.max_records = MAX_HOURLY;
        hdr.head        = hourly_head_;
        hdr.count       = hourly_count_;
        lfs_helper::update_header(LFS_HOURLY_PATH, &hdr, sizeof(hdr));
    }

    xSemaphoreGive(history_mutex_);
}

// ── ODIN forecast persistence ─────────────────────────────────────────────────

// LFS persist/load and the HTTP API response all derive from this table.
// Slot order (0-18) defines the on-disk layout — do NOT reorder existing entries.
std::array<EcodanDashboard::OdinArrayEntry, 19>
EcodanDashboard::odin_array_map_() {
    return {{
        {0,  "expected_end_temp",   &odin_expected_end_temp_    },
        {1,  "energy_consumption",  &odin_energy_               },
        {2,  "heat_production",     &odin_production_           },
        {3,  "expected_begin_temp", &odin_expected_temp_        },
        {4,  "expected_cost",       &odin_cost_                 },
        {5,  "battery_discharge",   &odin_battery_discharge_    },
        {6,  "actual_dhw_cons",     &odin_actual_dhw_cons_      },
        {7,  "actual_dhw_prod",     &odin_actual_dhw_prod_      },
        {8,  "actual_cons",         &odin_actual_cons_          },
        {9,  "actual_prod",         &odin_actual_prod_          },
        {10, "actual_room_temp",    &odin_actual_room_          },
        {11, "prices",              &odin_prices_               },
        {12, "weather",             &odin_weather_              },
        {13, "solar",               &odin_solar_                },
        {14, "operation_mode",      &odin_operation_mode_       },
        {15, "sched_base",          &odin_sched_base_           },
        {16, "sched_min",           &odin_sched_min_            },
        {17, "sched_max",           &odin_sched_max_            },
        {18, "actual_standby_cons", &odin_actual_standby_cons_  },
    }};
}

void EcodanDashboard::ensure_odin_vectors_() {
    for (const auto& entry : odin_array_map_()) {
        float fill_val = NAN;
        switch (entry.slot) {
            case 5:  // battery_discharge
            case 11: // prices
            case 12: // weather
            case 13: // solar
            case 14: // operation_mode
            case 15: // sched_base
            case 16: // sched_min
            case 17: // sched_max
                fill_val = 0.0f;
                break;
            default:
                break;
        }

        if (entry.vec->size() != 72) entry.vec->assign(72, fill_val);
    }
}

void EcodanDashboard::lfs_odin_task_(void* arg) {
    auto* self = static_cast<EcodanDashboard*>(arg);
    while (true) {
        if (xSemaphoreTake(self->lfs_odin_trigger_, portMAX_DELAY) == pdTRUE)
            self->lfs_persist_odin_();
    }
}

void EcodanDashboard::lfs_persist_odin_() {
    if (!this->lfs_mounted_) {
        return;
    }
    
    // make_unique value-initialises all fields to zero, so no explicit memset needed.
    auto cache = std::make_unique<OdinCacheStruct>();
    cache->magic = ODIN_MAGIC;

    if (this->snapshot_mutex_ != NULL && xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(200)) == pdTRUE) {
        cache->stored_day = this->odin_stored_day_;
        cache->show_tab = this->lfs_show_tab_cache_.load() ? 1 : 0;

        auto safe_save = [&](int idx, const std::vector<float>& v) {
            if (v.size() == 72) {
                memcpy(cache->arrays[idx], v.data(), 72 * sizeof(float));
            } else {
                for (int i = 0; i < 72; i++) cache->arrays[idx][i] = NAN;
            }
        };

        for (const auto& entry : odin_array_map_())
            safe_save(entry.slot, *entry.vec);

        for (int k = 19; k < 32; k++) {
            for (int i = 0; i < 72; i++) cache->arrays[k][i] = NAN;
        }

        xSemaphoreGive(this->snapshot_mutex_);
    } else {
        ESP_LOGW(TAG_LFS, "LFS persist: failed to acquire snapshot mutex");
        return;
    }

    if (lfs_helper::write_file(LFS_ODIN_PATH, cache.get(), sizeof(OdinCacheStruct))) {
        ESP_LOGI(TAG_LFS, "LFS: Odin 72h forecast saved to disk.");
    }
}

void EcodanDashboard::load_odin_data(int current_day, int current_hour) {
    if (!lfs_mounted_ || this->odin_data_ready_) return;

    auto cache = std::unique_ptr<OdinCacheStruct>(new OdinCacheStruct());
    bool ok = lfs_helper::read_file(LFS_ODIN_PATH, cache.get(), sizeof(OdinCacheStruct));
    if (ok && cache->magic != ODIN_MAGIC) ok = false;

    if (!ok) {
        ESP_LOGI(TAG_LFS, "LFS: no valid odin forecast cache found, starting fresh");
        if (this->snapshot_mutex_ != NULL && xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
            this->ensure_odin_vectors_();
            this->odin_stored_day_ = current_day;
            this->odin_data_ready_ = true;
            xSemaphoreGive(this->snapshot_mutex_);
        }
        return;
    }

    int32_t aligned_day = cache->stored_day;

    // Time availability check: only shift if current_day is valid (-1 means no NTP yet)
    if (current_day != -1 && current_day != (int)cache->stored_day) {
        int day_delta = current_day - (int)cache->stored_day;
        if (day_delta == 1 || day_delta == -364 || day_delta == -365) {
            ESP_LOGI(TAG_LFS, "LFS Load: Day transition (%d -> %d), shifting 72h window", cache->stored_day, current_day);
            for (int k = 0; k < 32; k++) {
                for (int i = 0; i < 48; i++) cache->arrays[k][i] = cache->arrays[k][i + 24];
                for (int i = 48; i < 72; i++) cache->arrays[k][i] = NAN; 
            }
            for (int i = 48; i < 72; i++) {
                cache->arrays[5][i]  = 0.0f; // batt
                cache->arrays[11][i] = 0.0f; // prices
                cache->arrays[12][i] = 0.0f; // weather
                cache->arrays[13][i] = 0.0f; // solar
                cache->arrays[14][i] = 0.0f; // op_mode
                cache->arrays[15][i] = 0.0f; // sb
                cache->arrays[16][i] = 0.0f; // smn
                cache->arrays[17][i] = 0.0f; // smx
            }
            // After the shift, slots 24..(24+current_hour-1) (new "today", elapsed hours)
            // still hold yesterday's tomorrow-forecast op_mode. Clear only those slots —
            // slots from current_hour onwards were set by the last solver run and are valid.
            for (int i = 24; i < 24 + current_hour; i++) cache->arrays[14][i] = 0.0f;
        } else {
            ESP_LOGI(TAG_LFS, "LFS Load: Day jump, clearing stale actuals");
            for (int k : {6, 7, 8, 9, 10, 18}) {
                for (int i = 0; i < 72; i++) cache->arrays[k][i] = NAN;
            }
        }
        aligned_day = current_day;
    } else if (current_day == -1) {
        aligned_day = cache->stored_day;
    }

    if (this->snapshot_mutex_ != NULL && xSemaphoreTake(this->snapshot_mutex_, pdMS_TO_TICKS(500)) == pdTRUE) {
        
        // Exact manual extraction using memcpy to protect memory pointers
        auto safe_load = [](int idx, const float src_arr[32][72], std::vector<float>& dest_vec) {
            if (dest_vec.size() != 72) dest_vec.resize(72);
            memcpy(dest_vec.data(), src_arr[idx], 72 * sizeof(float));
        };

        for (const auto& entry : odin_array_map_())
            safe_load(entry.slot, cache->arrays, *entry.vec);

        this->odin_stored_day_ = (int)aligned_day;
        this->odin_data_ready_ = true;
        xSemaphoreGive(this->snapshot_mutex_);
        
        ESP_LOGI(TAG_LFS, "LFS: ODIN forecast cache loaded successfully.");
    }

    if (this->sw_show_solver_tab_ != nullptr) {
        if (cache->show_tab) this->sw_show_solver_tab_->turn_on();
        else                 this->sw_show_solver_tab_->turn_off();
    }
}

} // namespace asgard_dashboard
} // namespace esphome

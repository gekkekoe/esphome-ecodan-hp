#include "asgard_lfs.h"
#include "asgard_dashboard.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
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

// Re-sync head/count with the data area; see the declaration in asgard_lfs.h.
void repair_head(const char* path, size_t max_records,
                 size_t& head, size_t& count) {
    FILE* f = fopen(path, "rb");
    if (!f) return;

    auto ts_at = [&](size_t idx) -> uint32_t {
        uint32_t ts = 0;
        if (fseek(f, (long)(LFS_DATA_OFFSET + idx * sizeof(MinuteRecord)), SEEK_SET) != 0) return 0;
        if (fread(&ts, sizeof(ts), 1, f) != 1) return 0;
        return ts;
    };

    // What the bookmark believes is newest, and the newest record inside the
    // window the bookmark can have missed.
    const uint32_t newest_per_header = ts_at((head + max_records - 1) % max_records);
    uint32_t best_ts = 0;
    size_t   best    = 0;
    bool     found   = false;
    for (size_t j = 0; j < LFS_HEAD_REPAIR_SCAN; j++) {
        const size_t idx = (head + j) % max_records;
        const uint32_t ts = ts_at(idx);
        if (ts > best_ts) { best_ts = ts; best = idx; found = true; }
    }
    fclose(f);

    if (!found || best_ts <= newest_per_header) return;
    const size_t advance = (best + max_records - head) % max_records + 1;
    if (advance >= LFS_HEAD_REPAIR_SCAN) return;

    head = (best + 1) % max_records;
    if (count < max_records) {
        count += advance;
        if (count > max_records) count = max_records;
    }
    ESP_LOGI(TAG_LFS, "LFS: %s bookmark was %zu record(s) stale, head corrected", path, advance);
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

    // The head/count bookmark is written at most once an hour (see the flush
    // task), so records flushed after it are newer than it knows. Find the real
    // head before anything reads or writes: resuming at the bookmark would
    // overwrite the newest records of the last hour instead of the oldest.
    lfs_helper::repair_head(LFS_MINUTES_PATH, MAX_MINUTES, history_head_, history_count_);

    lfs_helper::init_circular_file(LFS_HOURLY_PATH, sizeof(HourlyRecord),
                                   MAX_HOURLY, hourly_head_, hourly_count_);

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

        // Persist the head/count bookmark, but at most once per interval
        const uint32_t now_ms = millis();
        if (now_ms - self->last_header_persist_ms_ < LFS_HEADER_PERSIST_MS) {
            fclose(f);
            continue;
        }
        size_t cur_head = 0, cur_count = 0;
        if (self->history_mutex_ == NULL ||
            xSemaphoreTake(self->history_mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
            // Never persist a zeroed bookmark, and take the write again next
            // flush rather than store a head that was never read.
            fclose(f);
            continue;
        }
        cur_head  = self->history_head_;
        cur_count = self->history_count_;
        xSemaphoreGive(self->history_mutex_);
        CircularFileHeader hdr{};
        fseek(f, 0, SEEK_SET);
        if (fread(&hdr, sizeof(hdr), 1, f) == 1) {
            hdr.head  = cur_head;
            hdr.count = cur_count;
            fseek(f, 0, SEEK_SET);
            if (fwrite(&hdr, sizeof(hdr), 1, f) == 1) self->last_header_persist_ms_ = now_ms;
        }
        fclose(f);
    }
}


} // namespace asgard_dashboard
} // namespace esphome

#pragma once
#include "asgard_lfs_types.h"

namespace esphome {
namespace asgard_dashboard {
namespace lfs_helper {

// ── Low-level file helpers ────────────────────────────────────────────────────

bool write_file(const char* path, const void* data, size_t size);
bool read_file (const char* path, void*       data, size_t size);
bool append_file(const char* path, const void* data, size_t size);

// Overwrite the header portion (first header_size bytes) of an existing file.
bool update_header(const char* path, const void* header, size_t header_size);

// Write `count` records of `record_size` bytes into a circular file at the
// given `write_pos`, wrapping around at `max_records`. Data area starts at
// `offset` bytes from the beginning of the file.
bool write_circular(const char* path, size_t offset, size_t max_records,
                    size_t record_size, size_t write_pos,
                    const void* data, size_t count);

// Validate and restore an existing circular-buffer file, or create a fresh one.
// Returns true when an existing valid file was restored, false when a new file
// was created. head_out and count_out are always set on success.
bool init_circular_file(const char* path, uint16_t record_size, uint32_t max_records,
                        size_t& head_out, size_t& count_out);

} // namespace lfs_helper
} // namespace asgard_dashboard
} // namespace esphome

#pragma once
#include "env_driver.h"
#include "soil.h"
#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Logger Module — LittleFS-backed CSV session logger with file rotation
//
// Responsibilities:
//   - Mount LittleFS and persist log files across reboots
//   - Maintain up to MAX_FILES (5) CSV files; when full, the slot that would
//     be written next (round-robin) is deleted first (oldest-first rotation)
//   - Write one header + N data rows per session
//   - Store customer-facing names for completed log sessions
//
// Usage contract:
//   1. Call logger::begin() once in setup(), after M5.begin().
//   2. Call logger::start_new_session(name) when logging starts.
//   3. Call logger::append_row() for every sensor snapshot to persist.
//   4. Call logger::stop_session() when logging ends.
//   5. Use logger::list_files() to expose completed session metadata.
//   6. Call logger::clear_all() to wipe every log file (e.g. /clear endpoint).
// ---------------------------------------------------------------------------

namespace logger {

    static constexpr uint8_t MAX_LOG_FILES = 5;
    static constexpr size_t  LOG_PATH_LEN  = 16;
    static constexpr size_t  LOG_NAME_LEN  = 33; // 32 chars + null terminator

    struct LogFileInfo {
        uint8_t slot;
        char    path[LOG_PATH_LEN];
        char    name[LOG_NAME_LEN];
        size_t  size_bytes;
        bool    is_latest;
    };

    struct StorageInfo {
        size_t total_bytes;
        size_t used_bytes;
        size_t free_bytes;
        size_t log_bytes;
    };

    // Mount LittleFS and restore the write-slot counter from flash.
    // Returns false if the filesystem cannot be mounted.
    bool begin();

    // Open a new CSV file for writing; applies rotation when MAX_FILES exist.
    // Returns false if the file cannot be created.
    bool start_new_session(const char* name);

    // Append one data row to the open session file and flush immediately.
    // No-op (with a warning log) if no session is open.
    void append_row(uint32_t timestamp_ms,
                    const env::EnvReading& env,
                    uint16_t lux,
                    const soil::SoilReading& soil);

    // Close the current session file.
    // After this call is_session_open() returns false and
    // get_latest_filename() returns the path of the file just closed.
    void stop_session();

    // Returns true while a session file is open for writing.
    bool is_session_open();

    // Returns a pointer to a static buffer holding the path of the most
    // recently completed session file, or nullptr if none is available
    // (no session has finished yet, or the file was cleared).
    const char* get_latest_filename();

    // Returns a pointer to a static buffer holding the path of a completed
    // log slot, or nullptr if the slot is invalid, missing, or currently open.
    const char* get_filename_for_slot(uint8_t slot);

    // Returns true when a completed log already uses this display name.
    // Comparison is case-insensitive.
    bool name_exists(const char* name);

    // Fill out with metadata for completed log files. Returns the count
    // written, ordered newest-first when the latest slot is known.
    uint8_t list_files(LogFileInfo* out, uint8_t max_items);

    // Fill storage totals for the LittleFS partition and CSV log footprint.
    void get_storage_info(StorageInfo& out);

    // Delete all /log_*.csv files and reset the slot counter.
    // No-op (with a warning) if a session is currently open.
    void clear_all();

} // namespace logger

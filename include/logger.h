#pragma once
#include "env_driver.h"
#include "soil.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Logger Module — LittleFS-backed CSV session logger with file rotation
//
// Responsibilities:
//   - Mount LittleFS and persist log files across reboots
//   - Maintain up to MAX_FILES (5) CSV files; when full, the slot that would
//     be written next (round-robin) is deleted first (oldest-first rotation)
//   - Write one header + N data rows per session
//   - Expose the most-recently completed file path for download
//
// Usage contract:
//   1. Call logger::begin() once in setup(), after M5.begin().
//   2. Call logger::start_new_session() when logging starts.
//   3. Call logger::append_row() for every sensor snapshot to persist.
//   4. Call logger::stop_session() when logging ends.
//   5. Check logger::is_session_open() before allowing downloads.
//   6. Use logger::get_latest_filename() to stream the completed file.
//   7. Call logger::clear_all() to wipe every log file (e.g. /clear endpoint).
// ---------------------------------------------------------------------------

namespace logger {

    // Mount LittleFS and restore the write-slot counter from flash.
    // Returns false if the filesystem cannot be mounted.
    bool begin();

    // Open a new CSV file for writing; applies rotation when MAX_FILES exist.
    // Returns false if the file cannot be created.
    bool start_new_session();

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

    // Delete all /log_*.csv files and reset the slot counter.
    // No-op (with a warning) if a session is currently open.
    void clear_all();

} // namespace logger

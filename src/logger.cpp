#include "logger.h"

#include <LittleFS.h>
#include <esp_log.h>
#include <stdio.h>

static const char* TAG = "logger";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint8_t MAX_FILES        = 5;
static constexpr size_t  ROW_BUF_SIZE     = 128;
static constexpr uint8_t FLUSH_EVERY_ROWS = 30;
static constexpr char    INDEX_FILE[]     = "/log_index";

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static File    _file;
static bool    _session_open        = false;
static int8_t  _current_slot        = -1;
static int8_t  _last_completed_slot = -1;
static uint8_t _next_slot           = 0;
static uint8_t _unflushed_rows      = 0;  // rows written since last flush

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void slot_path(char* buf, size_t len, uint8_t slot)
{
    snprintf(buf, len, "/log_%u.csv", slot);
}

static void save_next_slot()
{
    File f = LittleFS.open(INDEX_FILE, "w");
    if (!f) {
        ESP_LOGW(TAG, "Could not save next slot index");
        return;
    }
    f.write(_next_slot);
    f.close();
}

static void load_next_slot()
{
    if (!LittleFS.exists(INDEX_FILE)) {
        _next_slot = 0;
        return;
    }
    File f = LittleFS.open(INDEX_FILE, "r");
    if (!f) {
        _next_slot = 0;
        return;
    }
    int v = f.read();
    f.close();
    _next_slot = (v >= 0 && v < MAX_FILES) ? static_cast<uint8_t>(v) : 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace logger {

bool begin()
{
    if (!LittleFS.begin(false)) {
        ESP_LOGE(TAG, "LittleFS mount failed; filesystem was not auto-formatted to avoid erasing existing logs");
        return false;
    }
    load_next_slot();
    ESP_LOGI(TAG, "LittleFS ready — next slot: %u", _next_slot);
    return true;
}

bool start_new_session()
{
    if (_session_open) {
        ESP_LOGW(TAG, "Session already open — ignoring start_new_session");
        return false;
    }

    char path[16];
    slot_path(path, sizeof(path), _next_slot);

    // In round-robin rotation the slot we are about to write holds the
    // oldest data (or does not exist yet); remove it before overwriting.
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        ESP_LOGI(TAG, "Rotated out %s", path);
    }

    _file = LittleFS.open(path, "w");
    if (!_file) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return false;
    }

    _file.print("timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct\r\n");
    _file.flush();

    _current_slot    = static_cast<int8_t>(_next_slot);
    _session_open    = true;
    _unflushed_rows  = 0;

    _next_slot = (_next_slot + 1) % MAX_FILES;
    save_next_slot();

    ESP_LOGI(TAG, "Logging session started -> %s", path);
    return true;
}

void append_row(uint32_t               timestamp_ms,
                const env::EnvReading& env,
                uint16_t               lux,
                const soil::SoilReading& soil)
{
    if (!_session_open || !_file) {
        ESP_LOGW(TAG, "append_row called with no open session");
        return;
    }

    char row[ROW_BUF_SIZE];
    snprintf(row, sizeof(row),
             "%lu,%.1f,%.1f,%.1f,%u,%u,%.0f\r\n",
             static_cast<unsigned long>(timestamp_ms),
             env.temp_c,
             env.rh_pct,
             env.pressure_pa / 100.0f,
             lux,
             soil.raw,
             soil.pct);

    _file.print(row);
    ++_unflushed_rows;
    if (_unflushed_rows >= FLUSH_EVERY_ROWS) {
        _file.flush();
        _unflushed_rows = 0;
        ESP_LOGD(TAG, "Flushed %u rows to flash", FLUSH_EVERY_ROWS);
    }
}

void stop_session()
{
    if (!_session_open) {
        return;
    }
    // Flush any rows that haven't been written to flash yet.
    if (_unflushed_rows > 0) {
        _file.flush();
        _unflushed_rows = 0;
    }
    _file.close();
    _last_completed_slot = _current_slot;
    _session_open        = false;
    ESP_LOGI(TAG, "Logging session closed (slot %d)", _current_slot);
}

bool is_session_open()
{
    return _session_open;
}

const char* get_latest_filename()
{
    if (_session_open || _last_completed_slot < 0) {
        return nullptr;
    }
    // NOTE: single-threaded Arduino loop; no concurrent access is possible.
    static char buf[16];
    slot_path(buf, sizeof(buf), static_cast<uint8_t>(_last_completed_slot));
    return LittleFS.exists(buf) ? buf : nullptr;
}

void clear_all()
{
    if (_session_open) {
        ESP_LOGW(TAG, "clear_all called while session is open — ignored");
        return;
    }
    for (uint8_t i = 0; i < MAX_FILES; i++) {
        char path[16];
        slot_path(path, sizeof(path), i);
        if (LittleFS.exists(path)) {
            LittleFS.remove(path);
        }
    }
    if (LittleFS.exists(INDEX_FILE)) {
        LittleFS.remove(INDEX_FILE);
    }
    _next_slot           = 0;
    _last_completed_slot = -1;
    ESP_LOGI(TAG, "All log files cleared");
}

} // namespace logger

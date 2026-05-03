#include "logger.h"

#include <LittleFS.h>
#include <esp_log.h>
#include <ctype.h>
#include <stdio.h>

static const char* TAG = "logger";

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
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

static void meta_path(char* buf, size_t len, uint8_t slot)
{
    snprintf(buf, len, "/log_%u.meta", slot);
}

static bool slot_is_available(uint8_t slot)
{
    if (slot >= logger::MAX_LOG_FILES) {
        return false;
    }
    if (_session_open && _current_slot == static_cast<int8_t>(slot)) {
        return false;
    }
    char path[logger::LOG_PATH_LEN];
    slot_path(path, sizeof(path), slot);
    return LittleFS.exists(path);
}

static bool names_equal_ci(const char* a, const char* b)
{
    if (a == nullptr || b == nullptr) {
        return false;
    }
    while (*a != '\0' && *b != '\0') {
        char ca = static_cast<char>(tolower(static_cast<unsigned char>(*a)));
        char cb = static_cast<char>(tolower(static_cast<unsigned char>(*b)));
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool read_log_name(uint8_t slot, char* out, size_t len)
{
    if (out == nullptr || len == 0) {
        return false;
    }
    out[0] = '\0';

    char path[logger::LOG_PATH_LEN];
    meta_path(path, sizeof(path), slot);
    if (!LittleFS.exists(path)) {
        return false;
    }

    File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        if (f) {
            f.close();
        }
        return false;
    }

    size_t n = f.readBytes(out, len - 1);
    f.close();
    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\r' || out[n - 1] == '\n')) {
        out[--n] = '\0';
    }
    return n > 0;
}

static void display_name_for_slot(uint8_t slot, char* out, size_t len)
{
    if (out == nullptr || len == 0) {
        return;
    }
    if (!read_log_name(slot, out, len)) {
        snprintf(out, len, "Legacy log %u", static_cast<unsigned int>(slot + 1));
    }
}

static bool write_log_name(uint8_t slot, const char* name)
{
    char path[logger::LOG_PATH_LEN];
    meta_path(path, sizeof(path), slot);

    File f = LittleFS.open(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for metadata", path);
        return false;
    }
    f.print(name);
    f.close();
    return true;
}

static void remove_slot_files(uint8_t slot)
{
    char path[logger::LOG_PATH_LEN];

    slot_path(path, sizeof(path), slot);
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        ESP_LOGI(TAG, "Removed %s", path);
    }

    meta_path(path, sizeof(path), slot);
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
    }
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
    _next_slot = (v >= 0 && v < logger::MAX_LOG_FILES) ? static_cast<uint8_t>(v) : 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace logger {

bool begin()
{
    if (!LittleFS.begin(false)) {
        // First boot (or genuinely corrupt FS): format once and retry.
        // On healthy subsequent boots the first begin() succeeds, so data
        // is never erased by a transient mount failure.
        ESP_LOGW(TAG, "LittleFS mount failed — formatting once and retrying (first boot or corrupt FS)");
        if (!LittleFS.begin(true)) {
            ESP_LOGE(TAG, "LittleFS format + mount failed");
            return false;
        }
        ESP_LOGI(TAG, "LittleFS formatted and mounted");
    }
    load_next_slot();

    // Infer the last completed slot from the persisted next-slot pointer.
    // The slot written most recently is (_next_slot - 1) in round-robin order.
    // If that file exists, make it visible in the completed log list after boot.
    uint8_t candidate = (_next_slot + logger::MAX_LOG_FILES - 1) % logger::MAX_LOG_FILES;
    char path[LOG_PATH_LEN];
    slot_path(path, sizeof(path), candidate);
    if (LittleFS.exists(path)) {
        _last_completed_slot = static_cast<int8_t>(candidate);
        ESP_LOGI(TAG, "Inferred last completed slot: %d (%s)", _last_completed_slot, path);
    }

    ESP_LOGI(TAG, "LittleFS ready — next slot: %u", _next_slot);
    return true;
}

bool start_new_session(const char* name)
{
    if (_session_open) {
        ESP_LOGW(TAG, "Session already open — ignoring start_new_session");
        return false;
    }
    if (name == nullptr || name[0] == '\0') {
        ESP_LOGW(TAG, "Session name is required");
        return false;
    }

    char path[LOG_PATH_LEN];
    slot_path(path, sizeof(path), _next_slot);

    // In round-robin rotation the slot we are about to write holds the
    // oldest data (or does not exist yet); remove it before overwriting.
    remove_slot_files(_next_slot);

    _file = LittleFS.open(path, "w");
    if (!_file) {
        ESP_LOGE(TAG, "Failed to open %s for writing", path);
        return false;
    }

    if (!write_log_name(_next_slot, name)) {
        _file.close();
        LittleFS.remove(path);
        return false;
    }

    _file.print("timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct\r\n");
    _file.flush();

    _current_slot    = static_cast<int8_t>(_next_slot);
    _session_open    = true;
    _unflushed_rows  = 0;

    _next_slot = (_next_slot + 1) % MAX_LOG_FILES;
    save_next_slot();

    ESP_LOGI(TAG, "Logging session started -> %s (%s)", path, name);
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
    static char buf[LOG_PATH_LEN];
    slot_path(buf, sizeof(buf), static_cast<uint8_t>(_last_completed_slot));
    return LittleFS.exists(buf) ? buf : nullptr;
}

const char* get_filename_for_slot(uint8_t slot)
{
    if (!slot_is_available(slot)) {
        return nullptr;
    }
    static char buf[LOG_PATH_LEN];
    slot_path(buf, sizeof(buf), slot);
    return buf;
}

bool name_exists(const char* name)
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }

    char existing[LOG_NAME_LEN];
    for (uint8_t slot = 0; slot < MAX_LOG_FILES; ++slot) {
        if (!slot_is_available(slot)) {
            continue;
        }
        display_name_for_slot(slot, existing, sizeof(existing));
        if (names_equal_ci(existing, name)) {
            return true;
        }
    }
    return false;
}

uint8_t list_files(LogFileInfo* out, uint8_t max_items)
{
    if (out == nullptr || max_items == 0) {
        return 0;
    }

    uint8_t count = 0;
    for (uint8_t offset = 0; offset < MAX_LOG_FILES && count < max_items; ++offset) {
        uint8_t slot = offset;
        if (_last_completed_slot >= 0) {
            slot = static_cast<uint8_t>(
                (_last_completed_slot + MAX_LOG_FILES - offset) % MAX_LOG_FILES);
        }
        if (!slot_is_available(slot)) {
            continue;
        }

        slot_path(out[count].path, sizeof(out[count].path), slot);
        File f = LittleFS.open(out[count].path, "r");
        if (!f || f.isDirectory()) {
            if (f) {
                f.close();
            }
            continue;
        }
        out[count].slot       = slot;
        out[count].size_bytes = f.size();
        out[count].is_latest  = (_last_completed_slot == static_cast<int8_t>(slot));
        display_name_for_slot(slot, out[count].name, sizeof(out[count].name));
        f.close();
        ++count;
    }

    return count;
}

void get_storage_info(StorageInfo& out)
{
    out.total_bytes = LittleFS.totalBytes();
    out.used_bytes  = LittleFS.usedBytes();
    out.free_bytes  = out.total_bytes > out.used_bytes ? out.total_bytes - out.used_bytes : 0;
    out.log_bytes   = 0;

    for (uint8_t slot = 0; slot < MAX_LOG_FILES; ++slot) {
        char path[LOG_PATH_LEN];
        slot_path(path, sizeof(path), slot);
        if (!LittleFS.exists(path)) {
            continue;
        }

        File f = LittleFS.open(path, "r");
        if (!f || f.isDirectory()) {
            if (f) {
                f.close();
            }
            continue;
        }
        out.log_bytes += f.size();
        f.close();
    }
}

void clear_all()
{
    if (_session_open) {
        ESP_LOGW(TAG, "clear_all called while session is open — ignored");
        return;
    }
    for (uint8_t i = 0; i < MAX_LOG_FILES; i++) {
        remove_slot_files(i);
    }
    if (LittleFS.exists(INDEX_FILE)) {
        LittleFS.remove(INDEX_FILE);
    }
    _next_slot           = 0;
    _last_completed_slot = -1;
    ESP_LOGI(TAG, "All log files cleared");
}

} // namespace logger

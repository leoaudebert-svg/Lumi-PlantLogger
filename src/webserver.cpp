#include "plant_webserver.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <atomic>
#include <ctype.h>
#include "logger.h"
#include "dashboard_page.h"

static const char* TAG = "webserver";

// ---------------------------------------------------------------------------
// Network constants
// ---------------------------------------------------------------------------
static constexpr char    AP_SSID[]            = "PlantLogger";
static constexpr uint8_t DNS_PORT             = 53;
static constexpr size_t  JSON_STATUS_BUF_SIZE = 384;

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static WebServer _server(80);
static DNSServer _dns;

static env::EnvReading   _last_env        = {};
static uint16_t          _last_lux        = 0;
static soil::SoilReading _last_soil       = {};
static bool              _logging         = false;
static uint32_t          _log_interval_ms = 60000;
static char              _log_name[logger::LOG_NAME_LEN] = "";
static std::atomic<bool> _read_req {false};

// Pending calibration points, initialised from NVS in begin().
// Both are needed before set_calibration() is called.
static uint16_t _pending_dry_raw = 0;
static uint16_t _pending_wet_raw = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool parse_slot_arg(uint8_t& slot_out) {
    String value = _server.arg("slot");
    if (value.length() == 0 || value.length() > 2) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        if (!isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    long slot = value.toInt();
    if (slot < 0 || slot >= logger::MAX_LOG_FILES) {
        return false;
    }
    slot_out = static_cast<uint8_t>(slot);
    return true;
}

static const char* basename_for_path(const char* path) {
    if (path == nullptr) {
        return "";
    }
    return path[0] == '/' ? path + 1 : path;
}

static void append_json_string(String& body, const char* value) {
    body += '"';
    if (value != nullptr) {
        for (const char* p = value; *p != '\0'; ++p) {
            char c = *p;
            if (c == '"' || c == '\\') {
                body += '\\';
                body += c;
            } else if (c == '\n') {
                body += "\\n";
            } else if (c == '\r') {
                body += "\\r";
            } else if (static_cast<unsigned char>(c) >= 0x20) {
                body += c;
            }
        }
    }
    body += '"';
}

static bool is_valid_log_name_char(char c) {
    return isalnum(static_cast<unsigned char>(c)) ||
           c == ' ' ||
           c == '_' ||
           c == '-' ||
           c == '.';
}

static bool validate_log_name(String& name, String& error) {
    name.trim();
    if (name.length() == 0) {
        error = "Log name is required";
        return false;
    }
    if (name.length() >= logger::LOG_NAME_LEN) {
        error = "Log name must be 32 characters or less";
        return false;
    }
    for (size_t i = 0; i < name.length(); ++i) {
        if (!is_valid_log_name_char(name[i])) {
            error = "Log name can use letters, numbers, spaces, _, -, and .";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// HTTP handlers (file-scope; registered in begin())
// ---------------------------------------------------------------------------

static void handle_root() {
    _server.send_P(200, "text/html", HTML_PAGE);
}

static void handle_status() {
    char buf[JSON_STATUS_BUF_SIZE];
    snprintf(buf, JSON_STATUS_BUF_SIZE,
        "{\"temp_c\":%.1f,\"rh_pct\":%.1f,\"pressure_hpa\":%.1f,"
        "\"lux\":%u,\"soil_raw\":%u,\"soil_pct\":%.0f,"
        "\"soil_calibrated\":%s,\"logging\":%s,"
        "\"cal_dry\":%u,\"cal_wet\":%u}",
        _last_env.temp_c,
        _last_env.rh_pct,
        _last_env.pressure_pa / 100.0f,
        _last_lux,
        _last_soil.raw,
        _last_soil.pct,
        _last_soil.calibrated ? "true" : "false",
        _logging ? "true" : "false",
        _pending_dry_raw,
        _pending_wet_raw);
    _server.send(200, "application/json", buf);
}

static void handle_logs() {
    logger::LogFileInfo files[logger::MAX_LOG_FILES];
    uint8_t count = logger::list_files(files, logger::MAX_LOG_FILES);
    logger::StorageInfo storage;
    logger::get_storage_info(storage);

    String body;
    body.reserve(160 + static_cast<size_t>(count) * 160);
    body = "{\"logs\":[";
    for (uint8_t i = 0; i < count; ++i) {
        if (i > 0) {
            body += ',';
        }
        body += "{\"slot\":";
        body += String(files[i].slot);
        body += ",\"name\":";
        append_json_string(body, files[i].name);
        body += ",\"filename\":";
        append_json_string(body, basename_for_path(files[i].path));
        body += ",\"path\":";
        append_json_string(body, files[i].path);
        body += ",\"size_bytes\":";
        body += String(static_cast<unsigned long>(files[i].size_bytes));
        body += ",\"is_latest\":";
        body += files[i].is_latest ? "true" : "false";
        body += '}';
    }
    body += "],\"storage\":{\"total_bytes\":";
    body += String(static_cast<unsigned long>(storage.total_bytes));
    body += ",\"used_bytes\":";
    body += String(static_cast<unsigned long>(storage.used_bytes));
    body += ",\"free_bytes\":";
    body += String(static_cast<unsigned long>(storage.free_bytes));
    body += ",\"log_bytes\":";
    body += String(static_cast<unsigned long>(storage.log_bytes));
    body += "}}";
    _server.send(200, "application/json", body);
}

static void handle_read_now() {
    _read_req = true;
    _server.send(200, "text/plain", "OK");
}

static void handle_start() {
    if (_logging) {
        _server.send(409, "text/plain", "Already logging - stop the current session first");
        return;
    }

    String name = _server.arg("name");
    String error;
    if (!validate_log_name(name, error)) {
        _server.send(400, "text/plain", error);
        return;
    }
    if (logger::name_exists(name.c_str())) {
        _server.send(409, "text/plain", "A log with that name already exists");
        return;
    }

    String interval_str = _server.arg("interval");
    if (interval_str.length() > 0) {
        long interval_secs = interval_str.toInt();
        if (interval_secs > 0) {
            _log_interval_ms = static_cast<uint32_t>(interval_secs) * 1000UL;
        }
    }
    snprintf(_log_name, sizeof(_log_name), "%s", name.c_str());
    _logging = true;
    ESP_LOGI(TAG, "Logging started via web (name: %s, interval: %u ms)", _log_name, _log_interval_ms);
    _server.send(200, "text/plain", "OK");
}

static void handle_stop() {
    _logging = false;
    ESP_LOGI(TAG, "Logging stopped via web");
    _server.send(200, "text/plain", "OK");
}

static void handle_graphs() {
    _server.send_P(200, "text/html", HTML_PAGE);
}

static void handle_log_data() {
    uint8_t slot = 0;
    if (!parse_slot_arg(slot)) {
        _server.send(400, "text/plain", "Invalid log slot");
        return;
    }

    const char* path = logger::get_filename_for_slot(slot);
    if (path == nullptr) {
        _server.send(404, "text/plain", "No log file available");
        return;
    }

    File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        _server.send(500, "text/plain", "Failed to open log file");
        return;
    }

    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(f, "text/csv");
    f.close();
}

static void handle_clear() {
    if (_logging) {
        _server.send(409, "text/plain", "Stop logging before clearing");
        return;
    }
    logger::clear_all();
    ESP_LOGI(TAG, "/clear: all log files removed");
    _server.send(200, "text/plain", "OK");
}

static void handle_calibrate_dry() {
    if (!soil::is_ok()) {
        _server.send(503, "text/plain", "Soil sensor not initialised");
        return;
    }
    uint16_t raw = soil::read_median();
    _pending_dry_raw = raw;
    ESP_LOGI(TAG, "Dry calibration point captured: raw=%u", raw);
    if (_pending_wet_raw > 0 && raw > _pending_wet_raw) {
        soil::set_calibration(raw, _pending_wet_raw);
        char msg[64];
        snprintf(msg, sizeof(msg), "Dry point saved (raw=%u). Calibration complete!", raw);
        _server.send(200, "text/plain", msg);
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "Dry point recorded (raw=%u). Now submerge in water and click Capture Wet.", raw);
        _server.send(200, "text/plain", msg);
    }
}

static void handle_calibrate_wet() {
    if (!soil::is_ok()) {
        _server.send(503, "text/plain", "Soil sensor not initialised");
        return;
    }
    uint16_t raw = soil::read_median();
    _pending_wet_raw = raw;
    ESP_LOGI(TAG, "Wet calibration point captured: raw=%u", raw);
    if (_pending_dry_raw > 0 && _pending_dry_raw > raw) {
        soil::set_calibration(_pending_dry_raw, raw);
        char msg[64];
        snprintf(msg, sizeof(msg), "Wet point saved (raw=%u). Calibration complete!", raw);
        _server.send(200, "text/plain", msg);
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg),
                 "Wet point recorded (raw=%u). Now hold in open air and click Capture Dry.", raw);
        _server.send(200, "text/plain", msg);
    }
}

static void handle_calibrate_clear() {
    soil::clear_calibration();
    _pending_dry_raw = 0;
    _pending_wet_raw = 0;
    ESP_LOGI(TAG, "Soil calibration cleared via web");
    _server.send(200, "text/plain", "Calibration cleared");
}

// Redirects OS captive-portal probes to the dashboard so the phone
// automatically presents a "Sign in to network" popup on connect.
static void handle_captive_redirect() {
    _server.sendHeader("Location", "http://192.168.4.1/", true);
    _server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace webserver {

void begin() {
    IPAddress ap_ip(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAPConfig(ap_ip, ap_ip, subnet);
    bool ap_ok = WiFi.softAP(AP_SSID, nullptr, 1, false, 4);
    Serial.printf("WiFi AP %s: SSID=%s IP=%s\n",
                  ap_ok ? "started" : "FAILED",
                  AP_SSID,
                  WiFi.softAPIP().toString().c_str());
    ESP_LOGI(TAG, "AP %s - SSID: %s  IP: %s",
             ap_ok ? "started" : "FAILED",
             AP_SSID,
             WiFi.softAPIP().toString().c_str());

    // Seed pending calibration state from whatever is already in NVS so that
    // single-point updates (for example, recapture only the wet point) work.
    {
        soil::CalibrationData cal = soil::get_calibration();
        if (cal.valid) {
            _pending_dry_raw = cal.dry_raw;
            _pending_wet_raw = cal.wet_raw;
        }
    }

    // Wildcard DNS: every hostname resolves to the AP IP so the phone's
    // captive-portal detector fires regardless of which domain it probes.
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(DNS_PORT, "*", ap_ip);
    ESP_LOGI(TAG, "DNS server started (wildcard -> 192.168.4.1)");

    // Main routes
    _server.on("/",                  HTTP_GET,  handle_root);
    _server.on("/graphs",            HTTP_GET,  handle_graphs);
    _server.on("/api/status",        HTTP_GET,  handle_status);
    _server.on("/api/logs",          HTTP_GET,  handle_logs);
    _server.on("/api/log-data",      HTTP_GET,  handle_log_data);
    _server.on("/read-now",          HTTP_POST, handle_read_now);
    _server.on("/start",             HTTP_POST, handle_start);
    _server.on("/stop",              HTTP_POST, handle_stop);
    _server.on("/clear",             HTTP_POST, handle_clear);
    _server.on("/calibrate-dry",     HTTP_POST, handle_calibrate_dry);
    _server.on("/calibrate-wet",     HTTP_POST, handle_calibrate_wet);
    _server.on("/calibrate-clear",   HTTP_POST, handle_calibrate_clear);

    // Captive portal probe URLs for the main operating systems
    _server.on("/hotspot-detect.html",          HTTP_GET, handle_captive_redirect); // iOS / macOS
    _server.on("/library/test/success.html",    HTTP_GET, handle_captive_redirect); // macOS alt
    _server.on("/generate_204",                 HTTP_GET, handle_captive_redirect); // Android
    _server.on("/gen_204",                      HTTP_GET, handle_captive_redirect); // Android alt
    _server.on("/connecttest.txt",              HTTP_GET, handle_captive_redirect); // Windows
    _server.on("/ncsi.txt",                     HTTP_GET, handle_captive_redirect); // Windows alt

    // Catch-all: redirect unknown paths back to the dashboard
    _server.onNotFound(handle_captive_redirect);

    _server.begin();
    ESP_LOGI(TAG, "HTTP server started on port 80");
}

void handle() {
    _dns.processNextRequest();
    _server.handleClient();
}

void set_reading(const env::EnvReading& env, uint16_t lux, const soil::SoilReading& s) {
    _last_env  = env;
    _last_lux  = lux;
    _last_soil = s;
}

void set_logging(bool active) {
    _logging = active;
}

bool is_logging() {
    return _logging;
}

bool consume_read_request() {
    return _read_req.exchange(false);
}

uint32_t get_log_interval_ms() {
    return _log_interval_ms;
}

const char* get_log_name() {
    return _log_name;
}

} // namespace webserver

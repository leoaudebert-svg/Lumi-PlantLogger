#include "plant_webserver.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <esp_log.h>
#include <atomic>
#include "logger.h"

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
static std::atomic<bool> _read_req  {false};

// Pending calibration points — initialised from NVS in begin().
// Both are needed before set_calibration() is called.
static uint16_t _pending_dry_raw = 0;
static uint16_t _pending_wet_raw = 0;

// ---------------------------------------------------------------------------
// HTML dashboard — served from flash via send_P()
// ---------------------------------------------------------------------------
static const char HTML_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PlantLogger</title>
</head>
<body>
<h1>PlantLogger</h1>
<h2>Sensor Readings</h2>
<table border="1" cellpadding="6">
<tr><td>Temperature</td><td id="temp">--</td></tr>
<tr><td>Humidity</td><td id="rh">--</td></tr>
<tr><td>Pressure</td><td id="pres">--</td></tr>
<tr><td>Light</td><td id="lux">--</td></tr>
<tr><td>Soil</td><td id="soil">--</td></tr>
</table>
<p>Logging: <b id="log_state">--</b></p>
<p id="msg" style="color:green"></p>
<br>
<label for="interval">Log interval (s):&nbsp;</label>
<input id="interval" type="number" min="1" value="60" style="width:60px">
<br><br>
<button onclick="cmd('/read-now','POST','Reading...')">Read Now</button>
<button onclick="start_logging()">Start Logging</button>
<button onclick="cmd('/stop','POST','Logging stopped')">Stop Logging</button>
<button id="dl_btn" onclick="download_csv()">Download CSV</button>
<button onclick="cmd('/clear','POST','Log cleared')">Clear Log</button>
<hr>
<h2>Soil Calibration</h2>
<p id="cal_info" style="color:gray">Loading...</p>
<p><b>Step 1 — Dry:</b> Hold the sensor in open air, then click &rarr;
  <button onclick="calibrate('dry')">Capture Dry</button></p>
<p><b>Step 2 — Wet:</b> Submerge the sensor tip in water, then click &rarr;
  <button onclick="calibrate('wet')">Capture Wet</button></p>
<button onclick="cmd('/calibrate-clear','POST','Calibration cleared')">Clear Calibration</button>
<p id="cal_msg" style="color:blue"></p>
<script>
function cmd(url,method,msg){
  fetch(url,{method:method}).then(function(){
    document.getElementById('msg').textContent=msg;
    poll();
  });
}
function start_logging(){
  var secs=parseInt(document.getElementById('interval').value)||60;
  if(secs<1)secs=1;
  cmd('/start?interval='+secs,'POST','Logging started');
}
function download_csv(){
  window.location.href='/download.csv';
}
function calibrate(point){
  fetch('/calibrate-'+point,{method:'POST'}).then(function(r){return r.text();}).then(function(t){
    document.getElementById('cal_msg').textContent=t;
    poll();
  });
}
function poll(){
  fetch('/api/status').then(function(r){return r.json();}).then(function(d){
    document.getElementById('temp').textContent=d.temp_c.toFixed(1)+' \u00b0C';
    document.getElementById('rh').textContent=d.rh_pct.toFixed(1)+' %';
    document.getElementById('pres').textContent=d.pressure_hpa>0?d.pressure_hpa.toFixed(1)+' hPa':'N/A';
    document.getElementById('lux').textContent=d.lux+' lx';
    document.getElementById('soil').textContent=d.soil_calibrated?d.soil_pct.toFixed(0)+' %':'raw='+d.soil_raw;
    document.getElementById('log_state').textContent=d.logging?'ACTIVE':'STOPPED';
    document.getElementById('interval').disabled=d.logging;
    document.getElementById('dl_btn').disabled=d.logging||!d.download_available;
    var dry=d.cal_dry,wet=d.cal_wet,s;
    if(dry>0&&wet>0){s='dry='+dry+' | wet='+wet;}
    else if(dry>0){s='dry='+dry+' | wet=?';}
    else if(wet>0){s='dry=? | wet='+wet;}
    else{s='Not calibrated';}
    document.getElementById('cal_info').textContent='Stored calibration: '+s;
  });
}
poll();
setInterval(poll,5000);
</script>
</body>
</html>
)rawliteral";

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
        "\"soil_calibrated\":%s,\"logging\":%s,\"download_available\":%s,"
        "\"cal_dry\":%u,\"cal_wet\":%u}",
        _last_env.temp_c,
        _last_env.rh_pct,
        _last_env.pressure_pa / 100.0f,
        _last_lux,
        _last_soil.raw,
        _last_soil.pct,
        _last_soil.calibrated ? "true" : "false",
        _logging ? "true" : "false",
        (!_logging && logger::get_latest_filename() != nullptr) ? "true" : "false",
        _pending_dry_raw,
        _pending_wet_raw);
    _server.send(200, "application/json", buf);
}

static void handle_read_now() {
    _read_req = true;
    _server.send(200, "text/plain", "OK");
}

static void handle_start() {
    if (_logging) {
        _server.send(409, "text/plain", "Already logging — stop the current session first");
        return;
    }
    String interval_str = _server.arg("interval");
    if (interval_str.length() > 0) {
        long interval_secs = interval_str.toInt();
        if (interval_secs > 0) {
            _log_interval_ms = static_cast<uint32_t>(interval_secs) * 1000UL;
        }
    }
    _logging = true;
    ESP_LOGI(TAG, "Logging started via web (interval: %u ms)", _log_interval_ms);
    _server.send(200, "text/plain", "OK");
}

static void handle_stop() {
    _logging = false;
    ESP_LOGI(TAG, "Logging stopped via web");
    _server.send(200, "text/plain", "OK");
}

static void handle_download_csv() {
    if (_logging) {
        _server.send(409, "text/plain", "Stop logging before downloading");
        return;
    }
    const char* path = logger::get_latest_filename();
    if (path == nullptr) {
        _server.send(404, "text/plain", "No log file available");
        return;
    }
    File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        _server.send(500, "text/plain", "Failed to open log file");
        return;
    }
    _server.sendHeader("Content-Disposition", "attachment; filename=\"plantlogger.csv\"");
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
    // single-point updates (e.g. recapture only the wet point) work correctly.
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
    _server.on("/api/status",        HTTP_GET,  handle_status);
    _server.on("/read-now",          HTTP_POST, handle_read_now);
    _server.on("/start",             HTTP_POST, handle_start);
    _server.on("/stop",              HTTP_POST, handle_stop);
    _server.on("/download.csv",      HTTP_GET,  handle_download_csv);
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

} // namespace webserver

#include "webserver.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_log.h>
#include <atomic>

static const char* TAG = "webserver";

// ---------------------------------------------------------------------------
// Network constants
// ---------------------------------------------------------------------------
static constexpr char    AP_SSID[]            = "PlantLogger";
static constexpr uint8_t DNS_PORT             = 53;
static constexpr size_t  JSON_STATUS_BUF_SIZE = 256;

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
    // Buffer sized for the fixed-format JSON; largest realistic values fit well
    // within 256 bytes (verified by inspection of the format string).
    char buf[JSON_STATUS_BUF_SIZE];
    snprintf(buf, JSON_STATUS_BUF_SIZE,
        "{\"temp_c\":%.1f,\"rh_pct\":%.1f,\"pressure_hpa\":%.1f,"
        "\"lux\":%u,\"soil_raw\":%u,\"soil_pct\":%.0f,"
        "\"soil_calibrated\":%s,\"logging\":%s}",
        _last_env.temp_c,
        _last_env.rh_pct,
        _last_env.pressure_pa / 100.0f,
        _last_lux,
        _last_soil.raw,
        _last_soil.pct,
        _last_soil.calibrated ? "true" : "false",
        _logging ? "true" : "false");
    _server.send(200, "application/json", buf);
}

static void handle_read_now() {
    _read_req = true;
    _server.send(200, "text/plain", "OK");
}

static void handle_start() {
    String interval_str = _server.arg("interval");
    if (interval_str.length() > 0) {
        long secs = interval_str.toInt();
        if (secs > 0) {
            _log_interval_ms = static_cast<uint32_t>(secs) * 1000UL;
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
    _server.sendHeader("Content-Disposition", "attachment; filename=\"plantlogger.csv\"");
    // Placeholder: real log data will come from LittleFS in a later phase.
    _server.send(200, "text/csv", "timestamp,temp,rh,lux,soil\r\n");
}

static void handle_clear() {
    // Placeholder: LittleFS integration pending.
    ESP_LOGI(TAG, "/clear requested (placeholder — LittleFS integration pending)");
    _server.send(200, "text/plain", "OK");
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
    WiFi.softAPConfig(ap_ip, ap_ip, subnet);
    WiFi.softAP(AP_SSID);
    ESP_LOGI(TAG, "AP started — SSID: %s  IP: 192.168.4.1", AP_SSID);

    // Wildcard DNS: every hostname resolves to the AP IP so the phone's
    // captive-portal detector fires regardless of which domain it probes.
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(DNS_PORT, "*", ap_ip);
    ESP_LOGI(TAG, "DNS server started (wildcard -> 192.168.4.1)");

    // Main routes
    _server.on("/",             HTTP_GET,  handle_root);
    _server.on("/api/status",   HTTP_GET,  handle_status);
    _server.on("/read-now",     HTTP_POST, handle_read_now);
    _server.on("/start",        HTTP_POST, handle_start);
    _server.on("/stop",         HTTP_POST, handle_stop);
    _server.on("/download.csv", HTTP_GET,  handle_download_csv);
    _server.on("/clear",        HTTP_POST, handle_clear);

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

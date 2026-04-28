// Production entry point for PlantLogger.

#include <M5Unified.h>
#include "button.h"
#include "dlight.h"
#include "env_driver.h"
#include "soil.h"
#include "webserver.h"
#include "logger.h"

static const char* TAG = "main";

// ADC1 pins on the ESP32-S3 are GPIO 1–10.
// GPIO 1 (SDA) and GPIO 2 (SCL) are used by the Grove I2C sensors, so
// choose any of GPIO 3–10 that is physically accessible on your board.
static constexpr uint8_t SOIL_ADC_PIN = 6;

// Logging transition tracking
static bool     _was_logging  = false;
static uint32_t _last_log_ms  = 0;
static bool     _fs_ok        = false;  // set from logger::begin(); gates session operations

// Read all sensors, push the snapshot to the web dashboard, and (if logging
// is active) append the row to the current session file.
static void do_sensor_read()
{
    env::EnvReading   env_r  = env::is_ok()          ? env::read()        : env::EnvReading{};
    uint16_t          lux    = dlight::is_available() ? dlight::read_lux() : 0;
    soil::SoilReading soil_r = soil::is_ok()          ? soil::read()       : soil::SoilReading{};

    webserver::set_reading(env_r, lux, soil_r);

    if (logger::is_session_open()) {
        logger::append_row(millis(), env_r, lux, soil_r);
    }

    ESP_LOGI(TAG, "Read: %.1f C  %.1f %%  %u lx  soil raw=%u",
             env_r.temp_c, env_r.rh_pct, lux, soil_r.raw);
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("PlantLogger", M5.Display.width() / 2, M5.Display.height() / 2);

    env::begin();
    dlight::init();
    soil::begin(SOIL_ADC_PIN);
    _fs_ok = logger::begin();
    if (!_fs_ok) {
        ESP_LOGE(TAG, "LittleFS unavailable — logging is disabled for this session");
    }

    // Populate the dashboard with an initial reading before the AP is up.
    do_sensor_read();

    webserver::begin();

    ESP_LOGI(TAG, "PlantLogger started.");
}

void loop()
{
    M5.update();
    button::update();

    webserver::handle();

    bool logging_now = webserver::is_logging();

    // Detect logging transitions so logger sessions are opened/closed in sync
    // with the flag, regardless of whether it was set by the web UI or the
    // physical button.
    if (logging_now && !_was_logging) {
        if (!_fs_ok) {
            ESP_LOGW(TAG, "Logging requested but LittleFS is unavailable — ignoring");
            webserver::set_logging(false);
        } else {
            logger::start_new_session();
            // Trigger an immediate first reading at the start of the session.
            // Back-date so the interval check fires immediately on the first tick.
            _last_log_ms = millis() - webserver::get_log_interval_ms();
            ESP_LOGI(TAG, "Logging started");
        }
    } else if (!logging_now && _was_logging) {
        logger::stop_session();
        ESP_LOGI(TAG, "Logging stopped");
    }
    _was_logging = logging_now;

    // Web button or physical short-press → one immediate sensor read.
    if (webserver::consume_read_request() || button::was_clicked()) {
        do_sensor_read();
    }

    // Interval-based periodic read while a logging session is active.
    if (logging_now) {
        uint32_t now = millis();
        if (now - _last_log_ms >= webserver::get_log_interval_ms()) {
            _last_log_ms = now;
            do_sensor_read();
        }
    }

    // Physical long-press → toggle logging on/off.
    if (button::was_long_press()) {
        bool next = !webserver::is_logging();
        webserver::set_logging(next);
        ESP_LOGI(TAG, "Logging %s via button", next ? "started" : "stopped");
    }

    vTaskDelay(1);
}

// Production entry point for PlantLogger.

#include <M5Unified.h>
#include <Avatar.h>
#include "button.h"
#include "dlight.h"
#include "env_driver.h"
#include "soil.h"
#include "plant_webserver.h"
#include "logger.h"

static const char* TAG = "main";

using namespace m5avatar;

static Avatar _avatar;

// ADC1 pins on the ESP32-S3 are GPIO 1–10.
// GPIO 2 (SDA) and GPIO 1 (SCL) are used by the Grove I2C sensors, so
// choose any of GPIO 3–10 that is physically accessible on your board.
static constexpr uint8_t SOIL_ADC_PIN = 6;

// Logging transition tracking
static bool     _was_logging  = false;
static uint32_t _last_log_ms  = 0;
static bool     _fs_ok        = false;  // set from logger::begin(); gates session operations

// Cached sensor readings — updated by do_sensor_read() and sent to the web dashboard.
static env::EnvReading   _last_env_r  = {};
static uint16_t          _last_lux    = 0;
static soil::SoilReading _last_soil_r = {};

// The happy face is the steady state. A short variation is shown every ten
// seconds without delaying sensor reads or web-server handling.
static uint32_t _next_avatar_variation_ms = 0;
static uint32_t _avatar_variation_end_ms  = 0;
static bool     _avatar_variation_active  = false;
static float    _avatar_gaze_vertical   = 0.0f;
static float    _avatar_gaze_horizontal = 0.0f;
static uint8_t  _avatar_expression_idx  = 0;

static constexpr Expression AVATAR_EXPRESSIONS[] = {
    Expression::Neutral,
    Expression::Doubt,
    Expression::Sleepy,
};

static bool time_reached(uint32_t now, uint32_t deadline)
{
    return static_cast<int32_t>(now - deadline) >= 0;
}

static void set_avatar_gaze(float vertical, float horizontal)
{
    // The library holds a separate gaze vector for each eye.
    _avatar.setLeftGaze(vertical, horizontal);
    _avatar.setRightGaze(horizontal, horizontal);
}

static void update_avatar_idle()
{
    const uint32_t now = millis();

    // The Avatar's own facial task changes gaze periodically. Reapply our
    // current target while a variation is active so each glance is visible.
    if (_avatar_variation_active) {
        set_avatar_gaze(_avatar_gaze_vertical, _avatar_gaze_horizontal);
    }

    if (_avatar_variation_active && time_reached(now, _avatar_variation_end_ms)) {
        _avatar.setMouthOpenRatio(0.0f);
        _avatar.setLeftEyeOpenRatio(1.0f);
        _avatar.setRightEyeOpenRatio(1.0f);
        set_avatar_gaze(0.0f, 0.0f);
        _avatar.setExpression(Expression::Happy);
        _avatar_variation_active = false;
    }

    if (_avatar_variation_active || !time_reached(now, _next_avatar_variation_ms)) {
        return;
    }

    // The standard -1..1 gaze range is almost imperceptible after scaling the
    // 320px Avatar down to 128px, so use a larger horizontal glance.
    _avatar_gaze_vertical = static_cast<float>(random(-4, 5)) / 10.0f;
    _avatar_gaze_horizontal = random(2) == 0 ? -3.0f : 3.0f;
    set_avatar_gaze(_avatar_gaze_vertical, _avatar_gaze_horizontal);

    _avatar.setExpression(
        AVATAR_EXPRESSIONS[_avatar_expression_idx % (sizeof(AVATAR_EXPRESSIONS) / sizeof(AVATAR_EXPRESSIONS[0]))]);
    ++_avatar_expression_idx;

    switch (random(3)) {
        case 0:  // A short, quiet "talking" motion.
            _avatar.setMouthOpenRatio(static_cast<float>(random(35, 80)) / 100.0f);
            break;
        case 1:  // A quick playful wink.
            _avatar.setLeftEyeOpenRatio(0.0f);
            break;
        default: // A wider, surprised-looking open mouth.
            _avatar.setMouthOpenRatio(1.0f);
            break;
    }

    _avatar_variation_active = true;
    _avatar_variation_end_ms = now + random(900, 1600);
    _next_avatar_variation_ms = now + 10000;
}

// Read all sensors, push the snapshot to the web dashboard, and (if logging is
// active) append the row to the current session file. The Avatar owns the LCD.
static void do_sensor_read()
{
    _last_env_r  = env::is_ok()          ? env::read()        : env::EnvReading{};
    _last_lux    = dlight::is_available() ? dlight::read_lux() : 0;
    _last_soil_r = soil::is_ok()          ? soil::read()       : soil::SoilReading{};

    webserver::set_reading(_last_env_r, _last_lux, _last_soil_r);

    if (logger::is_session_open()) {
        logger::append_row(millis(), _last_env_r, _last_lux, _last_soil_r);
    }

    ESP_LOGI(TAG, "Read: %.1f C  %.1f %%  %.1f hPa  %u lx  soil raw=%u",
             _last_env_r.temp_c,
             _last_env_r.rh_pct,
             _last_env_r.pressure_pa / 100.0f,
             _last_lux,
             _last_soil_r.raw);
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("PlantLogger boot");

    auto cfg = M5.config();
    M5.begin(cfg);

    ColorPalette palette;
    palette.set(COLOR_PRIMARY, TFT_WHITE);
    palette.set(COLOR_BACKGROUND, TFT_BLACK);
    _avatar.setColorPalette(palette);
    _avatar.setScale(0.4f);
    _avatar.setPosition(-56, -96);
    _avatar.init();
    _avatar.setExpression(Expression::Happy);
    _next_avatar_variation_ms = millis() + 10000;

    webserver::begin();

    env::begin();
    dlight::init();
    soil::begin(SOIL_ADC_PIN);
    _fs_ok = logger::begin();
    if (!_fs_ok) {
        ESP_LOGE(TAG, "LittleFS unavailable — logging is disabled for this session");
    }

    // Initial sensor read populates the dashboard. The Avatar owns the display.
    do_sensor_read();

    ESP_LOGI(TAG, "PlantLogger started.");
    Serial.println("PlantLogger started");
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
            logging_now = false;
        } else {
            if (logger::start_new_session(webserver::get_log_name())) {
                // Trigger an immediate first reading at the start of the session.
                // Back-date so the interval check fires immediately on the first tick.
                _last_log_ms = millis() - webserver::get_log_interval_ms();
                ESP_LOGI(TAG, "Logging started");
            } else {
                ESP_LOGE(TAG, "Logging requested but session could not be opened");
                webserver::set_logging(false);
                logging_now = false;
            }
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

    // Long-press logging is disabled because every session now needs a name
    // from the dashboard before it can start.

    update_avatar_idle();  // Timestamp checks only; never waits or blocks I/O.
    vTaskDelay(1);
}

#include "soil.h"

#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

// ---------------------------------------------------------------------------
// NVS / calibration constants
// ---------------------------------------------------------------------------
static constexpr char     SOIL_NVS_NS[]      = "soil_cal";
static constexpr char     SOIL_NVS_DRY[]     = "dry_raw";
static constexpr char     SOIL_NVS_WET[]     = "wet_raw";
static constexpr uint8_t  SOIL_DEFAULT_N     = 7;
static constexpr uint16_t SOIL_ADC_MAX       = 4095;

static const char* TAG = "soil";

namespace soil {

    static uint8_t         _pin         = 0;
    static bool            _initialized = false;
    static CalibrationData _cal         = {0, 0, false};
    static Preferences     _prefs;

    // -----------------------------------------------------------------------

    bool begin(uint8_t pin) {
        // Minimal validity check: ESP32-S3 ADC1 channels are on GPIOs 1–10.
        // ADC2 shares pins with Wi-Fi and is unreliable; reject any pin
        // outside the ADC1 range to catch obvious wiring mistakes early.
        if (pin == 0 || pin > 10) {
            ESP_LOGE(TAG, "begin() rejected: GPIO %u is not an ADC1 pin (use GPIO 1–10)", pin);
            return false;
        }

        _pin = pin;

        analogReadResolution(12);
        analogSetPinAttenuation(_pin, ADC_11db);

        // Load persisted calibration from NVS (read-only mode).
        _prefs.begin(SOIL_NVS_NS, /*readOnly=*/true);
        uint16_t stored_dry = _prefs.getUShort(SOIL_NVS_DRY, 0);
        uint16_t stored_wet = _prefs.getUShort(SOIL_NVS_WET, 0);
        _prefs.end();

        if (stored_dry > stored_wet && stored_dry > 0) {
            _cal = {stored_dry, stored_wet, true};
            ESP_LOGI(TAG, "Calibration loaded from NVS — dry=%u wet=%u", stored_dry, stored_wet);
        } else {
            _cal = {0, 0, false};
            ESP_LOGW(TAG, "No valid calibration in NVS — readings will lack pct");
        }

        _initialized = true;
        ESP_LOGI(TAG, "SEN0193 initialised on GPIO %u (12-bit, ADC_11db)", _pin);
        return true;
    }

    // -----------------------------------------------------------------------

    bool is_ok() {
        return _initialized;
    }

    // -----------------------------------------------------------------------

    uint16_t read_raw() {
        if (!_initialized) {
            ESP_LOGW(TAG, "read_raw() called before begin()");
            return 0;
        }
        return static_cast<uint16_t>(analogRead(_pin));
    }

    // -----------------------------------------------------------------------

    uint16_t read_median(uint8_t n) {
        if (!_initialized) {
            ESP_LOGW(TAG, "read_median() called before begin()");
            return 0;
        }

        // Guard against unreasonable burst sizes to keep stack usage bounded.
        if (n == 0) {
            n = SOIL_DEFAULT_N;
        }
        if (n > 15) {
            n = 15;
        }

        uint16_t buf[15];
        for (uint8_t i = 0; i < n; ++i) {
            buf[i] = static_cast<uint16_t>(analogRead(_pin));
        }

        // Insertion sort — O(n²) is fine for n ≤ 15.
        for (uint8_t i = 1; i < n; ++i) {
            uint16_t key = buf[i];
            int      j   = static_cast<int>(i) - 1;
            while (j >= 0 && buf[j] > key) {
                buf[j + 1] = buf[j];
                --j;
            }
            buf[j + 1] = key;
        }

        return buf[n / 2];
    }

    // -----------------------------------------------------------------------

    SoilReading read() {
        SoilReading result = {0, 0.0f, false};

        result.raw = read_median(SOIL_DEFAULT_N);

        if (_cal.valid) {
            // Map: dry_raw → 0 %, wet_raw → 100 %.
            // cast to long to match Arduino map() signature.
            long pct = map(static_cast<long>(result.raw),
                           static_cast<long>(_cal.dry_raw),
                           static_cast<long>(_cal.wet_raw),
                           0L, 100L);
            result.pct        = static_cast<float>(constrain(pct, 0L, 100L));
            result.calibrated = true;
        }

        return result;
    }

    // -----------------------------------------------------------------------

    void set_calibration(uint16_t dry_raw, uint16_t wet_raw) {
        if (dry_raw <= wet_raw) {
            ESP_LOGW(TAG,
                     "set_calibration() rejected: dry_raw (%u) must be > wet_raw (%u)",
                     dry_raw, wet_raw);
            return;
        }

        _prefs.begin(SOIL_NVS_NS, /*readOnly=*/false);
        _prefs.putUShort(SOIL_NVS_DRY, dry_raw);
        _prefs.putUShort(SOIL_NVS_WET, wet_raw);
        _prefs.end();

        _cal = {dry_raw, wet_raw, true};
        ESP_LOGI(TAG, "Calibration saved — dry=%u wet=%u", dry_raw, wet_raw);
    }

    // -----------------------------------------------------------------------

    CalibrationData get_calibration() {
        return _cal;
    }

    // -----------------------------------------------------------------------

    bool is_calibrated() {
        return _cal.valid;
    }

    // -----------------------------------------------------------------------

    void clear_calibration() {
        _prefs.begin(SOIL_NVS_NS, /*readOnly=*/false);
        _prefs.remove(SOIL_NVS_DRY);
        _prefs.remove(SOIL_NVS_WET);
        _prefs.end();

        _cal = {0, 0, false};
        ESP_LOGI(TAG, "Calibration cleared");
    }

} // namespace soil

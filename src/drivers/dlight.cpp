#include "dlight.h"
#include <M5_DLight.h>
#include <Wire.h>
#include <esp_log.h>

// ---------------------------------------------------------------------------
// Hardware constants — AtomS3R Grove / HY2.0-4P port
// Promote to a shared config.h once the full sensor stack is wired.
// ---------------------------------------------------------------------------
static constexpr uint8_t  DLIGHT_SDA_PIN  = 1;
static constexpr uint8_t  DLIGHT_SCL_PIN  = 2;
static constexpr uint32_t DLIGHT_I2C_FREQ = 400000UL;
static constexpr uint8_t  DLIGHT_I2C_ADDR = 0x23;

static const char* TAG = "dlight";

namespace dlight {
    static M5_DLight _sensor;
    static bool      _available = false;

    bool init() {
        // Probe the I2C address before initialising the driver so that a
        // missing sensor is detected cleanly rather than silently ignored.
        // Wire is initialised here; _sensor.begin() reuses the same bus.
        Wire.begin(DLIGHT_SDA_PIN, DLIGHT_SCL_PIN, DLIGHT_I2C_FREQ);
        Wire.beginTransmission(DLIGHT_I2C_ADDR);
        uint8_t err = Wire.endTransmission();

        if (err != 0) {
            ESP_LOGE(TAG, "BH1750 not found at 0x%02X (err=%d)", DLIGHT_I2C_ADDR, err);
            _available = false;
            return false;
        }

        // Pass the already-initialised Wire instance so the library does not
        // reinitialise I2C with different parameters.
        _sensor.begin(&Wire);
        _sensor.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
        _available = true;
        ESP_LOGI(TAG, "BH1750 initialised at 0x%02X, CONTINUOUSLY_H_RESOLUTION_MODE", DLIGHT_I2C_ADDR);
        return true;
    }

    bool is_available() {
        return _available;
    }

    uint16_t read_lux() {
        if (!_available) {
            ESP_LOGW(TAG, "read_lux() called but sensor is unavailable");
            return 0;
        }
        return _sensor.getLUX();
    }

    void power_on() {
        if (_available) {
            _sensor.powerOn();
        }
    }

    void power_off() {
        if (_available) {
            _sensor.powerOff();
        }
    }
}

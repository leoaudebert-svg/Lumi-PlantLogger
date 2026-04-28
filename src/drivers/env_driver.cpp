#include "env_driver.h"

#include <Wire.h>
#include <SHT3X.h>
#include <QMP6988.h>

static const char* TAG = "env";

static constexpr uint8_t SHT30_ADDR   = 0x44;
static constexpr uint8_t QMP6988_ADDR = 0x70;

static SHT3X   _sht30;
static QMP6988 _qmp6988;

// Tracks whether begin() succeeded for the critical sensor (SHT30).
static bool _initialized = false;

// Tracks whether the optional pressure sensor came up cleanly.
static bool _qmp_available = false;

namespace env {

bool begin() {
    // Wire is expected to be ready — M5.begin() initializes it before setup() continues.

    bool sht_ok = _sht30.begin(&Wire, SHT30_ADDR);
    if (!sht_ok) {
        ESP_LOGE(TAG, "SHT30 init failed (addr 0x%02X) — check wiring", SHT30_ADDR);
        _initialized  = false;
        _qmp_available = false;
        return false;
    }
    ESP_LOGI(TAG, "SHT30 initialized (addr 0x%02X)", SHT30_ADDR);

    uint8_t qmp_ret = _qmp6988.init(&Wire, QMP6988_ADDR);
    _qmp_available = (qmp_ret == 0);
    if (!_qmp_available) {
        ESP_LOGW(TAG, "QMP6988 init failed (addr 0x%02X, ret=%d) — pressure unavailable",
                 QMP6988_ADDR, qmp_ret);
    } else {
        ESP_LOGI(TAG, "QMP6988 initialized (addr 0x%02X)", QMP6988_ADDR);
    }

    _initialized = true;
    return true;
}

bool is_ok() {
    return _initialized;
}

EnvReading read() {
    EnvReading result = {};

    if (!_initialized) {
        ESP_LOGW(TAG, "env::read() called before env::begin()");
        return result;
    }

    result.sht_ok = _sht30.get();
    if (result.sht_ok) {
        result.temp_c = _sht30.cTemp;
        result.rh_pct = _sht30.humidity;
    } else {
        ESP_LOGE(TAG, "SHT30 read failed");
    }

    if (_qmp_available) {
        result.pressure_pa = _qmp6988.calcPressure();
        result.qmp_ok      = true;
    }

    return result;
}

} // namespace env

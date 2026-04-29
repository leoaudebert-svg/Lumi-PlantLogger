#include "env_driver.h"

#include <Wire.h>
#include <SHT3X.h>
#include <QMP6988.h>

static const char* TAG = "env";

static constexpr uint8_t SHT30_ADDR   = 0x44;
static constexpr uint8_t QMP6988_ADDR = 0x70;
static constexpr uint8_t I2C_SDA_PIN  = 2;
static constexpr uint8_t I2C_SCL_PIN  = 1;
static constexpr uint32_t I2C_FREQ_HZ  = 400000UL;

static SHT3X   _sht30;
static QMP6988 _qmp6988;

// Tracks whether begin() succeeded for the critical sensor (SHT30).
static bool _initialized = false;

// Tracks whether the optional pressure sensor came up cleanly.
static bool _qmp_available = false;

namespace env {

bool begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

    // Wire is expected to be ready — M5.begin() initializes it before setup() continues.

    bool sht_ok = _sht30.init(SHT30_ADDR, &Wire);
    if (!sht_ok) {
        ESP_LOGE(TAG, "SHT30 init failed (addr 0x%02X) — check wiring", SHT30_ADDR);
        _initialized  = false;
        _qmp_available = false;
        return false;
    }
    ESP_LOGI(TAG, "SHT30 initialized (addr 0x%02X)", SHT30_ADDR);

    uint8_t qmp_ret = _qmp6988.init(QMP6988_ADDR, &Wire);
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

    byte sht_status = _sht30.get();
    result.sht_ok = (sht_status == 0);
    if (result.sht_ok) {
        result.temp_c = _sht30.cTemp;
        result.rh_pct = _sht30.humidity;
    } else {
        ESP_LOGE(TAG, "SHT30 read failed (status=%u)", sht_status);
    }

    if (_qmp_available) {
        result.pressure_pa = _qmp6988.calcPressure();
        result.qmp_ok      = true;
    }

    return result;
}

} // namespace env

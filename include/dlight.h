#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// DLight Driver — thin wrapper over M5_DLight (BH1750FVI, I2C 0x23)
//
// Responsibilities:
//   - Initialize the BH1750 sensor on the Grove I2C bus
//   - Provide raw lux readings to the logic layer
//   - Expose power control for future low-power duty-cycling
//
// Usage contract:
//   1. Call dlight::init() once in setup(), after M5.begin().
//   2. Check dlight::is_available() before reading if the sensor may be absent.
//   3. Call dlight::read_lux() from the scheduler to obtain a raw lux value.
//   4. Filtering, aggregation, and DLI are handled by higher layers.
// ---------------------------------------------------------------------------

namespace dlight {
    // Initializes the I2C bus and BH1750 sensor.
    // Probes address 0x23; sets CONTINUOUSLY_H_RESOLUTION_MODE on success.
    // Returns true if the sensor was found and configured, false otherwise.
    bool init();

    // Returns true if the last init() succeeded and the sensor is usable.
    bool is_available();

    // Returns the current raw lux reading from the BH1750 (1–65535 lux).
    // Returns 0 and emits ESP_LOGW if the sensor is unavailable.
    uint16_t read_lux();

    // Powers the BH1750 on (for wake after duty-cycle sleep).
    void power_on();

    // Powers the BH1750 off (for duty-cycle power saving, Phase 4+).
    void power_off();
}

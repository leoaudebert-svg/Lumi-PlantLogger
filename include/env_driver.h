#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// ENV Driver — thin wrapper over M5Unit-ENV (SHT30 + QMP6988)
//
// Responsibilities:
//   - Initialise and read the M5Stack ENV III unit (SHT30 temp/RH, QMP6988 pressure)
//   - Return raw sensor measurements only; no filtering or derived values
//
// Usage contract:
//   1. Call env::begin() once in setup(), after M5.begin() (which initializes Wire).
//   2. Guard reads with env::is_ok() to skip the sensor if initialisation failed.
//   3. Call env::read() to obtain a single raw snapshot of all sensor values.
//
// Hardware:
//   SHT30   — I2C address 0x44, temperature ±0.2 °C, humidity ±2 % RH
//   QMP6988 — I2C address 0x70, pressure (optional for MVP)
// ---------------------------------------------------------------------------

namespace env {

    struct EnvReading {
        float temp_c;       // Temperature in °C (SHT30)
        float rh_pct;       // Relative humidity in % (SHT30)
        float pressure_pa;  // Atmospheric pressure in Pa (QMP6988); 0.0 if unavailable
        bool  sht_ok;       // true if the SHT30 read succeeded
        bool  qmp_ok;       // true if the QMP6988 read succeeded
    };

    // Initialize both sensors over the shared I2C bus.
    // Must be called once in setup(), after M5.begin().
    // Returns false if the SHT30 (critical sensor) fails to initialise.
    // A QMP6988 failure is non-fatal: begin() still returns true and
    // subsequent reads will set qmp_ok = false.
    bool begin();

    // Returns true if begin() completed successfully (SHT30 is online).
    bool is_ok();

    // Perform a single synchronous read of both sensors.
    // Returns an EnvReading with per-sensor ok flags.
    // Logs a warning and returns a zeroed reading if called before begin().
    EnvReading read();

} // namespace env

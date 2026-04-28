#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Soil Moisture Driver — DFRobot SEN0193 Capacitive Soil Moisture Sensor
//
// Responsibilities:
//   - Configure the ADC pin for the sensor analog output
//   - Provide raw and median-filtered ADC readings to the logic layer
//   - Apply dry/wet calibration to compute a moisture percentage
//   - Persist calibration values in NVS via Preferences
//
// Usage contract:
//   1. Call soil::begin(pin) once in setup(), after M5.begin().
//   2. Guard reads with soil::is_ok() to skip if initialisation failed.
//   3. Call soil::read() from the scheduler for a calibrated reading.
//   4. Call soil::set_calibration() with known dry/wet ADC values before
//      trusting soil::read().pct; check soil::is_calibrated() first.
//
// Hardware:
//   DFRobot SEN0193 — analog output, higher voltage = drier soil
//   Supply: 3.3 V; output ~1.0 V (wet) to ~2.8 V (dry)
//   ADC: 12-bit, ADC_11db attenuation (usable linear range ~0.15–2.45 V;
//        full-scale 3.3 V, but non-linearity increases above ~2.45 V)
// ---------------------------------------------------------------------------

namespace soil {

    struct CalibrationData {
        uint16_t dry_raw;  // ADC reading in air (fully dry)
        uint16_t wet_raw;  // ADC reading submerged in water (fully wet)
        bool     valid;    // true only when both values have been stored
    };

    struct SoilReading {
        uint16_t raw;         // Median-filtered ADC value from the burst
        float    pct;         // Calibrated moisture percentage (0–100)
        bool     calibrated;  // true if valid calibration was applied to pct
    };

    // Configure the ADC pin and load calibration from NVS.
    // Must be called once in setup(), after M5.begin().
    // Note: analogReadResolution(12) is a global setting that affects all ADC
    // reads in the application, not just this driver's pin.
    // Returns false if the pin is not a valid ADC-capable GPIO.
    bool begin(uint8_t pin);

    // Returns true if begin() completed successfully.
    bool is_ok();

    // Perform a single raw analogRead().
    // Returns 0 and emits ESP_LOGW if called before begin().
    uint16_t read_raw();

    // Collect n raw samples, insertion-sort them, return the median.
    // Default n = 7 matches the preferred burst size in PROJECT_CONTEXT.md.
    // No delay() calls — all reads are back-to-back analogRead().
    uint16_t read_median(uint8_t n = 7);

    // Return a full reading: median-filtered raw value plus calibrated pct.
    // pct is only meaningful when calibrated == true.
    SoilReading read();

    // Persist a dry/wet calibration pair to NVS and update in-memory state.
    // dry_raw must be greater than wet_raw (higher voltage = drier); logs a
    // warning and returns without writing if this invariant is violated.
    void set_calibration(uint16_t dry_raw, uint16_t wet_raw);

    // Return the current in-memory calibration (check .valid before use).
    CalibrationData get_calibration();

    // Convenience: returns true if a valid calibration is loaded.
    bool is_calibrated();

    // Erase calibration from NVS and invalidate in-memory state.
    void clear_calibration();

} // namespace soil

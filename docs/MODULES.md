# Module API Reference

This document describes the public C++ API exposed by each module in
PlantLogger.  All modules live in the `include/` headers and are implemented
under `src/`.

---

## `namespace button`

**Header:** `include/button.h`  
**Source:** `src/drivers/button.cpp`  
**Dependency:** M5Unified (`M5.BtnA`)

Thin wrapper over the M5Unified built-in button, providing a stable named
interface that hides the M5Unified type details.

### Usage Contract

1. Call `M5.update()` in `loop()` — this is what actually samples the button.
2. Call `button::update()` immediately after (currently a no-op; reserved for
   future expansion).
3. Query state with the functions below.

### Functions

| Signature | Returns | Description |
|---|---|---|
| `void update()` | — | Must be called once per loop after `M5.update()`. |
| `bool was_pressed()` | `bool` | True on the rising edge (button just pressed). |
| `bool was_released()` | `bool` | True on the falling edge (button just released). |
| `bool is_pressed()` | `bool` | True while the button is held down. |
| `bool was_long_press()` | `bool` | True after a long hold is detected. |
| `bool was_clicked()` | `bool` | True after a short press-and-release. |
| `bool was_double_clicked()` | `bool` | True after two rapid clicks. |
| `bool is_holding()` | `bool` | True continuously while the button is being held. |
| `int get_click_count()` | `int` | Number of clicks detected in a multi-click sequence. |

All `was_*` functions return true **once** per event; the state clears on the
next `M5.update()` call.

---

## `namespace dlight`

**Header:** `include/dlight.h`  
**Source:** `src/drivers/dlight.cpp`  
**Dependency:** M5_DLight (BH1750FVI)

Driver for the M5Stack DLight ambient light sensor (BH1750).

### Usage Contract

1. Call `dlight::init()` once in `setup()`, after `M5.begin()`.
2. Check `dlight::is_available()` before reading; skip gracefully if absent.
3. Call `dlight::read_lux()` to obtain a raw lux value.

### Functions

| Signature | Returns | Description |
|---|---|---|
| `bool init()` | `bool` | Initialise the I2C bus and BH1750.  Returns `true` on success. |
| `bool is_available()` | `bool` | True if `init()` succeeded and the sensor is usable. |
| `uint16_t read_lux()` | `uint16_t` | Current raw lux reading (1–65535).  Returns 0 and logs a warning if unavailable. |
| `void power_on()` | — | Wake the sensor after a duty-cycle sleep. |
| `void power_off()` | — | Put the sensor into power-down mode (Phase 4+ feature). |

### Hardware Constants (internal)

| Constant | Value | Description |
|---|---|---|
| `DLIGHT_SDA_PIN` | 1 | Grove SDA pin |
| `DLIGHT_SCL_PIN` | 2 | Grove SCL pin |
| `DLIGHT_I2C_FREQ` | 400000 Hz | I2C bus speed |
| `DLIGHT_I2C_ADDR` | 0x23 | BH1750 I2C address |

---

## `namespace env`

**Header:** `include/env_driver.h`  
**Source:** `src/drivers/env_driver.cpp`  
**Dependency:** M5Unit-ENV (SHT3X, QMP6988)

Driver for the M5Stack ENV III unit: SHT30 (temperature/humidity) and the
optional QMP6988 (pressure).

### Usage Contract

1. Call `env::begin()` once in `setup()`, after `M5.begin()` (which
   initialises `Wire`).
2. Guard reads with `env::is_ok()`.
3. Call `env::read()` to get a snapshot of all sensor values.

### Types

#### `env::EnvReading`

```cpp
struct EnvReading {
    float temp_c;       // Temperature in °C (SHT30)
    float rh_pct;       // Relative humidity in % (SHT30)
    float pressure_pa;  // Pressure in Pa (QMP6988); 0.0 if unavailable
    bool  sht_ok;       // true if the SHT30 read succeeded
    bool  qmp_ok;       // true if the QMP6988 read succeeded
};
```

### Functions

| Signature | Returns | Description |
|---|---|---|
| `bool begin()` | `bool` | Initialise SHT30 and QMP6988.  Returns `false` if SHT30 fails (critical).  QMP6988 failure is non-fatal. |
| `bool is_ok()` | `bool` | True if `begin()` completed with SHT30 online. |
| `EnvReading read()` | `EnvReading` | Synchronous read of all sensors.  Per-sensor `ok` flags indicate success. |

---

## `namespace soil`

**Header:** `include/soil.h`  
**Source:** `src/drivers/soil.cpp`  
**Dependency:** Arduino ADC, Preferences (NVS)

Driver for the DFRobot SEN0193 capacitive soil moisture sensor, with
two-point calibration persisted to NVS.

### Usage Contract

1. Call `soil::begin(pin)` once in `setup()`, after `M5.begin()`.
2. Guard reads with `soil::is_ok()`.
3. Call `soil::set_calibration()` before trusting `SoilReading::pct`.
4. Check `soil::is_calibrated()` before displaying a percentage.

### Types

#### `soil::CalibrationData`

```cpp
struct CalibrationData {
    uint16_t dry_raw;  // ADC reading in open air (sensor fully dry)
    uint16_t wet_raw;  // ADC reading submerged in water (sensor fully wet)
    bool     valid;    // true only when both values are stored and valid
};
```

#### `soil::SoilReading`

```cpp
struct SoilReading {
    uint16_t raw;         // Median-filtered 12-bit ADC value
    float    pct;         // Calibrated moisture percentage (0–100)
    bool     calibrated;  // true if valid calibration was applied to pct
};
```

### Functions

| Signature | Returns | Description |
|---|---|---|
| `bool begin(uint8_t pin)` | `bool` | Configure ADC pin, set 12-bit resolution, load calibration from NVS.  Returns `false` if pin is outside GPIO 1–10. |
| `bool is_ok()` | `bool` | True if `begin()` completed successfully. |
| `uint16_t read_raw()` | `uint16_t` | Single raw `analogRead()`.  Returns 0 if not initialised. |
| `uint16_t read_median(uint8_t n = 7)` | `uint16_t` | Collect `n` samples (max 15), insertion-sort, return the median. |
| `SoilReading read()` | `SoilReading` | Full reading: 7-sample median plus calibrated percentage. |
| `void set_calibration(uint16_t dry_raw, uint16_t wet_raw)` | — | Persist dry/wet ADC pair to NVS.  `dry_raw` must be greater than `wet_raw`; logs a warning and returns without writing otherwise. |
| `CalibrationData get_calibration()` | `CalibrationData` | Return current in-memory calibration. |
| `bool is_calibrated()` | `bool` | Convenience: true if a valid calibration is loaded. |
| `void clear_calibration()` | — | Erase calibration from NVS and invalidate in-memory state. |

### Calibration Notes

- **Higher ADC = drier soil** (capacitive sensors output more voltage in dry soil).
- `dry_raw` is always expected to be greater than `wet_raw`.
- The percentage is computed with `map(raw, dry_raw, wet_raw, 0, 100)` and
  clamped to 0–100.

---

## `namespace logger`

**Header:** `include/logger.h`  
**Source:** `src/logger.cpp`  
**Dependency:** LittleFS, Preferences (slot index)

LittleFS-backed CSV session logger with round-robin file rotation.

### Rotation

Up to **5 log files** (`/log_0.csv` … `/log_4.csv`) are kept on flash.  When
a sixth session starts, the slot that would be written to next (oldest data)
is deleted first.  The current slot index is persisted in `/log_index` so
rotation survives reboots.

### Usage Contract

1. Call `logger::begin()` once in `setup()`.
2. Call `logger::start_new_session()` when logging should begin.
3. Call `logger::append_row()` for every sensor snapshot.
4. Call `logger::stop_session()` when logging ends.
5. Use `logger::get_latest_filename()` to find the file for download.
6. Call `logger::clear_all()` to wipe everything.

### Functions

| Signature | Returns | Description |
|---|---|---|
| `bool begin()` | `bool` | Mount LittleFS (formats once on first boot if needed) and restore slot counter.  Returns `false` if mount fails. |
| `bool start_new_session()` | `bool` | Open a new CSV file; apply rotation if needed.  Returns `false` if the file cannot be created. |
| `void append_row(uint32_t ts_ms, const env::EnvReading&, uint16_t lux, const soil::SoilReading&)` | — | Write one data row and flush every 30 rows.  No-op if no session is open. |
| `void stop_session()` | — | Flush remaining rows and close the file. |
| `bool is_session_open()` | `bool` | True while a session file is open for writing. |
| `const char* get_latest_filename()` | `const char*` | Path of the most recently completed file, or `nullptr` if none is available. |
| `void clear_all()` | — | Delete all `/log_*.csv` files and reset the slot counter.  No-op if a session is open. |

### CSV Columns

```
timestamp_ms, temp_c, rh_pct, pressure_hpa, lux, soil_raw, soil_pct
```

See [DATA_FORMAT.md](DATA_FORMAT.md) for full field definitions.

---

## `namespace webserver`

**Header:** `include/webserver.h`  
**Source:** `src/webserver.cpp`  
**Dependency:** WiFi, WebServer, DNSServer, LittleFS

Wi-Fi Access Point, captive-portal DNS server, HTTP dashboard, and JSON API.

### Usage Contract

1. Call `webserver::begin()` once in `setup()`, after all sensor drivers are
   ready.
2. Call `webserver::handle()` in every `loop()` iteration.
3. Call `webserver::set_reading()` after each sensor read to refresh
   `/api/status`.
4. Check `webserver::consume_read_request()` each loop; perform a read if it
   returns true.
5. Use `webserver::set_logging()` / `webserver::is_logging()` to share the
   logging flag with any physical button logic.

### Functions

| Signature | Returns | Description |
|---|---|---|
| `void begin()` | — | Start Wi-Fi AP (`PlantLogger`, 192.168.4.1), wildcard DNS, and HTTP server on port 80. |
| `void handle()` | — | Poll DNS and HTTP servers.  Must be called every `loop()`. |
| `void set_reading(const env::EnvReading&, uint16_t lux, const soil::SoilReading&)` | — | Update the cached sensor snapshot served by `/api/status`. |
| `void set_logging(bool active)` | — | Set the shared logging flag (written by `/start`, `/stop`, and the physical button). |
| `bool is_logging()` | `bool` | Read the shared logging flag. |
| `bool consume_read_request()` | `bool` | Returns `true` exactly once after a `/read-now` POST, then auto-clears. |
| `uint32_t get_log_interval_ms()` | `uint32_t` | Returns the logging interval last set by `/start` (default 60 000 ms). |

For the full list of HTTP endpoints see [API.md](API.md).

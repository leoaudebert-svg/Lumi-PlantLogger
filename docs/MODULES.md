# Module API Reference

This document describes the public C++ modules in PlantLogger. Headers live in
`include/`; implementations live in `src/`.

## `namespace button`

**Header:** `include/button.h`  
**Source:** `src/drivers/button.cpp`  
**Dependency:** M5Unified (`M5.BtnA`)

Thin wrapper over the built-in AtomS3R button.

Usage contract:

1. Call `M5.update()` in `loop()`.
2. Call `button::update()` after `M5.update()`.
3. Query state with the functions below.

| Signature | Returns | Description |
|---|---|---|
| `void update()` | void | Reserved update hook; call once per loop |
| `bool was_pressed()` | bool | True once when the button is pressed |
| `bool was_released()` | bool | True once when the button is released |
| `bool is_pressed()` | bool | True while held |
| `bool was_long_press()` | bool | True once after a long hold |
| `bool was_clicked()` | bool | True once after a short press/release |
| `bool was_double_clicked()` | bool | True once after two quick clicks |
| `bool is_holding()` | bool | True while the button is being held |
| `int get_click_count()` | int | Click count from M5Unified |

Current app behavior:

- Short press triggers one immediate sensor read.
- Long press toggles logging on or off.

## `namespace dlight`

**Header:** `include/dlight.h`  
**Source:** `src/drivers/dlight.cpp`  
**Dependency:** M5_DLight

Driver for the M5Stack DLight ambient light sensor using the BH1750 at I2C
address `0x23`.

| Signature | Returns | Description |
|---|---|---|
| `bool init()` | bool | Initialise the I2C bus and BH1750 |
| `bool is_available()` | bool | True if the sensor initialised successfully |
| `uint16_t read_lux()` | uint16_t | Current lux reading; `0` if unavailable |
| `void power_on()` | void | Wake the sensor |
| `void power_off()` | void | Put the sensor in power-down mode |

## `namespace env`

**Header:** `include/env_driver.h`  
**Source:** `src/drivers/env_driver.cpp`  
**Dependency:** M5Unit-ENV

Driver for the M5Stack ENV III unit:

- SHT30 temperature and humidity at `0x44`
- QMP6988 pressure at `0x70`

```cpp
struct EnvReading {
    float temp_c;
    float rh_pct;
    float pressure_pa;
    bool  sht_ok;
    bool  qmp_ok;
};
```

| Signature | Returns | Description |
|---|---|---|
| `bool begin()` | bool | Initialise SHT30 and QMP6988; SHT30 failure is critical |
| `bool is_ok()` | bool | True if SHT30 is online |
| `EnvReading read()` | `EnvReading` | Synchronous read with per-sensor status flags |

## `namespace soil`

**Header:** `include/soil.h`  
**Source:** `src/drivers/soil.cpp`  
**Dependency:** Arduino ADC, Preferences

Driver for the DFRobot SEN0193 capacitive soil moisture sensor. The sensor is
analog, not I2C. Higher ADC values indicate drier soil.

```cpp
struct CalibrationData {
    uint16_t dry_raw;
    uint16_t wet_raw;
    bool     valid;
};

struct SoilReading {
    uint16_t raw;
    float    pct;
    bool     calibrated;
};
```

| Signature | Returns | Description |
|---|---|---|
| `bool begin(uint8_t pin)` | bool | Configure ADC pin, set 12-bit resolution, load NVS calibration |
| `bool is_ok()` | bool | True if the ADC pin was accepted |
| `uint16_t read_raw()` | uint16_t | Single raw `analogRead()` |
| `uint16_t read_median(uint8_t n = 7)` | uint16_t | Median of up to 15 ADC samples |
| `SoilReading read()` | `SoilReading` | Median raw reading plus calibrated percentage |
| `void set_calibration(uint16_t dry_raw, uint16_t wet_raw)` | void | Persist a valid dry/wet pair to NVS |
| `CalibrationData get_calibration()` | `CalibrationData` | Current in-memory calibration |
| `bool is_calibrated()` | bool | True if a valid calibration is loaded |
| `void clear_calibration()` | void | Erase calibration from NVS |

Calibration rules:

- `dry_raw` must be greater than `wet_raw`.
- Percent is linearly mapped from dry = 0 percent to wet = 100 percent.
- Percent is clamped to 0-100.

## `namespace logger`

**Header:** `include/logger.h`  
**Source:** `src/logger.cpp`  
**Dependency:** LittleFS, Preferences-style slot index file

LittleFS-backed CSV session logger with 5-file round-robin rotation.

```cpp
static constexpr uint8_t MAX_LOG_FILES = 5;
static constexpr size_t  LOG_PATH_LEN  = 16;

struct LogFileInfo {
    uint8_t slot;
    char    path[LOG_PATH_LEN];
    size_t  size_bytes;
    bool    is_latest;
};
```

Usage contract:

1. Call `logger::begin()` once in `setup()`.
2. Call `logger::start_new_session()` when logging starts.
3. Call `logger::append_row()` for every sensor snapshot.
4. Call `logger::stop_session()` when logging ends.
5. Use `logger::get_latest_filename()` or `logger::list_files()` for downloads.
6. Call `logger::clear_all()` only when no session is open.

| Signature | Returns | Description |
|---|---|---|
| `bool begin()` | bool | Mount LittleFS and restore the next write slot |
| `bool start_new_session()` | bool | Open a new CSV file and rotate the target slot if needed |
| `void append_row(uint32_t ts_ms, const env::EnvReading&, uint16_t lux, const soil::SoilReading&)` | void | Append one CSV row; flushes every 30 rows |
| `void stop_session()` | void | Flush pending rows and close the current file |
| `bool is_session_open()` | bool | True while a session file is open |
| `const char* get_latest_filename()` | `const char*` | Most recently completed file, or `nullptr` |
| `const char* get_filename_for_slot(uint8_t slot)` | `const char*` | Completed file for a slot, or `nullptr` |
| `uint8_t list_files(LogFileInfo* out, uint8_t max_items)` | uint8_t | Fill metadata for completed log files, newest-first when possible |
| `void clear_all()` | void | Delete all `/log_*.csv` files and reset rotation |

CSV columns:

```csv
timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct
```

See [DATA_FORMAT.md](DATA_FORMAT.md) for field definitions.

## `namespace webserver`

**Header:** `include/plant_webserver.h`  
**Source:** `src/webserver.cpp`  
**Dependency:** WiFi, WebServer, DNSServer, LittleFS

Wi-Fi Access Point, captive-portal DNS server, embedded dashboard, and HTTP API.

Usage contract:

1. Call `webserver::begin()` once in `setup()`.
2. Call `webserver::handle()` in every `loop()` iteration.
3. Call `webserver::set_reading()` after each sensor read.
4. Check `webserver::consume_read_request()` each loop.
5. Use `webserver::set_logging()` and `webserver::is_logging()` to share the
   logging flag between HTTP handlers and physical button logic.

| Signature | Returns | Description |
|---|---|---|
| `void begin()` | void | Start AP `PlantLogger`, wildcard DNS, routes, and HTTP server |
| `void handle()` | void | Poll DNS and HTTP clients |
| `void set_reading(const env::EnvReading&, uint16_t lux, const soil::SoilReading&)` | void | Update cached `/api/status` values |
| `void set_logging(bool active)` | void | Set shared logging flag |
| `bool is_logging()` | bool | Read shared logging flag |
| `bool consume_read_request()` | bool | True once after a `/read-now` POST |
| `uint32_t get_log_interval_ms()` | uint32_t | Logging interval set by `/start`, default 60000 ms |

HTTP routes:

- `GET /`
- `GET /api/status`
- `GET /api/logs`
- `POST /read-now`
- `POST /start`
- `POST /stop`
- `GET /download.csv`
- `POST /clear`
- `POST /calibrate-dry`
- `POST /calibrate-wet`
- `POST /calibrate-clear`

See [API.md](API.md) for endpoint details.

## TODO - Next Session

- Add a small time service module so browser-provided wall-clock time can be
  stored and exposed cleanly.
- Add logger/session metadata for sample count, current slot, and session start
  time.
- Add storage health helpers for free bytes and filesystem availability.
- Keep all new APIs small and explicit; avoid broad abstractions until the
  first multi-day field trial exposes real needs.

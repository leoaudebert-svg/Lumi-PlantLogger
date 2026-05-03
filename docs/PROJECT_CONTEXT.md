# PlantLogger Project Context For Coding Agents

Version: 2.0  
Status: Source-of-truth document for future coding sessions

## 1. Project Overview

PlantLogger is a modular embedded monitoring and data-logging system for plant
health and indoor growing conditions. It runs on an M5Stack AtomS3R with an
embedded web dashboard and local CSV storage.

Primary objective:

**Reliable sensing and clean logs first; analytics second.**

Current measured values:

- Air temperature
- Relative humidity
- Atmospheric pressure
- Ambient light in lux
- Soil moisture raw ADC
- Soil moisture calibrated percentage

Future derived metrics:

- Vapor Pressure Deficit (VPD)
- Daily Light Integral (DLI)
- Watering event detection
- Soil drying trends
- Plant stress hints

Project priorities:

1. Reliability
2. Clean data
3. Simple local operation
4. Modularity
5. Future analytics support

## 2. Current Working Product

The MVP is working.

Implemented:

- PlatformIO firmware for M5Stack AtomS3R.
- On-device 128 x 128 display.
- ENV III driver for SHT30 and QMP6988.
- DLight driver for BH1750.
- SEN0193 capacitive soil driver with median filtering.
- Dry/wet calibration persisted to NVS.
- Wi-Fi AP named `PlantLogger`.
- Captive-portal DNS and redirects.
- Embedded single-page dashboard from `Frontend/`.
- `/api/status` JSON endpoint.
- `/api/logs` metadata and LittleFS storage endpoint.
- `/api/log-data` graph-data endpoint for completed slots.
- Start/stop/read-now HTTP controls.
- Customer-named log sessions.
- Dashboard graph view for each completed log.
- 5-file LittleFS round-robin log rotation.
- Physical button shortcuts:
  - short press: read now
  - long press: no logging action; sessions require dashboard names

## 3. Hardware Platform

Controller: M5Stack AtomS3R

- ESP32-S3
- 8 MB flash
- 8 MB PSRAM
- Wi-Fi
- 128 x 128 integrated display
- Built-in front button
- HY2.0-4P / Grove I2C port

Framework:

- PlatformIO
- Arduino framework
- C++17-compatible embedded style

## 4. Sensor Stack

### Ambient Light

Sensor: M5Stack DLight / BH1750

- Interface: I2C
- Address: `0x23`
- Current output: raw lux
- Future use: DLI and light trend analysis

### Environment

Sensor: M5Stack ENV III

SHT30:

- Address: `0x44`
- Temperature
- Relative humidity
- Critical for `env::is_ok()`

QMP6988:

- Address: `0x70`
- Atmospheric pressure
- Optional; absence should not stop the logger

### Soil Moisture

Sensor: DFRobot SEN0193 capacitive moisture sensor

- Interface: analog ADC, not I2C
- Default ADC pin: GPIO 6
- Valid ADC1 pins: GPIO 1-10
- I2C uses GPIO 1 and GPIO 2, so practical soil choices are GPIO 3-10
- Higher ADC value means drier soil
- 7-sample median read
- Dry/wet calibration required before trusting percentage

## 5. Firmware Architecture

Current layout:

```text
include/
  button.h
  dlight.h
  env_driver.h
  logger.h
  plant_webserver.h
  soil.h

src/
  main.cpp
  logger.cpp
  webserver.cpp
  drivers/
    button.cpp
    dlight.cpp
    env_driver.cpp
    soil.cpp

Frontend/
  index.html
  style.css
  app.js

scripts/
  embed_frontend.py
```

Architecture rule:

Drivers should stay focused on hardware access and raw readings. Higher layers
handle display, logging, web state, calibration workflow, derived calculations,
and future analytics.

## 6. Runtime Flow

Setup:

1. Start serial and M5.
2. Start the webserver AP.
3. Initialise environment, light, soil, and logger modules.
4. Take an initial sensor reading.
5. Display and dashboard become ready.

Loop:

1. Update M5 and button state.
2. Service DNS and HTTP clients.
3. Detect logging transitions and open/close CSV sessions.
4. Consume read-now requests or button clicks.
5. Read sensors at the active logging interval.
6. Append rows while a logger session is open.
7. Toggle logging on long button press.

Rule:

Avoid blocking logic. The existing code uses one short `vTaskDelay(1)` yield at
the end of `loop()`.

## 7. Data Format

Current CSV header:

```csv
timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct
```

Important:

- `timestamp_ms` is `millis()` since boot, not wall-clock time.
- Soil percentage is only meaningful when calibration is valid.
- Always preserve raw values when adding derived values.
- Prefer adding new columns over replacing existing ones.

## 8. Development Roadmap

Completed MVP:

- Stable local sensor acquisition
- Display dashboard
- Captive-portal web dashboard
- Soil calibration
- CSV logging
- Named log rotation and graph review
- HTTP API for automation

Next phase:

- Field-trial hardening
- Browser-driven clock setting
- Richer status metadata
- Dashboard trend charts
- Stale-data and sensor-failure warnings
- Stronger validation of user inputs

Later:

- VPD
- DLI
- Watering event detection
- Multi-plant metadata
- Optional MQTT or network sync
- Lower-power logging modes

## 9. Coding Rules

Mandatory:

- Prefer small, incremental changes.
- Use existing modules and patterns before adding abstractions.
- Do not rewrite working drivers without a specific reason.
- Keep HTTP responses small and ESP32-friendly.
- Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, or `ESP_LOGD` for firmware logs.
- Avoid new dependencies unless they clearly pay for themselves.
- Preserve the embedded dashboard build flow through `scripts/embed_frontend.py`.
- Keep CSV backward compatibility where practical.

If uncertain, first inspect the existing module and choose the least surprising
extension.

## 10. Approved Libraries

Currently used:

- M5Unified
- M5GFX
- M5Unit-ENV
- M5_DLight
- Wire
- Preferences
- LittleFS
- WiFi
- WebServer
- DNSServer

ArduinoJson is acceptable if JSON output grows enough that manual string
assembly becomes risky, but it is not required right now.

## 11. Scope Boundaries

Keep work focused on plant monitoring, environmental sensing, calibration,
local logging, local dashboard controls, and connectivity features that directly
support PlantLogger.

Avoid ML or cloud features until the local data pipeline has survived real
multi-day field trials.

## 12. Success Criteria

Successful next milestone:

- Runs continuously for 24-72 hours.
- Logs at the selected interval with no unexpected gaps.
- Produces named completed logs with graphable CSV rows.
- Calibration survives power cycles.
- Dashboard remains reachable during the run.
- Any sensor failure is visible rather than silent.

Longer-term success:

- Survives weeks unattended.
- Produces clean data suitable for analysis.
- Adds derived metrics without losing raw measurements.

## 13. TODO - Next Session

Field trial:

- Run a 24-72 hour logging session beside a real plant.
- Use a 60-300 second interval for realistic wear and data density.
- Open the completed log graphs and inspect timestamp gaps, unexpected zeros,
  calibration drift, dashboard reachability, and file rotation.
- Record observations in `docs/GETTING_STARTED.md` or a new field-trial note.

Clock and timestamps:

- Add a browser-to-device clock endpoint, for example `POST /api/time`.
- Store both elapsed `timestamp_ms` and wall-clock time in future CSV rows.
- Decide whether wall-clock output should be UTC ISO 8601 or local time with
  offset.

Status and reliability:

- Expand `/api/status` with uptime, current log slot, sample count, and
  last-read age.
- Add API/UI validation for interval bounds.
- Add visible warnings for stale readings and sensor failures.
- Add low-storage or filesystem-unavailable warnings.

Dashboard:

- Show "last read" age instead of only "Last sync".
- Improve active-logging feedback in the log table.

Data and analytics:

- Add session metadata such as session start time, sample count, and firmware
  version.
- Add VPD once temperature and humidity logs are proven stable.
- Add DLI once light sampling behavior is validated over a full day.
- Add watering-event detection only after several real soil-drying traces exist.

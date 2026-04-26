# PlantLogger — Project Context for Coding Agents
Version: 1.0  
Status: Source-of-truth document for AI coding agents

---

# 1. Project Overview

PlantLogger is a modular embedded monitoring system for plant health and environmental logging, built around the M5Stack AtomS3R.

Primary objective:

**Reliable sensing and data logging first, intelligence second.**

The system monitors:

- Air temperature
- Relative humidity
- Soil moisture
- Ambient light (lux)
- Atmospheric pressure (optional)

Future derived metrics:
- Vapor Pressure Deficit (VPD)
- Daily Light Integral (DLI)
- Watering event detection
- Soil drying trends
- Plant stress heuristics

Project priorities:

1. Reliability  
2. Clean data  
3. Modularity  
4. Low power  
5. Future analytics support

---

# 2. Hardware Platform

## Controller

## M5Stack AtomS3R

MCU:
ESP32-S3

Specs:
- Dual core 240 MHz
- 8MB Flash
- 8MB PSRAM
- WiFi
- 128x128 integrated display
- Built-in sensors may be considered later if they support the plant logging roadmap

Framework:
- PlatformIO
- Arduino framework
- C++17

---

# 3. Sensor Stack

## Ambient Light
Sensor:
BH1750 (M5Stack DLight)

Interface:
I2C

Address:
0x23

Range:
1–65535 lux

Sampling:
30 sec

Store:
1-minute aggregated values

Use:
- Light trends
- DLI (future)

---

## Environmental Sensor
M5Stack ENV III

Contains:

### SHT30

Address:
0x44

Measures:
- Temperature
- Humidity

Accuracy:
- ±0.2°C
- ±2% RH

Sampling:
30–60 sec

---

### QMP6988

Address:
0x70

Measures:
- Pressure

Sampling:
1–5 min

Optional for MVP.

---

## Soil Moisture

Sensor:
DFRobot SEN0193 Capacitive Moisture Sensor

Interface:
Analog ADC

Important:
NOT I2C.

Requirements:
- ADC read
- Dry/wet calibration
- Median burst filtering

Sampling:
60–300 sec

Acquisition:
7 samples preferred

Store:
- Raw ADC
- Calibrated %
- Calibration references

Never trust percentage without calibration.

---

# 4. Bus Architecture

## Shared I2C Bus

Devices:
- BH1750 → 0x23
- SHT30 → 0x44
- QMP6988 → 0x70

Bus speed:
400kHz preferred.

Check possible address conflicts.

---

## ADC Path

Dedicated analog input for soil sensor.

Filtering:
- Oversampling
- Median filter
- Optional moving average

---

# 5. Firmware Architecture

PlantLogger/

include/
- button.h

src/

drivers/
- button.cpp
- dlight_driver.cpp
- env_driver.cpp
- soil_driver.cpp

services/
- scheduler.cpp
- logger.cpp
- calibration.cpp

analytics/
- vpd.cpp
- watering_detector.cpp
- trend_analysis.cpp

storage/
- flash_store.cpp
- record_format.cpp
- mqtt_publish.cpp

ui/
- display_manager.cpp
- dashboard_screen.cpp

main.cpp

---

Architecture rule:

Drivers return raw measurements only.

Higher layers handle:
- Filtering
- Derived calculations
- Event logic

Strict separation.

---

# 6. Sampling Scheduler

Master scheduler:
60 second loop.

Cycle:

Every cycle:
- Soil burst read
- Lux read
- Temp/RH read

Every 5 cycles:
- Pressure read

Sequence:

t = 60 s

1 Soil burst median  
2 Light read  
3 Temp/RH read  
4 Optional pressure  
5 Package record  
6 Log record

Rule:
Never use delay()

Use millis()-based scheduling only.

---

# 7. Data Record Format

Each record stores:

timestamp_utc

Measurements:
- temp_c
- rh_pct
- pressure_pa
- lux
- soil_raw
- soil_pct

Derived:
- vpd_kpa
- watering_event

Metadata:
- firmware_version
- calibration_id
- pot_id
- sensor_ids

Example:

{
 "ts":"2026-04-26T18:30:00Z",
 "temp_c":24.1,
 "rh":57.2,
 "lux":18340,
 "soil_raw":2148,
 "soil_pct":42.7,
 "vpd":1.21,
 "pot_id":"monstera_01"
}

Rule:

Always store:
- raw values
- calibrated values
- derived values

Never derived only.

---

# 8. Development Roadmap

## Phase 1 MVP
- Stable sensor acquisition
- Local logging
- Calibration
- Serial debugging

## Phase 2
- Display dashboard
- Trend visualization
- Basic alerts

## Phase 3
- VPD
- DLI
- Watering detection

## Phase 4
- WiFi sync
- MQTT
- Multi-plant support

---

# 9. Coding Rules

Mandatory:

- Simplest implementation first
- Use library support before custom code
- Keep hardware constants centralized once sensor pins are chosen
- Never use delay()
- Use ESP_LOGI/W/E/D logging macros
- Avoid over-engineering
- Build incrementally
- Ask before adding abstractions

If uncertain:
Check existing library support first.

---

# 10. Approved Libraries

Allowed:

- M5Unified
- M5GFX
- M5Unit-ENV
- M5_DLight
- Wire
- ArduinoJson (optional)
- Preferences
- LittleFS

Avoid new dependencies unless justified.

---

# 11. Scope Boundaries

Keep code generation focused on plant monitoring, environmental sensing,
calibration, logging, display, and connectivity features that directly support
PlantLogger.

---

# 12. Optional Future Sensors

Possible additions:
- Soil temperature
- CO2
- IR leaf temperature
- Reservoir level
- Pot weight load cell

Architecture should stay extensible.

---

# 13. Success Criteria

Successful MVP:

- Runs continuously
- Logs reliably
- Produces clean data
- Survives weeks unattended
- Supports later AI analytics

Reliability over sophistication.

---

# 14. Agent Guardrails

Agents must:

- Never rewrite working drivers unnecessarily
- Prefer extending existing code over replacing it
- Avoid FreeRTOS multitasking unless justified
- Avoid premature optimization
- Avoid ML features before stable data pipeline exists

Default philosophy:

Simple > Clever  
Robust > Complex  
Maintainable > Fancy

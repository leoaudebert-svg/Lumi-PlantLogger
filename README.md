# PlantLogger

A plant monitoring and data-logging device built on the **M5Stack AtomS3R**.

PlantLogger reads soil moisture, ambient light, air temperature, humidity, and
atmospheric pressure at a configurable interval, displays live readings on the
built-in screen, and saves CSV logs to internal flash that you can download
over Wi-Fi from any phone or laptop.

---

## Features

| Feature | Status |
|---|---|
| Temperature & Humidity (SHT30) | ✅ MVP |
| Ambient Light / Lux (BH1750) | ✅ MVP |
| Soil Moisture — raw ADC + calibrated % | ✅ MVP |
| Atmospheric Pressure (QMP6988) | ✅ optional |
| On-device 128 × 128 live dashboard | ✅ MVP |
| Wi-Fi Access Point + captive-portal web UI | ✅ MVP |
| CSV log rotation (up to 5 files on LittleFS) | ✅ MVP |
| Soil moisture calibration via web UI | ✅ MVP |
| CSV download over HTTP | ✅ MVP |
| Physical button: short-press = read now, long-press = toggle logging | ✅ MVP |

---

## Hardware

| Component | Part | Interface |
|---|---|---|
| MCU | M5Stack AtomS3R (ESP32-S3) | — |
| Temp / Humidity | M5Stack ENV III — SHT30 | I2C 0x44 |
| Pressure | M5Stack ENV III — QMP6988 | I2C 0x70 |
| Ambient Light | M5Stack DLight — BH1750 | I2C 0x23 |
| Soil Moisture | DFRobot SEN0193 | ADC GPIO 6 |

All I2C sensors share the Grove/HY2.0-4P port (SDA=GPIO1, SCL=GPIO2, 400 kHz).
The soil sensor is wired to GPIO 6 (ADC1 channel; change `SOIL_ADC_PIN` in
`src/main.cpp` if a different pin is used).

---

## Quick Start

### Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB-C cable

### Build & Flash

```sh
# Build
pio run

# Upload to connected AtomS3R
pio run --target upload

# Open serial monitor (115200 baud)
pio device monitor
```

### First Use

1. Power on the device.  The display shows live sensor readings.
2. On your phone or laptop, connect to the **PlantLogger** Wi-Fi network
   (no password required).
3. A "Sign in to network" prompt should appear automatically — tap it, or
   open a browser and go to `http://192.168.4.1`.
4. Use the web dashboard to start/stop logging, trigger an immediate read,
   calibrate the soil sensor, and download CSV files.

See [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) for a detailed walkthrough.

---

## Documentation

| Document | Contents |
|---|---|
| [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) | Build, flash, first-use walkthrough |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Wiring, pin assignments, sensor specs |
| [docs/API.md](docs/API.md) | Web HTTP API endpoint reference |
| [docs/MODULES.md](docs/MODULES.md) | C++ module and driver API reference |
| [docs/DATA_FORMAT.md](docs/DATA_FORMAT.md) | CSV log format and field definitions |
| [docs/PROJECT_CONTEXT.md](docs/PROJECT_CONTEXT.md) | Architecture, roadmap, coding rules |

---

## Project Layout

```
include/          Public headers (button, dlight, env_driver, soil, logger, webserver)
src/
  drivers/        Hardware wrappers (button, dlight, env, soil)
  logger.cpp      LittleFS CSV session logger
  webserver.cpp   Wi-Fi AP, captive-portal DNS, HTTP dashboard & API
  main.cpp        Application entry point (setup / loop)
docs/             Project notes and documentation
platformio.ini    PlatformIO build configuration
```

---

## License

See repository root for license information.

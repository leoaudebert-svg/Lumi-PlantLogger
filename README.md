# PlantLogger

A plant monitoring and data-logging device built on the M5Stack AtomS3R.

PlantLogger reads soil moisture, ambient light, air temperature, humidity, and
atmospheric pressure at a configurable interval. It shows live readings on the
built-in screen, serves a local web dashboard over Wi-Fi, and saves CSV logs to
internal flash for in-browser graph review.

## Features

| Feature | Status |
|---|---|
| Temperature and humidity, SHT30 | MVP complete |
| Ambient light / lux, BH1750 | MVP complete |
| Soil moisture, raw ADC plus calibrated percent | MVP complete |
| Atmospheric pressure, QMP6988 | Optional sensor supported |
| On-device 128 x 128 live dashboard | MVP complete |
| Wi-Fi Access Point plus captive-portal web UI | MVP complete |
| CSV log rotation, up to 5 files on LittleFS | MVP complete |
| Soil moisture calibration via web UI | MVP complete |
| Named log sessions, storage bar, and per-log graphs | MVP complete |
| Physical button: short press reads now | MVP complete |

## Hardware

| Component | Part | Interface |
|---|---|---|
| MCU | M5Stack AtomS3R, ESP32-S3 | USB-C / Wi-Fi |
| Temperature / humidity | M5Stack ENV III, SHT30 | I2C `0x44` |
| Pressure | M5Stack ENV III, QMP6988 | I2C `0x70` |
| Ambient light | M5Stack DLight, BH1750 | I2C `0x23` |
| Soil moisture | DFRobot SEN0193 | ADC GPIO 6 |

All I2C sensors share the Grove/HY2.0-4P port. The firmware currently
initialises I2C with SDA on GPIO 2 and SCL on GPIO 1 at 400 kHz. The soil
sensor is wired to GPIO 6 by default; change `SOIL_ADC_PIN` in `src/main.cpp`
if a different ADC1 pin is used.

## Quick Start

### Requirements

- PlatformIO, either the VS Code extension or CLI
- USB-C cable

### Build And Flash

```sh
pio run
pio run --target upload
pio device monitor
```

### First Use

1. Power on the device. The display shows live sensor readings after boot.
2. On your phone or laptop, connect to the `PlantLogger` Wi-Fi network.
3. Tap the captive-portal prompt, or open `http://192.168.4.1`.
4. Use the dashboard to read sensors, start/stop logging, calibrate soil
   moisture, open log graphs, and clear stored logs.

See [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) for the full walkthrough.

## Documentation

| Document | Contents |
|---|---|
| [docs/START_HERE.md](docs/START_HERE.md) | Current project state and first field-trial checklist |
| [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) | Build, flash, and first-use walkthrough |
| [docs/HARDWARE.md](docs/HARDWARE.md) | Wiring, pin assignments, and sensor specs |
| [docs/API.md](docs/API.md) | Web HTTP API endpoint reference |
| [docs/MODULES.md](docs/MODULES.md) | C++ module and driver API reference |
| [docs/DATA_FORMAT.md](docs/DATA_FORMAT.md) | CSV log format and field definitions |
| [docs/PROJECT_CONTEXT.md](docs/PROJECT_CONTEXT.md) | Architecture, priorities, and next-session TODOs |

## Project Layout

```text
include/          Public headers
src/
  drivers/        Hardware wrappers
  logger.cpp      LittleFS CSV session logger
  webserver.cpp   Wi-Fi AP, captive portal, dashboard, and API
  main.cpp        Application entry point
Frontend/         Embedded dashboard source
scripts/          Build helpers, including frontend embedding
docs/             Project documentation
platformio.ini    PlatformIO build configuration
```

## TODO - Next Session

- Run a 24-72 hour field trial beside a real plant.
- Add browser-driven clock setting for wall-clock CSV timestamps.
- Expand `/api/status` with uptime, current slot, sample count, and last-read
  age.
- Add visible stale-data, sensor-failure, and low-storage warnings.
- Tighten log interval validation in the API and dashboard.

## License

See repository root for license information.

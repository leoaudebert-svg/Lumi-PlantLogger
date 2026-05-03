# PlantLogger Start Here

PlantLogger is now a working embedded plant-monitoring and CSV data logger for
the M5Stack AtomS3R. It reads temperature, humidity, pressure, ambient light,
and capacitive soil moisture, shows live readings on the built-in display,
serves a captive-portal web dashboard, and stores downloadable CSV log files on
LittleFS flash.

## Current Working Features

- M5Stack AtomS3R firmware built with PlatformIO and Arduino.
- M5Stack ENV III support for SHT30 temperature/humidity and QMP6988 pressure.
- M5Stack DLight support for BH1750 lux readings.
- DFRobot SEN0193 soil moisture readings on an ADC pin.
- Dry/wet soil calibration saved to NVS.
- 128 x 128 on-device live status display.
- Wi-Fi Access Point named `PlantLogger` at `192.168.4.1`.
- Captive portal redirects for common phone/laptop network checks.
- Single-page embedded dashboard from `Frontend/`.
- Start/stop logging from the dashboard or physical button.
- Immediate read from the dashboard or physical button.
- CSV logging to up to 5 rotating LittleFS files.
- Latest-file download and per-slot log downloads.
- Clear-all log and clear-calibration controls.

## First Thing To Do

Run a real field trial:

1. Build and flash the firmware.
2. Connect to the `PlantLogger` Wi-Fi network.
3. Open `http://192.168.4.1`.
4. Calibrate the soil sensor with dry and wet readings.
5. Start a 24-72 hour logging session beside an actual plant.
6. Download the CSV and inspect gaps, timestamps, calibration stability, and
   file rotation behavior.

This project has crossed the MVP line. The next work should be driven by what
the first real long-running logs reveal.

## Key Docs

- [GETTING_STARTED.md](GETTING_STARTED.md) - build, flash, and first use.
- [HARDWARE.md](HARDWARE.md) - wiring, pins, and sensor details.
- [API.md](API.md) - dashboard and HTTP endpoint reference.
- [DATA_FORMAT.md](DATA_FORMAT.md) - CSV naming and columns.
- [MODULES.md](MODULES.md) - firmware module API reference.
- [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) - architecture, priorities, and TODOs.

## Build

```sh
pio run
```

## Flash

```sh
pio run --target upload
```

## Monitor

```sh
pio device monitor
```

## TODO - Next Session

- Run the first 24-72 hour field trial beside a real plant.
- Add browser-driven clock setting for wall-clock CSV timestamps.
- Expand status metadata with uptime, free flash, current slot, sample count,
  and last-read age.
- Add dashboard mini charts and clearer stale-data warnings.
- Tighten interval validation and log any field-trial issues in the docs.

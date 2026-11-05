# PlantLogger

PlantLogger is a self-contained plant monitor and CSV logger for the
[M5Stack AtomS3R](https://shop.m5stack.com/products/atoms3r-dev-kit-w-0-85-inch-screen). It reads soil moisture, ambient light, temperature, humidity, and (when fitted) atmospheric pressure; hosts a local dashboard over Wi-Fi; and stores recent logging sessions on the device.

It is intended for hobby and prototyping use. It is not a safety-critical,
scientific, or irrigation-control system.

## What it does

- Reads an SHT30 temperature/humidity sensor, BH1750 light sensor, and a DFRobot SEN0193 capacitive soil sensor.
- Supports the QMP6988 pressure sensor included with M5Stack ENV III when present.
- Creates an open Wi-Fi access point named `PlantLogger` and presents a captive-portal dashboard at `http://192.168.4.1`.
- Lets you take an immediate reading, calibrate the soil sensor, start and stop named sessions, and graph completed sessions in the browser.
- Saves CSV data to LittleFS, retaining up to five completed sessions in round-robin order.
- Keeps soil calibration in NVS across reboots.

## Hardware

| Role | Supported part | Connection |
|---|---|---|
| Controller | M5Stack AtomS3R | USB-C / Wi-Fi |
| Temperature and humidity | M5Stack ENV III (SHT30) | I2C, `0x44` |
| Pressure (optional) | M5Stack ENV III (QMP6988) | I2C, `0x70` |
| Ambient light | M5Stack DLight (BH1750) | I2C, `0x23` |
| Soil moisture | DFRobot SEN0193 capacitive sensor | ADC GPIO 6 |

The I2C devices share the AtomS3R Grove/HY2.0-4P port: SDA is GPIO 2 and
SCL is GPIO 1. Use a compatible hub or splitter when connecting both M5Stack
modules. Wire the SEN0193's analog output to GPIO 6, VCC to 3.3 V, and GND to
GND. To use another ADC1 pin, change `SOIL_ADC_PIN` in
[`src/main.cpp`](src/main.cpp).

See the [hardware reference](docs/HARDWARE.md) for wiring and electrical
details.

## Quick start

### 1. Install prerequisites

- [PlatformIO](https://platformio.org/) CLI, or VS Code with the PlatformIO extension
- A USB-C data cable
- The hardware listed above

Clone the repository:

```sh
git clone <your-repository-url>
cd PlantLogger
```

### 2. Build and flash

PlatformIO downloads the declared Arduino libraries automatically on the first
build.

```sh
pio run
pio run --target upload
pio device monitor
```

The serial monitor runs at `115200` baud. If upload-port detection fails, pass
your device port explicitly, for example `--upload-port COM5` on Windows or
`--upload-port /dev/ttyACM0` on Linux.

### 3. Connect and calibrate

1. Power the AtomS3R and join the `PlantLogger` Wi-Fi network from a phone or computer.
2. Open the captive-portal prompt, or browse to `http://192.168.4.1`.
3. In **Soil calibration**, capture **Dry** with the sensor in open air, then capture **Wet** with its sensing area submerged to the safe fill line.
4. Choose a session name and interval, then start logging. A reading is recorded immediately and again at the selected interval.
5. Stop the session to make it available in the dashboard's graph view.

The physical button triggers an immediate sensor read. Logging sessions must be
started from the dashboard because each session needs a name.

## Data and storage

Sessions are CSV files with this header:

```csv
timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct
```

`timestamp_ms` is elapsed time since boot, not a wall-clock date or UTC
timestamp. After five completed sessions, starting another session overwrites
the oldest slot. Rows are flushed every 30 samples, so an unexpected power
loss can discard up to 29 recent rows.

For the full file format and ways to inspect a CSV, see
[data format](docs/DATA_FORMAT.md).

## Security, privacy, and limitations

- The access point is intentionally **open**: it has no Wi-Fi password, and the dashboard/API use unencrypted HTTP with no authentication. Use it only in a trusted, temporary local environment. Anyone within Wi-Fi range can view data, change calibration, start or stop logs, or clear them.
- PlantLogger does not use an internet connection or cloud service. Sensor logs remain in on-device flash unless someone retrieves them through the local dashboard/API.
- The device's display is an animated status avatar; use the browser dashboard for sensor values and graphs.
- Soil percentage is meaningful only after a two-point calibration for the installed sensor. It is an estimate, not a laboratory measurement.
- Sensor failures currently appear as zero values in logs/status responses. Check your wiring and validate readings before relying on a session.
- The current logging interval accepts positive whole seconds. Very short intervals can produce unnecessary flash writes; for longer trials, 60–300 seconds is a sensible starting point.

## HTTP API

The dashboard is built on a small local HTTP API. Useful endpoints include:

| Endpoint | Purpose |
|---|---|
| `GET /api/status` | Latest sensor values, calibration, and logging status |
| `GET /api/logs` | Completed log metadata and storage usage |
| `GET /api/log-data?slot=0` | CSV content for one completed slot |
| `POST /read-now` | Request an immediate sensor read |
| `POST /start?name=...&interval=60` | Start a named session |
| `POST /stop` | Stop the active session |
| `POST /clear` | Permanently delete completed logs |

All endpoints are served from `http://192.168.4.1`. Read the complete
[API reference](docs/API.md) before automating it, especially the destructive
`POST /clear` endpoint.

## Repository layout

```text
Frontend/         Source dashboard embedded into the firmware at build time
include/          Public module headers
scripts/          Build helper for embedding the frontend
src/drivers/      Sensor and button wrappers
src/logger.cpp    LittleFS-backed CSV session logger
src/webserver.cpp Wi-Fi access point, captive portal, dashboard, and API
docs/             Setup, hardware, API, data format, and module references
platformio.ini    PlatformIO environment and dependencies
```

## Documentation

- [Getting started](docs/GETTING_STARTED.md) — detailed build, flash, calibration, and field-trial walkthrough
- [Hardware reference](docs/HARDWARE.md) — pin assignments, wiring, and sensor specifications
- [API reference](docs/API.md) — HTTP endpoints, validation, and examples
- [Data format](docs/DATA_FORMAT.md) — CSV rotation, columns, and import notes
- [Module reference](docs/MODULES.md) — firmware interfaces and extension points

## Project status

This is a working hardware MVP. It has not yet completed the recommended
24–72 hour real-plant field trial. Before using it for unattended monitoring,
validate the chosen wiring, calibration, sensor stability, log rotation, and
power behavior in your own environment.

## Contributing

Issues and pull requests are welcome. Keep changes focused, avoid adding
dependencies unless a committed feature needs them, and verify the firmware
build before opening a pull request:

```sh
pio run
```

## License

<<<<<<< Updated upstream
This repository does not currently include a license file. Publishing source
without a license does **not** grant others permission to reuse, modify, or
distribute it. Choose and add a license before accepting external contributions
or inviting reuse.
=======
<<<<<<< Updated upstream
See repository root for license information.
=======
PlantLogger is licensed under the **GNU General Public License v3.0 only**
(GPL-3.0-only).

You may use, modify, and distribute this project. If you distribute a modified
version—such as firmware in a device—you must provide the corresponding source
under GPLv3 as well. Private modifications that are never distributed do not
have to be published.

See [LICENSE](LICENSE) for the complete terms.
>>>>>>> Stashed changes
>>>>>>> Stashed changes

# Getting Started with PlantLogger

This guide walks through building the firmware, flashing the M5Stack AtomS3R,
and using the PlantLogger dashboard for the first time.

## Prerequisites

### Software

| Tool | Purpose | Install |
|---|---|---|
| PlatformIO | Build system and uploader | VS Code extension or `pip install platformio` |
| VS Code | Optional IDE | `https://code.visualstudio.com` |

### Hardware

- M5Stack AtomS3R
- M5Stack ENV III, with SHT30 and QMP6988
- M5Stack DLight, with BH1750
- DFRobot SEN0193 capacitive soil moisture sensor
- HY2.0-4P / Grove cable or splitter
- USB-C cable

See [HARDWARE.md](HARDWARE.md) for wiring details.

## 1. Clone And Open

```sh
git clone <repo-url>
cd PlantLogger
```

Open the folder in VS Code with the PlatformIO extension, or use the PlatformIO
CLI directly.

## 2. Build

```sh
pio run
```

The first build downloads the required libraries from the PlatformIO registry.
Subsequent builds are faster because the libraries are cached.

## 3. Flash

Connect the AtomS3R over USB-C, then run:

```sh
pio run --target upload
```

If PlatformIO cannot auto-detect the port, specify it:

```sh
pio run --target upload --upload-port /dev/ttyACM0
```

On first upload, the device may need bootloader mode. Hold the front button
while plugging in USB if upload detection fails.

## 4. Serial Monitor

Open the serial monitor to see boot logs and live sensor output:

```sh
pio device monitor
```

Baud rate is `115200`. You should see logs like:

```text
I (...) env: SHT30 initialized (addr 0x44)
I (...) env: QMP6988 initialized (addr 0x70)
I (...) dlight: BH1750 initialised at 0x23
I (...) soil: SEN0193 initialised on GPIO 6
I (...) logger: LittleFS ready - next slot: 0
I (...) webserver: AP started - SSID: PlantLogger  IP: 192.168.4.1
I (...) webserver: DNS server started (wildcard -> 192.168.4.1)
I (...) webserver: HTTP server started on port 80
I (...) main: PlantLogger started.
```

## 5. Connect To The Web Dashboard

1. On your phone, tablet, or laptop, connect to the `PlantLogger` Wi-Fi network.
2. A captive-portal "Sign in to network" prompt should appear automatically.
   Tap it, or open a browser and go to `http://192.168.4.1`.
3. The dashboard shows live sensor values, logging controls, calibration
   controls, and a table of completed log files.

## 6. Calibrate The Soil Sensor

Raw ADC values from the SEN0193 depend on the exact sensor, supply voltage, and
wiring. Calibrate before trusting soil percentage values.

One-time calibration:

1. Open the web dashboard and find **Soil calibration**.
2. Hold the sensor in open air, not touching soil or water.
3. Click **Capture dry** and wait for the confirmation message.
4. Submerge the sensor tip in water up to the safe fill line.
5. Click **Capture wet** and wait for the confirmation message.

Once both points are captured, calibration is saved to NVS and survives power
cycles. The dashboard and display will then show soil moisture as a percentage
instead of only raw ADC values.

Repeat calibration when swapping sensors or when readings drift.

## 7. Start Logging

1. Set **Log interval** to the desired sampling period in seconds.
2. Click **Start logging**. The display shows `Log: ON`.
3. The device immediately records a first reading, then records at the selected
   interval.
4. Click **Stop logging** when finished. The display shows `Log: OFF`.
5. Download the latest CSV or a specific completed log from the log table.

Physical button shortcuts:

| Action | Result |
|---|---|
| Short press | Trigger one immediate sensor read |
| Long press | Toggle logging on or off |

## 8. Download And Clear Logs

- **Download latest** streams the most recently completed session as
  `plantlogger.csv`.
- The **Available logs** table lists up to 5 completed slots and downloads each
  specific slot as `log_N.csv`.
- **Clear logs** permanently deletes every log file and resets the rotation
  counter.

Downloads and clearing are disabled while a logging session is active.

Up to 5 log files are kept at once. When a sixth session starts, the oldest slot
is overwritten automatically using round-robin rotation.

## 9. Recommended Field Trial

After the first successful flash, run a real 24-72 hour logging session:

1. Put PlantLogger beside an actual plant.
2. Use a 60-300 second interval for a long run, or 60 seconds for a dense test.
3. Download the CSV at the end.
4. Check for timestamp gaps, unexpected zeros, calibration drift, and whether
   the dashboard remains reachable.

## Next Reading

- [HARDWARE.md](HARDWARE.md) for pin assignments and sensor specs.
- [API.md](API.md) for HTTP automation.
- [DATA_FORMAT.md](DATA_FORMAT.md) for CSV processing.
- [MODULES.md](MODULES.md) for firmware extension points.
- [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) for architecture notes and TODOs.

## TODO - Next Session

- Add browser-driven clock setting for wall-clock CSV timestamps.
- Add `/api/status` fields for uptime, free flash, current slot, sample count,
  and last-read age.
- Add dashboard mini charts and stale-data warnings.
- Validate logging intervals more strictly in both firmware and frontend.
- Capture the results of the first 24-72 hour field trial in the docs.

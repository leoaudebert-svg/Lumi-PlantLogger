# Getting Started with PlantLogger

This guide walks you through building the firmware, flashing your M5Stack
AtomS3R, and using PlantLogger for the first time.

---

## Prerequisites

### Software

| Tool | Purpose | Install |
|---|---|---|
| [PlatformIO](https://platformio.org/) | Build system & uploader | VS Code extension or `pip install platformio` |
| VS Code (optional) | IDE with PlatformIO extension | [code.visualstudio.com](https://code.visualstudio.com) |

### Hardware

- M5Stack AtomS3R
- M5Stack ENV III (SHT30 + QMP6988)
- M5Stack DLight (BH1750)
- DFRobot SEN0193 capacitive soil moisture sensor
- HY2.0-4P (Grove) cable(s)
- USB-C cable

See [HARDWARE.md](HARDWARE.md) for detailed wiring instructions.

---

## 1. Clone and Open

```sh
git clone <repo-url>
cd PlantLogger
```

Open in VS Code and let the PlatformIO extension install automatically, **or**
use the PlatformIO CLI directly.

---

## 2. Build

```sh
pio run
```

The first build downloads the required libraries (`M5Unified`, `M5GFX`,
`M5_DLight`, `M5Unit-ENV`) from the PlatformIO registry.  Subsequent builds
are fast because the libraries are cached.

Expected output (abridged):
```
Building .pio/build/m5stack-atoms3/firmware.bin
RAM: [=         ]  10.6% (used 34736 bytes from 327680 bytes)
Flash: [===       ]  31.4% (used 1032384 bytes from 3276800 bytes)
========================= [SUCCESS] ...
```

---

## 3. Flash

Connect the AtomS3R with a USB-C cable, then:

```sh
pio run --target upload
```

If PlatformIO cannot auto-detect the port, specify it:

```sh
pio run --target upload --upload-port /dev/ttyACM0
```

On the first upload the device may need to be put into bootloader mode by
holding the button while plugging in USB.

---

## 4. Serial Monitor

After flashing, open the serial monitor to see boot logs and live sensor output:

```sh
pio device monitor
```

Baud rate is `115200`.  You will see output similar to:

```
I (432) env: SHT30 initialized (addr 0x44)
I (445) env: QMP6988 initialized (addr 0x70)
I (467) dlight: BH1750 initialised at 0x23, CONTINUOUSLY_H_RESOLUTION_MODE
I (501) soil: SEN0193 initialised on GPIO 6 (12-bit, ADC_11db)
I (510) logger: LittleFS ready — next slot: 0
I (820) main: Read: 23.4 C  55.2 %  8400 lx  soil raw=2210
I (1540) webserver: AP started — SSID: PlantLogger  IP: 192.168.4.1
I (1542) webserver: DNS server started (wildcard -> 192.168.4.1)
I (1543) webserver: HTTP server started on port 80
I (1544) main: PlantLogger started.
```

---

## 5. Connect to the Web Dashboard

1. On your phone, tablet, or laptop, open Wi-Fi settings and connect to
   **PlantLogger** (open network, no password).
2. A captive-portal notification ("Sign in to network") should appear
   automatically.  Tap it — or open any browser and navigate to
   **`http://192.168.4.1`**.
3. The dashboard shows live sensor values and logging controls.

---

## 6. Calibrate the Soil Sensor

Raw ADC values from the SEN0193 are not directly meaningful without knowing the
dry and wet extremes for your specific sensor and supply voltage.

**One-time calibration procedure:**

1. Open the web dashboard → scroll to **Soil Calibration**.
2. Hold the sensor in open air (not touching anything).
3. Click **Capture Dry** and wait for the confirmation message.
4. Submerge the sensor tip in a glass of water (up to the fill line).
5. Click **Capture Wet** and wait for the confirmation message.

Once both points are captured the calibration is saved to NVS and survives
power cycles.  The dashboard and display will now show soil moisture as a
percentage (`S: 42 %`) instead of a raw value (`S: raw=2210`).

Repeat the procedure any time you swap sensors or notice drift.

---

## 7. Start Logging

1. Set the **Log interval (s)** field to your desired sampling rate (default 60 s).
2. Click **Start Logging**.  The display shows `Log: ON` in green.
3. The device reads all sensors at every interval and appends a row to the
   current CSV file on internal flash.
4. Click **Stop Logging** when finished.  The display shows `Log: OFF` in red.
5. Click **Download CSV** to save the file to your device.

**Physical button shortcuts:**

| Action | Result |
|---|---|
| Short press (click) | Trigger one immediate sensor read |
| Long press (hold) | Toggle logging on / off |

---

## 8. Download and Clear Logs

- **Download CSV** — streams the most recently completed session as
  `plantlogger.csv`.  The button is disabled while a session is active.
- **Clear Log** — permanently deletes all log files from flash and resets the
  rotation counter.  Not reversible.

Up to **5 log files** are kept at once.  When a sixth session starts, the
oldest slot is overwritten automatically (round-robin rotation).

---

## Next Steps

- Read [HARDWARE.md](HARDWARE.md) to understand pin assignments.
- Read [API.md](API.md) to automate logging via HTTP.
- Read [DATA_FORMAT.md](DATA_FORMAT.md) to process your CSV files.
- Read [MODULES.md](MODULES.md) to extend the firmware.

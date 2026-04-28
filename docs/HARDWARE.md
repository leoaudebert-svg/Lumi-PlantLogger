# Hardware Reference

This document covers the hardware components used in PlantLogger, their
electrical connections to the M5Stack AtomS3R, and relevant specifications.

---

## Controller — M5Stack AtomS3R

| Parameter | Value |
|---|---|
| SoC | ESP32-S3 (dual-core Xtensa LX7, 240 MHz) |
| Flash | 8 MB (QIO OPI mode) |
| PSRAM | 8 MB |
| Display | 128 × 128 px IPS LCD |
| Wireless | Wi-Fi 802.11 b/g/n (2.4 GHz) |
| USB | USB-C, USB CDC on boot |
| GPIO connector | HY2.0-4P (Grove) port — SDA=GPIO1, SCL=GPIO2 |
| Button | Built-in front-face button (M5.BtnA) |

The I2C Grove port is shared by all three I2C sensors.  The bus runs at
**400 kHz** and is initialised in `dlight::init()` (which runs first in
`setup()`).

---

## I2C Sensor Map

| Sensor | Module | I2C Address | Bus Pins |
|---|---|---|---|
| SHT30 (temp / RH) | M5Stack ENV III | 0x44 | GPIO1 (SDA), GPIO2 (SCL) |
| QMP6988 (pressure) | M5Stack ENV III | 0x70 | GPIO1 (SDA), GPIO2 (SCL) |
| BH1750 (lux) | M5Stack DLight | 0x23 | GPIO1 (SDA), GPIO2 (SCL) |

No address conflicts exist.  All three sensors can coexist on the same Grove
port simultaneously.

---

## Analog Sensor — Soil Moisture

| Parameter | Value |
|---|---|
| Sensor | DFRobot SEN0193 capacitive soil moisture sensor |
| Interface | Analog voltage output |
| Supply voltage | 3.3 V |
| Output range | ~1.0 V (wet) – ~2.8 V (dry) |
| ADC pin | GPIO 6 (default; configurable via `SOIL_ADC_PIN` in `main.cpp`) |
| ADC resolution | 12-bit (0–4095) |
| ADC attenuation | ADC_11db (usable linear range ≈ 0.15–2.45 V) |
| Sampling | 7-sample burst, median selected |

> **Important:** The SEN0193 uses capacitive sensing.  Higher ADC values
> correspond to **drier** soil.  The calibration stores a `dry_raw` (high) and
> `wet_raw` (low) value; the percentage is linearly mapped between them.

### ADC Pin Selection

The ESP32-S3 ADC1 channels are on **GPIO 1–10**.  ADC2 shares pins with the
Wi-Fi radio and is unreliable when Wi-Fi is active; `soil::begin()` rejects
any pin outside GPIO 1–10.

GPIO 1 (SDA) and GPIO 2 (SCL) are already used by the I2C bus.  Choose any
remaining pin from GPIO 3–10 that is physically accessible on your board.
The default is **GPIO 6**.

To change the ADC pin, edit `SOIL_ADC_PIN` in `src/main.cpp`:

```cpp
static constexpr uint8_t SOIL_ADC_PIN = 6;  // change to your pin
```

---

## Wiring Summary

```
AtomS3R Grove Port ──┬── M5Stack ENV III (SHT30 + QMP6988)
                     └── M5Stack DLight (BH1750)

AtomS3R GPIO 6 ──── DFRobot SEN0193 (analog output)
AtomS3R 3.3 V  ──── DFRobot SEN0193 (VCC)
AtomS3R GND    ──── DFRobot SEN0193 (GND)
```

Both M5Stack Grove accessories connect to the same 4-pin HY2.0 port (or via a
hub/splitter).

---

## Sensor Specifications

### SHT30 (Temperature & Humidity)

| Parameter | Value |
|---|---|
| Temperature range | -40 – +125 °C |
| Temperature accuracy | ±0.2 °C (typ.) |
| Humidity range | 0 – 100 % RH |
| Humidity accuracy | ±2 % RH (typ.) |
| Interface | I2C |
| Supply voltage | 2.4 – 5.5 V |
| Sampling interval | 30–60 s recommended |

### QMP6988 (Atmospheric Pressure)

| Parameter | Value |
|---|---|
| Pressure range | 300 – 1100 hPa |
| Accuracy | ±1 hPa (absolute, typical) |
| Interface | I2C |
| Supply voltage | 1.71 – 3.6 V |
| Sampling interval | 60–300 s (optional sensor) |

### BH1750 (Ambient Light)

| Parameter | Value |
|---|---|
| Measurement range | 1 – 65535 lux |
| Resolution mode | CONTINUOUSLY_H_RESOLUTION_MODE (1 lux) |
| Interface | I2C |
| Supply voltage | 2.4 – 3.6 V |
| Sampling interval | 30 s recommended |

### DFRobot SEN0193 (Soil Moisture)

| Parameter | Value |
|---|---|
| Sensing method | Capacitive (corrosion-resistant) |
| Output | Analog voltage |
| Supply voltage | 3.3 – 5.5 V |
| Output at 3.3 V (dry) | ~2.8 V (ADC ≈ 3480) |
| Output at 3.3 V (wet) | ~1.0 V (ADC ≈ 1240) |
| Burst samples | 7 (median filtered) |

The exact dry/wet ADC values vary by unit and supply voltage.  Always perform
the two-point calibration procedure described in
[GETTING_STARTED.md](GETTING_STARTED.md#6-calibrate-the-soil-sensor).

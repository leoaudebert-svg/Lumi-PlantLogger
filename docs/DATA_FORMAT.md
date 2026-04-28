# Data Format Reference

PlantLogger records sensor data as CSV files stored on LittleFS flash.
This document describes the file naming scheme, CSV column definitions,
and notes on interpreting the values.

---

## File Naming

Log files are stored at the root of the LittleFS partition:

```
/log_0.csv
/log_1.csv
/log_2.csv
/log_3.csv
/log_4.csv
```

Up to **5 files** are kept using round-robin rotation.  When a sixth session
starts, the oldest slot is overwritten.  The current write-slot pointer is
persisted in `/log_index` and restored on every boot, so rotation survives
power cycles.

---

## CSV Format

Each file opens with a header row followed by one data row per sensor read.

### Header Row

```
timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct
```

### Data Row Example

```
125430,23.4,55.2,1013.2,8400,2210,42
```

---

## Column Definitions

| Column | Unit | Source | Notes |
|---|---|---|---|
| `timestamp_ms` | milliseconds | `millis()` | Time since last boot; resets to 0 on every reboot.  Not a wall-clock UTC timestamp. |
| `temp_c` | °C | SHT30 | Raw sensor value, no post-processing.  `0.0` if the SHT30 read failed. |
| `rh_pct` | % RH | SHT30 | Relative humidity.  `0.0` if the SHT30 read failed. |
| `pressure_hpa` | hPa | QMP6988 | Atmospheric pressure divided by 100 from the raw Pa reading.  `0.0` if the QMP6988 is unavailable. |
| `lux` | lux | BH1750 | Raw ambient light value (1–65535 lux).  `0` if the sensor is unavailable. |
| `soil_raw` | ADC counts | SEN0193 | Median-filtered 12-bit ADC reading (0–4095).  Higher values = drier soil. |
| `soil_pct` | % | SEN0193 | Calibrated moisture percentage (0–100).  Only meaningful when calibration has been performed; see note below. |

### `soil_pct` Validity

The `soil_pct` column is written to every row regardless of calibration status.
When no valid calibration is stored, `soil_pct` is `0`.  Always check the
`soil_calibrated` field in `/api/status` before processing the `soil_pct` column.

A post-processing script should treat `soil_pct = 0` rows with `soil_raw > 0`
as "not calibrated" rather than "bone dry".

---

## Flush Behaviour

Rows are flushed to flash every **30 rows** to reduce write wear, and
unconditionally when `logger::stop_session()` is called.  In the event of an
unexpected power loss, up to 29 rows may be lost, but all previously flushed
rows are intact.

---

## Importing into a Spreadsheet

The CSV uses `\r\n` line endings (Windows-style) for maximum compatibility.

### Microsoft Excel / LibreOffice Calc

1. Download `plantlogger.csv` from the web dashboard.
2. Open the file directly (both applications auto-detect the delimiter).
3. The `timestamp_ms` column can be converted to elapsed time:
   `=A2/1000` gives seconds since boot.

### Python (pandas)

```python
import pandas as pd

df = pd.read_csv("plantlogger.csv")
df["elapsed_s"] = df["timestamp_ms"] / 1000.0
print(df.head())
```

### Example Output

```
   timestamp_ms  temp_c  rh_pct  pressure_hpa   lux  soil_raw  soil_pct  elapsed_s
0         60432    23.4    55.2        1013.2  8400      2210      42.0       60.4
1        120611    23.6    54.8        1013.1  8520      2195      43.0      120.6
2        180789    23.5    55.0        1013.3  8300      2230      41.0      180.8
```

---

## Timestamp Note

`timestamp_ms` is `millis()` — milliseconds elapsed since the last boot.
It resets to 0 on every power cycle or reset.

If you need wall-clock timestamps, the recommended approach (Phase 4 roadmap)
is to add NTP synchronisation over Wi-Fi and log UTC ISO 8601 strings instead.
Until then, correlate sessions to real time manually using the download
time-stamp from your browser or file system.

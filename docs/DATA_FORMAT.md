# Data Format Reference

PlantLogger records sensor data as CSV files stored on LittleFS flash. This
document describes file naming, rotation, CSV columns, and interpretation notes.

## File Naming

Log files are stored at the root of the LittleFS partition:

```text
/log_0.csv
/log_1.csv
/log_2.csv
/log_3.csv
/log_4.csv
```

Up to 5 files are kept using round-robin rotation. When a sixth session starts,
the slot that would be written next is overwritten. The write-slot pointer is
persisted in `/log_index`, so rotation survives power cycles.

The dashboard can download:

- the latest completed session via `/download.csv`, named `plantlogger.csv` in
  the browser; or
- a specific completed slot via `/download.csv?slot=N`, named `log_N.csv`.

## CSV Format

Each file opens with a header row followed by one data row per sensor read.

### Header Row

```csv
timestamp_ms,temp_c,rh_pct,pressure_hpa,lux,soil_raw,soil_pct
```

### Data Row Example

```csv
125430,23.4,55.2,1013.2,8400,2210,42
```

## Column Definitions

| Column | Unit | Source | Notes |
|---|---|---|---|
| `timestamp_ms` | milliseconds | `millis()` | Time since last boot; resets to 0 on every reboot |
| `temp_c` | deg C | SHT30 | Raw sensor value, no post-processing; `0.0` if the read failed |
| `rh_pct` | percent RH | SHT30 | Relative humidity; `0.0` if the read failed |
| `pressure_hpa` | hPa | QMP6988 | Atmospheric pressure; `0.0` if unavailable |
| `lux` | lux | BH1750 | Raw ambient light value from 1-65535; `0` if unavailable |
| `soil_raw` | ADC counts | SEN0193 | Median-filtered 12-bit ADC reading from 0-4095; higher is drier |
| `soil_pct` | percent | SEN0193 | Calibrated moisture percentage from 0-100 |

## Soil Percentage Validity

The `soil_pct` column is written to every row. When no valid calibration is
stored, `soil_pct` is `0`. Always check the `soil_calibrated` field from
`/api/status` before treating `soil_pct` as a real moisture percentage.

A post-processing script should treat `soil_pct = 0` rows with `soil_raw > 0`
and `soil_calibrated = false` as "not calibrated", not necessarily "bone dry".

## Flush Behaviour

The CSV header is flushed immediately when a session starts. Data rows are
flushed to flash every 30 rows to reduce write wear, and all pending rows are
flushed when `logger::stop_session()` runs.

If power is lost unexpectedly, up to 29 unflushed rows may be lost. Previously
flushed rows remain intact.

## Importing Into A Spreadsheet

The CSV uses CRLF line endings for broad spreadsheet compatibility.

### Microsoft Excel / LibreOffice Calc

1. Download `plantlogger.csv` or a `log_N.csv` file from the dashboard.
2. Open the file directly.
3. Convert elapsed time with a formula such as `=A2/1000` for seconds since
   boot.

### Python With pandas

```python
import pandas as pd

df = pd.read_csv("plantlogger.csv")
df["elapsed_s"] = df["timestamp_ms"] / 1000.0
print(df.head())
```

Example output:

```text
   timestamp_ms  temp_c  rh_pct  pressure_hpa   lux  soil_raw  soil_pct  elapsed_s
0         60432    23.4    55.2        1013.2  8400      2210      42.0       60.4
1        120611    23.6    54.8        1013.1  8520      2195      43.0      120.6
2        180789    23.5    55.0        1013.3  8300      2230      41.0      180.8
```

## Timestamp Note

`timestamp_ms` is milliseconds elapsed since the last boot. It is not a
wall-clock UTC timestamp and resets to 0 on every power cycle or reset.

For the next version, the recommended path is browser-driven clock setting from
the dashboard. The browser already has local wall-clock time, and this avoids
depending on an internet-connected Wi-Fi network while PlantLogger is running
as its own access point.

## TODO - Next Session

- Add a wall-clock timestamp column, likely `timestamp_iso`, while preserving
  `timestamp_ms` for elapsed-time analysis.
- Decide whether clock values should be local time with offset or UTC ISO 8601.
- Add sample-count and session metadata so CSV files are easier to audit after
  multi-day runs.
- Document the first real field-trial CSV and any observed gaps or drift.

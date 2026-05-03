# Web API Reference

PlantLogger exposes a simple HTTP API on port 80 over its Wi-Fi Access Point
at `192.168.4.1`. All endpoints are available without authentication because
the device is intended for a local, temporary AP workflow.

## Base URL

```text
http://192.168.4.1
```

## Endpoints

### `GET /`

Returns the embedded single-page dashboard.

**Response:** `200 text/html`

### `GET /api/status`

Returns a JSON snapshot of the most recent sensor readings, logging state, and
soil calibration values.

**Response:** `200 application/json`

```json
{
  "temp_c": 23.4,
  "rh_pct": 55.2,
  "pressure_hpa": 1013.2,
  "lux": 8400,
  "soil_raw": 2210,
  "soil_pct": 42.0,
  "soil_calibrated": true,
  "logging": false,
  "download_available": true,
  "cal_dry": 3210,
  "cal_wet": 1240
}
```

| Field | Type | Description |
|---|---|---|
| `temp_c` | float | Temperature in deg C from SHT30; `0.0` if unavailable |
| `rh_pct` | float | Relative humidity in percent from SHT30; `0.0` if unavailable |
| `pressure_hpa` | float | Pressure in hPa from QMP6988; `0.0` if unavailable |
| `lux` | integer | Ambient light in lux from BH1750; `0` if unavailable |
| `soil_raw` | integer | Median-filtered raw 12-bit ADC reading |
| `soil_pct` | float | Calibrated moisture percentage from 0-100 |
| `soil_calibrated` | boolean | True when valid dry/wet calibration points are stored |
| `logging` | boolean | True while a logging session is active |
| `download_available` | boolean | True when a completed latest log exists and logging is stopped |
| `cal_dry` | integer | Stored dry calibration ADC value; `0` if not set |
| `cal_wet` | integer | Stored wet calibration ADC value; `0` if not set |

### `GET /api/logs`

Returns metadata for completed log files. Files are ordered newest-first when
the latest completed slot is known. The currently open log file is not listed
or downloadable while logging is active.

**Response:** `200 application/json`

```json
{
  "logs": [
    {
      "slot": 2,
      "filename": "log_2.csv",
      "path": "/log_2.csv",
      "size_bytes": 4280,
      "is_latest": true
    }
  ]
}
```

| Field | Type | Description |
|---|---|---|
| `slot` | integer | Log slot number, from 0 to 4 |
| `filename` | string | File basename suitable for display |
| `path` | string | LittleFS path used internally by the firmware |
| `size_bytes` | integer | File size in bytes |
| `is_latest` | boolean | True for the most recently completed session |

### `POST /read-now`

Requests one immediate out-of-schedule sensor read. The main loop consumes the
request, reads all sensors, updates the dashboard/display, and appends a row if
a logging session is open.

**Response:** `200 text/plain` with `OK`

### `POST /start[?interval=<seconds>]`

Starts a new logging session.

| Query parameter | Type | Default | Description |
|---|---|---|---|
| `interval` | integer seconds | 60 | Sensor sampling and logging interval |

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | `OK` | Session started |
| `409` | error message | A session is already active; stop it first |

Notes:

- The interval is applied immediately and persists until `POST /stop` is called
  or the device reboots.
- The first logged read is triggered immediately after the session opens.

### `POST /stop`

Stops the active logging session, flushes any buffered rows, and closes the CSV
file. After this call the completed file is available for download.

**Response:** `200 text/plain` with `OK`

### `GET /download.csv[?slot=<slot>]`

Streams a completed log file as a CSV attachment. Without a query parameter,
the endpoint downloads the most recently completed log as `plantlogger.csv`.
With `slot`, it downloads that specific completed slot using its real filename,
such as `log_2.csv`.

| Query parameter | Type | Default | Description |
|---|---|---|---|
| `slot` | integer, 0-4 | latest completed file | Specific log slot to download |

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | CSV data | File streamed with a `Content-Disposition` attachment header |
| `400` | `Invalid log slot` | `slot` was not a valid 0-4 integer |
| `404` | `No log file available` | No matching completed file exists, or files were cleared |
| `409` | `Stop logging before downloading` | A session is currently active |
| `500` | error message | Filesystem error opening the file |

### `POST /clear`

Deletes all log files from flash and resets the rotation counter. This operation
is permanent and cannot be undone.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | `OK` | All files deleted |
| `409` | `Stop logging before clearing` | A session is currently active |

### `POST /calibrate-dry`

Captures the current median ADC reading as the dry calibration point. Hold the
sensor in open air before calling this endpoint. If both dry and wet points are
available in the correct order, calibration is committed to NVS immediately.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | confirmation message with `raw=<value>` | Point recorded |
| `503` | `Soil sensor not initialised` | `soil::begin()` failed |

### `POST /calibrate-wet`

Captures the current median ADC reading as the wet calibration point. Submerge
the sensor tip in water before calling this endpoint. If both dry and wet
points are available in the correct order, calibration is committed to NVS
immediately.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | confirmation message with `raw=<value>` | Point recorded |
| `503` | `Soil sensor not initialised` | `soil::begin()` failed |

### `POST /calibrate-clear`

Erases both calibration points from NVS. The `soil_calibrated` field in
`/api/status` returns `false` after this call.

**Response:** `200 text/plain` with `Calibration cleared`

## Captive Portal Endpoints

The following paths redirect to `http://192.168.4.1/` to trigger common
operating systems' captive-portal prompts:

| Path | OS |
|---|---|
| `/hotspot-detect.html` | iOS / macOS |
| `/library/test/success.html` | macOS alternate |
| `/generate_204` | Android |
| `/gen_204` | Android alternate |
| `/connecttest.txt` | Windows |
| `/ncsi.txt` | Windows alternate |

Any other unrecognised path also redirects to the dashboard.

## Example - Automate Logging from a Script

```sh
BASE="http://192.168.4.1"

# Start a 30-second logging session
curl -X POST "$BASE/start?interval=30"

# Read current status
curl "$BASE/api/status"

# Stop after desired time
curl -X POST "$BASE/stop"

# Download the latest CSV
curl -o plantlogger.csv "$BASE/download.csv"

# List completed log slots
curl "$BASE/api/logs"

# Download a specific slot
curl -o log_2.csv "$BASE/download.csv?slot=2"
```

## TODO - Next Session

- Add browser-driven clock setting so CSV rows can include real wall-clock
  timestamps as well as `millis()`.
- Expand `/api/status` with uptime, free flash, current log slot, sample count,
  and last-read age.
- Add dashboard mini charts for soil moisture, lux, temperature, and humidity.
- Add clearer stale-data and sensor-failure warnings to the dashboard.
- Tighten interval validation to reject zero, huge, or accidental rapid-start
  values at both the API and UI layers.
- Run and document a 24-72 hour field trial beside a real plant.

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

### `GET /graphs?slot=<slot>`

Returns the same embedded dashboard shell, opened in graph-view mode for a
completed log slot.

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
| `cal_dry` | integer | Stored dry calibration ADC value; `0` if not set |
| `cal_wet` | integer | Stored wet calibration ADC value; `0` if not set |

### `GET /api/logs`

Returns metadata for completed log files. Files are ordered newest-first when
the latest completed slot is known. The currently open log file is not listed
while logging is active.

**Response:** `200 application/json`

```json
{
  "logs": [
    {
      "slot": 2,
      "name": "Basil window",
      "filename": "log_2.csv",
      "path": "/log_2.csv",
      "size_bytes": 4280,
      "is_latest": true
    }
  ],
  "storage": {
    "total_bytes": 1441792,
    "used_bytes": 24576,
    "free_bytes": 1417216,
    "log_bytes": 4280
  }
}
```

| Field | Type | Description |
|---|---|---|
| `slot` | integer | Log slot number, from 0 to 4 |
| `name` | string | Customer-facing session name |
| `filename` | string | File basename suitable for display |
| `path` | string | LittleFS path used internally by the firmware |
| `size_bytes` | integer | File size in bytes |
| `is_latest` | boolean | True for the most recently completed session |
| `storage.total_bytes` | integer | Total LittleFS capacity in bytes |
| `storage.used_bytes` | integer | Used LittleFS bytes |
| `storage.free_bytes` | integer | Free LittleFS bytes |
| `storage.log_bytes` | integer | Bytes used by CSV log files |

### `GET /api/log-data?slot=<slot>`

Streams one completed CSV log inline for the dashboard graph view. This is not
served as a browser download.

| Query parameter | Type | Default | Description |
|---|---|---|---|
| `slot` | integer, 0-4 | required | Completed log slot to graph |

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | CSV data | File streamed as `text/csv` |
| `400` | `Invalid log slot` | `slot` was missing or invalid |
| `404` | `No log file available` | No matching completed file exists |
| `500` | error message | Filesystem error opening the file |

### `POST /read-now`

Requests one immediate out-of-schedule sensor read. The main loop consumes the
request, reads all sensors, updates the dashboard/display, and appends a row if
a logging session is open.

**Response:** `200 text/plain` with `OK`

### `POST /start?name=<log_name>[&interval=<seconds>]`

Starts a new logging session.

| Query parameter | Type | Default | Description |
|---|---|---|---|
| `name` | string | required | Customer-facing log name, unique among stored sessions |
| `interval` | integer seconds | 60 | Sensor sampling and logging interval |

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | `OK` | Session started |
| `400` | error message | Name is missing or invalid |
| `409` | error message | A session is active or the name already exists |

Notes:

- Names are trimmed, limited to 32 characters, and may use letters, numbers,
  spaces, `_`, `-`, and `.`.
- The interval is applied immediately and persists until `POST /stop` is called
  or the device reboots.
- The first logged read is triggered immediately after the session opens.

### `POST /stop`

Stops the active logging session, flushes any buffered rows, and closes the CSV
file. After this call the completed file appears in `/api/logs` and can be
opened in the dashboard graph view.

**Response:** `200 text/plain` with `OK`

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

# Start a named 30-second logging session
curl -X POST "$BASE/start?name=Basil%20window&interval=30"

# Read current status
curl "$BASE/api/status"

# Stop after desired time
curl -X POST "$BASE/stop"

# List completed logs and storage
curl "$BASE/api/logs"

# Read graph data for a specific slot
curl "$BASE/api/log-data?slot=2"
```

## TODO - Next Session

- Add browser-driven clock setting so CSV rows can include real wall-clock
  timestamps as well as `millis()`.
- Expand `/api/status` with uptime, current log slot, sample count, and
  last-read age.
- Add clearer stale-data and sensor-failure warnings to the dashboard.
- Tighten interval validation to reject zero, huge, or accidental rapid-start
  values at both the API and UI layers.
- Run and document a 24-72 hour field trial beside a real plant.

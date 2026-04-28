# Web API Reference

PlantLogger exposes a simple HTTP API on port 80 over its Wi-Fi Access Point
(`192.168.4.1`).  All endpoints are available without authentication.

---

## Base URL

```
http://192.168.4.1
```

---

## Endpoints

### `GET /`

Returns the HTML dashboard page.

**Response:** `200 text/html` — the full single-page dashboard.

---

### `GET /api/status`

Returns a JSON snapshot of the most recent sensor readings, logging state, and
calibration values.

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
| `temp_c` | float | Temperature in °C (SHT30); 0.0 if sensor unavailable |
| `rh_pct` | float | Relative humidity in % (SHT30); 0.0 if sensor unavailable |
| `pressure_hpa` | float | Pressure in hPa (QMP6988); 0.0 if sensor unavailable |
| `lux` | integer | Ambient light in lux (BH1750); 0 if sensor unavailable |
| `soil_raw` | integer | Median-filtered raw 12-bit ADC reading |
| `soil_pct` | float | Calibrated moisture percentage (0–100); only valid when `soil_calibrated` is true |
| `soil_calibrated` | boolean | True when valid dry/wet calibration points are stored |
| `logging` | boolean | True while a logging session is active |
| `download_available` | boolean | True when a completed log file is available and logging is stopped |
| `cal_dry` | integer | Stored dry calibration ADC value; 0 if not set |
| `cal_wet` | integer | Stored wet calibration ADC value; 0 if not set |

---

### `POST /read-now`

Requests an immediate out-of-schedule sensor read.  The next call to
`loop()` detects the request, reads all sensors, updates the display,
and (if a session is open) appends a row to the log file.

**Response:** `200 text/plain` — `OK`

---

### `POST /start[?interval=<seconds>]`

Starts a new logging session.

| Query parameter | Type | Default | Description |
|---|---|---|---|
| `interval` | integer (seconds) | 60 | Sensor sampling and logging interval |

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | `OK` | Session started |
| `409` | error message | A session is already active — stop it first |

**Notes:**
- The interval is applied immediately and persists until `POST /stop` is called
  or the device reboots.
- An immediate first read is triggered when the session opens.

---

### `POST /stop`

Stops the active logging session, flushes any buffered rows, and closes the
CSV file.  After this call the completed file is available for download.

**Response:** `200 text/plain` — `OK`

---

### `GET /download.csv`

Streams the most recently completed log file as a CSV attachment.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | CSV data | File streamed with `Content-Disposition: attachment; filename="plantlogger.csv"` |
| `404` | `No log file available` | No session has completed yet (or files were cleared) |
| `409` | `Stop logging before downloading` | A session is currently active |
| `500` | error message | Filesystem error opening the file |

---

### `POST /clear`

Deletes all log files from flash and resets the rotation counter.  This
operation is permanent and cannot be undone.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | `OK` | All files deleted |
| `409` | `Stop logging before clearing` | A session is currently active |

---

### `POST /calibrate-dry`

Captures the current median ADC reading as the **dry** calibration point.
Hold the sensor in open air before calling this endpoint.

If both a dry and a wet point are now available (in the correct order), the
calibration is committed to NVS immediately.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | confirmation message with `raw=<value>` | Point recorded |
| `503` | `Soil sensor not initialised` | `soil::begin()` failed |

---

### `POST /calibrate-wet`

Captures the current median ADC reading as the **wet** calibration point.
Submerge the sensor tip in water before calling this endpoint.

If both a dry and a wet point are now available (in the correct order), the
calibration is committed to NVS immediately.

**Responses:**

| Status | Body | Meaning |
|---|---|---|
| `200` | confirmation message with `raw=<value>` | Point recorded |
| `503` | `Soil sensor not initialised` | `soil::begin()` failed |

---

### `POST /calibrate-clear`

Erases both calibration points from NVS.  The `soil_calibrated` field in
`/api/status` will return `false` after this call.

**Response:** `200 text/plain` — `Calibration cleared`

---

## Captive Portal Endpoints

The following paths are intercepted and redirected to `http://192.168.4.1/`
to trigger the operating system's "Sign in to network" popup:

| Path | OS |
|---|---|
| `/hotspot-detect.html` | iOS / macOS |
| `/library/test/success.html` | macOS (alt) |
| `/generate_204` | Android |
| `/gen_204` | Android (alt) |
| `/connecttest.txt` | Windows |
| `/ncsi.txt` | Windows (alt) |

Any other unrecognised path also redirects to the dashboard.

---

## Example — Automate Logging from a Script

```sh
BASE="http://192.168.4.1"

# Start a 30-second logging session
curl -X POST "$BASE/start?interval=30"

# Read current status
curl "$BASE/api/status"

# Stop after desired time
curl -X POST "$BASE/stop"

# Download the CSV
curl -o plantlogger.csv "$BASE/download.csv"
```

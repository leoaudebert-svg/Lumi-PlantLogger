Made on 05/03/2026 with Gemini. 

Project Document: Digital Co-Pilot (Automated Pace Note System)
This document serves as the technical blueprint for the AtomS3R-based Digital Co-Pilot. The system uses high-speed inertial odometry and GPS to automatically detect, grade, and display cornering information for rally or performance driving.
------------------------------
1. Hardware Specification

### Controller: M5Stack AtomS3R (SKU: C126)
- **SoC**: ESP32-S3-PICO-1-N8R8 — Dual-core Xtensa LX7 @ up to 240 MHz, Hardware FPU
- **Flash**: 8MB (QIO mode)
- **PSRAM**: 8MB (OPI mode) — enabled via `-DBOARD_HAS_PSRAM` and `qio_opi` memory type
- **Connectivity**: 2.4 GHz Wi-Fi, USB OTG, USB Serial/JTAG
- **USB**: USB Type-C (power + firmware flashing)
- **Expansion**: HY2.0-4P port (GND / 5V / G2 / G1), 6x bottom GPIO (G5, G6, G7, G8, G38, G39)
- **Infrared**: Built-in IR LED (180° FOV, up to ~12.5m range) on GPIO47
- **Button**: Programmable user button on GPIO41
- **Dimensions**: 24.0 x 24.0 x 12.9 mm / 6.8g

### Display: 0.85" IPS LCD (GC9107 driver)
- **Resolution**: 128 x 128 pixels, color IPS
- **Interface**: SPI
- **Pin mapping**:
  - MOSI  → GPIO21
  - SCK   → GPIO15
  - CS    → GPIO14
  - RS    → GPIO42
  - RST   → GPIO48
  - BL    → LP5562_W (PWM backlight, recommended frequency: 500 Hz)

### IMU: Bosch BMI270 (6-axis)
- **Interface**: I2C, address `0x68`, shared SYS bus (SDA: GPIO45, SCL: GPIO0)
- **Accelerometer accuracy**: 0.05%
- **Gyroscope accuracy**: 0.05 °/s
- **Target sample rate for this project**: 200Hz+
- **Memory placement**: All fusion/EKF code using BMI270 data must use `IRAM_ATTR`

### Magnetometer: Bosch BMM150 (3-axis)
- **Interface**: Connected through BMI270 Sensor Hub auxiliary I2C (not directly to ESP32)
- **Access**: Read via BMI270 passthrough/auxiliary mode — do NOT attempt direct ESP32 I2C access
- **Accuracy**: 0.3 µT

For both sensors, make sure to follow what is done in the library in the folder .pio>libdeps>M5Unified>src>utility>imu>BMI270 or ./BMM150

### Memory Strategy
- **Internal SRAM (512KB)**: EKF core, gyroscope integration loop, real-time heading — use `IRAM_ATTR`
- **PSRAM (8MB)**: UI frame buffers, icon assets, telemetry logs — use `EXT_RAM_ATTR`

### GNSS: M5Stack Atomic GPS Base V2.0 (SKU: A134-V2)
- **Module**: ATGM336H-6N-74 @ AT6668 chip with built-in MAX2659 low-noise amplifier + ceramic antenna
- **Interface**: See below for fixed pin mapping to AtomS3R GPIO
- GPS feeds into Serial2: Serial2.begin(115200, SERIAL_8N1, G5, G6)

- **Supported satellite systems**: GPS/QZSS (L1), BDS B1/B1C, GALILEO (E1), GLONASS (L1)
- **Channels**: 50
- **Positioning accuracy**: < 1.5m CEP50
- **Output protocol**: NMEA 0183 v4.1
- **Default output rate**: 1Hz (configurable up to 10Hz)
- **Sensitivity**:
  - Tracking:       -167 dBm
  - Reacquisition:  -160 dBm
  - Cold start:     -148 dBm
  - Hot start:      -156 dBm
- **Startup times**: Cold: 23s / Hot: 1s / Reacquisition: 1s
- **Power consumption**: ~35mA operating, ~5mA standby

### GPS Usage Notes for This Project
- - **Pin mapping is fixed (GPS Bus → AtomS3R bottom GPIO)**:
  - UART_TX (GPS out) → G5 (ESP32 Serial RX)
  - UART_RX (GPS in)  → G6 (ESP32 Serial TX)
  - RST               → G7
  - 5V                → 5V
  - GND               → GND

- GPS runs on Core 1 alongside the UI task — never block Core 0 (EKF) waiting for GPS data
- At 1Hz default rate, consider bumping to 10Hz via CASIC proprietary command if more precision is needed.
- Library: `TinyGPSPlus` (already used in M5Stack's official examples for this module)
- Example sketch is available in .pio>libdeps>M5AtomS3>examples>AtomicBase>AtomicGPS>GPS>GPS.ino

### Available libraries for the project : ###
- M5Unified (for display, PSRAM management, and general ESP32 utilities)
- M5GFX (for advanced graphics on the IPS display)
- M5AtomS3 (for low-level control of the Atom S3/S3R hardware)
- TinyGPSPlus (for parsing NMEA sentences from the GPS module)
- BasicLinearAlgebra (for matrix operations in the EKF implementation)

------------------------------
2. Project Goal
To create an automated co-pilot that detects vehicle rotation in real-time, categorizes the severity of corners on a standard 1–6 rally scale, and provides a visual "Pace Note" icon on the AtomS3R display to confirm the driver's line and slip angle.
------------------------------
3. Technical Stack & Logic

* Fusion Method: Error-State Kalman Filter (ESKF). Focuses on tracking the drift of the sensors.
* Corner Grading Logic:
* Trigger: Detection of Yaw Rate (Gyro Z) exceeding a noise threshold (e.g., > 5°/sec).
   * Integration: Accumulate the change in heading until Yaw Rate returns to zero (the "exit").
   * Classification:
    - Grade 6 (Slight):      10–30°
    - Grade 5 (Easy):        30–50°
    - Grade 4 (Medium):      50–80°
    - Grade 3 (Square):      80–100°
    - Grade 2 (Tight):       100–150°
    - Grade 1 (Hairpin):     >150°
    - Plus and minus can be added to give nuance (e.g., 3+ for 90–100°, 3- for 80–90°)
   * Memory Management: Use EXT_RAM_ATTR for all UI assets to ensure 0% impact on the EKF calculation speed.

------------------------------
4. Implementation Milestones
Phase 1: High-Speed Inertial Pathing

* Initialize BMI270 at 200Hz.
* Implement a Madgwick or EKF filter to maintain a stable 3D orientation (Yaw/Pitch/Roll).
* Benchmark: Ensure the orientation update takes < 2ms in Internal RAM.

Phase 2: Corner Detection Algorithm

* Code a "Turn Monitor" that integrates Gyro Z-axis data during a turn.
* Implement a "Straight-Line Reset" to re-calibrate the compass when the GPS confirms a constant heading.
* Calculate Slip Angle: Compare IMU Heading vs. GPS Course-over-Ground to detect if the car is sliding.

Phase 3: PSRAM Asset Management & UI

* Load 12+ corner icons (Left 1-6, Right 1-6) into PSRAM.
* Develop the "Display Driver" task on the second ESP32 core to ensure UI updates don't "stutter" the sensor readings.
* Map the "Corner Grade" output to the corresponding PSRAM image.

Phase 4: Recce & Playback (Lookahead Pace Notes)

The corner detector is purely reactive — it confirms a corner as you drive through it. To display upcoming corners *before* reaching them, a two-run recce+playback system is required. This is how professional digital co-pilots work.

* **Recce mode**: Drive the stage once. The system records GPS waypoints alongside the auto-detected corner grade and direction → saves a pace note file to PSRAM / flash.
* **Stage mode**: On the actual run, `stage_manager.cpp` monitors current GPS position against the recorded waypoint list. When the distance to the next waypoint falls below a configurable trigger threshold (e.g. 150m), the corresponding pace note icon is displayed in advance.
* **Distance calculation**: Use a flat-earth approximation (sufficient for stage distances, runs in < 10µs on the ESP32-S3 FPU). Only the next waypoint in the list needs to be checked each GPS fix — no full-list scan needed.
* **GPS COG gate**: `course_deg` (Course-over-Ground) is the bearing of the GPS velocity vector between consecutive fixes. It is only reliable above ~5 km/h — gate all COG-dependent logic (slip angle, straight-line reset) on `speed_kmph > 5.0f`.
* **Waypoint storage**: Store waypoints as `(lat, lon, grade, direction)` structs
* `stage_manager.cpp` owns the waypoint list, the recce/stage mode state machine, and the distance-to-next-corner output consumed by the UI layer.

Phase 5: Field Tuning

* Tune "G-force rejection": Ensure the system doesn't confuse hard braking or bumps with cornering.
* Calibrate the BMM150 magnetometer to ignore the vehicle's metal frame (Hard-iron/Soft-iron calibration).

------------------------------

To ensure high maintainability and ease of navigation, a Layered Architecture is best. This decouples the "How to talk to the sensor" from the "How to calculate a corner."

Rally Copilot S3R/
├── .github/
│   ├── copilot-instructions.md
│   └── instructions/
├── data/
│   └── icons/
├── include/
│   ├── config.h
│   └── constants.h
├── src/
│   ├── drivers/
│   │   ├── bmi270_driver.cpp
│   │   ├── bmm150_driver.cpp
│   │   └── gps_parser.cpp
│   ├── fusion/
│   │   ├── ekf_core.cpp
│   │   ├── orientation.cpp
│   │   └── motion_stats.cpp
│   ├── logic/
│   │   ├── corner_detector.cpp
│   │   ├── slip_calculator.cpp
│   │   └── stage_manager.cpp
│   ├── ui/
│   │   ├── display_manager.cpp
│   │   └── screen_pace_notes.cpp
│   └── main.cpp
├── lib/
├── test/
├── .gitignore
├── platformio.ini
└── README.md

Logical Breakdown
drivers/ (The "Hands"): If you ever switch to a different IMU, you only change files in this folder. These files return "Raw" units (m/s², rad/s).
fusion/ (The "Brain"): This takes raw data and produces a clean "State." It doesn't care about rally corners; it only cares about "Where am I and which way am I pointing?" Keep these algorithms in Internal SRAM.
logic/ (The "Co-Pilot"): This is where the rally-specific magic happens. It consumes the "State" from the fusion layer and decides, "Hey, we just turned 90 degrees, that's a Square Right!"
ui/ (The "Voice"): This layer is strictly for the screen. It pulls icons from the PSRAM warehouse and shows them to the driver.

----------------------------------


## Coding Rules
- Never use delay(), always millis()
- EKF and fusion code must stay in Internal SRAM
- UI assets must use PSRAM only
- All pin definitions go in include/config.h, never hardcoded
- Use C++17
- For any feature, always use the simplest possible implementation that meets the requirements. Do not over-engineer and over complicate the codebase.
- Use the ESP32 built-in logging macros (`ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`) over USB Serial. Do not add an external logging library. Verbose logging is already enabled via `-DCORE_DEBUG_LEVEL=5` in `platformio.ini`.
- **Always use project wrapper functions (e.g., bmi270::, bmm150::) instead of calling M5Unified classes or methods directly, except for features not covered by a wrapper (e.g., button input).**

## Task Architecture
- Core 0: Sensor fusion task (IMU, EKF update)
- Core 1: UI task
- No blocking calls allowed on Core 0

## Current Status
- Phase 0: Libraries installed. 
- Do not assume any existing implementation. If in doubt, look for existing features in the Libraries. 
- If a feature is not available, do not write from scratch. Instead, make a comment to the user to ask if they want you to implement it.
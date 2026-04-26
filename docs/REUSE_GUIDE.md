# Reuse Guide for a New AtomS3R Project

This document explains the reusable parts of the current Rally Co-Pilot S3R
implementation. It is written for a new project that wants to keep the button,
display, page/navigation behavior, and optional image loading, but does not want
the BMI270, BMM150, GPS, or rally-specific logic.

## What to Copy

Copy these files directly:

```text
include/button.h
src/drivers/button.cpp
```

Optionally copy the image assets if the new project still needs full-screen PNGs:

```text
data/icons/
```

Use these files as references, but do not copy them wholesale unless you want
their test behavior:

```text
src/tests/test_display.cpp   - smallest display + button example
src/tests/test_nav.cpp       - page switching, page dots, SPIFFS image loading
src/tests/test_rotation.cpp  - simplest SPIFFS icon rendering example
platformio.ini               - board, filesystem, and library dependencies
```

Do not copy these if the new project will not use sensors or GPS:

```text
include/bmi270_driver.h
include/bmm150_driver.h
include/gps_parser.h
src/drivers/bmi270_driver.cpp
src/drivers/bmm150_driver.cpp
src/drivers/gps_parser.cpp
```

`include/config.h` is mostly GPS/IMU tuning in this repo. Only copy it if you
want a central place for your own pin and tuning constants.

## Required PlatformIO Setup

The transferable display/button stack depends on M5Stack libraries:

```ini
[env:m5stack-atoms3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
board_build.arduino.memory_type = qio_opi
board_build.filesystem = spiffs
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    m5stack/M5AtomS3@^1.0.2
    m5stack/M5Unified@^0.2.13
    m5stack/M5GFX@^0.2.19
```

Keep `board_build.filesystem = spiffs` if the new project loads images from
the `data/` folder.

## Global Initialization Pattern

Every sketch follows the same M5Unified startup pattern:

```cpp
#include <M5Unified.h>
#include "button.h"

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.fillScreen(TFT_BLACK);
}

void loop()
{
    M5.update();
    button::update();

    if (button::was_clicked()) {
        // React to button click.
    }

    vTaskDelay(1);
}
```

Important details:

- `M5.begin(cfg)` initializes the AtomS3R display and built-in M5 hardware.
- `M5.update()` must be called every loop before reading button state.
- `button::update()` is currently a no-op wrapper, but call it anyway so the
  application stays consistent if the driver grows later.
- Use `vTaskDelay(1)` instead of `delay()` to keep the ESP32 scheduler healthy.

## Button Driver

The button driver is a thin wrapper over `M5.BtnA`.

Reusable API:

```cpp
button::was_pressed();        // edge: button just pressed
button::was_released();       // edge: button just released
button::is_pressed();         // level: currently pressed
button::was_long_press();     // long hold event
button::was_clicked();        // short click event
button::was_double_clicked(); // double click event
button::is_holding();         // currently held
button::get_click_count();    // multi-click count
```

The current driver is hardwired to the AtomS3R front button:

```cpp
static m5::Button_Class* btn = &M5.BtnA;
```

If the new project uses a different M5 button, this is the only line that
should need to change.

## Display Basics

The screen is accessed through `M5.Display`, which is provided by M5Unified and
M5GFX. The AtomS3R display is 128x128.

Common calls used in this codebase:

```cpp
M5.Display.fillScreen(TFT_BLACK);
M5.Display.fillRect(x, y, w, h, color);
M5.Display.fillCircle(x, y, radius, color);
M5.Display.drawFastHLine(x, y, w, color);
M5.Display.drawFastVLine(x, y, h, color);
M5.Display.setTextSize(1);
M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
M5.Display.setCursor(0, 0);
M5.Display.println("Text");
M5.Display.printf("Value: %d\n", value);
M5.Display.drawString("DONE", 64, 64);
```

The simplest proof that the display and button work is in
`src/tests/test_display.cpp`: pressing the button fills the screen with a random
color.

## Page Navigation Pattern

`src/tests/test_nav.cpp` contains reusable page switching logic. The parts worth
keeping are the page constants, page state, page indicator dots, and the button
click page switch.

Minimal constants:

```cpp
static constexpr uint8_t PAGE_COUNT = 3;

static constexpr int16_t SCREEN_W   = 128;
static constexpr int16_t SCREEN_H   = 128;
static constexpr int16_t DOT_AREA_H = 10;
static constexpr int16_t CONTENT_H  = SCREEN_H - DOT_AREA_H;

static constexpr int16_t DOT_RADIUS  = 3;
static constexpr int16_t DOT_Y       = SCREEN_H - DOT_AREA_H / 2;
static constexpr int16_t DOT_SPACING = 14;
static constexpr int16_t DOT0_X      = SCREEN_W / 2 - DOT_SPACING;
static constexpr int16_t DOT1_X      = SCREEN_W / 2;
static constexpr int16_t DOT2_X      = SCREEN_W / 2 + DOT_SPACING;

static constexpr uint32_t DOT_HIDE_MS = 3000;
```

Minimal state:

```cpp
static uint8_t current_page  = 0;
static uint32_t page_enter_ms = 0;
static bool show_dots = true;
```

Reusable page indicator:

```cpp
static void draw_page_indicator(uint8_t active, bool visible)
{
    if (!visible) {
        return;
    }

    M5.Display.fillRect(0, CONTENT_H, SCREEN_W, DOT_AREA_H, TFT_BLACK);

    uint16_t c0 = (active == 0) ? TFT_WHITE : 0x4208;
    uint16_t c1 = (active == 1) ? TFT_WHITE : 0x4208;
    uint16_t c2 = (active == 2) ? TFT_WHITE : 0x4208;

    M5.Display.fillCircle(DOT0_X, DOT_Y, DOT_RADIUS, c0);
    M5.Display.fillCircle(DOT1_X, DOT_Y, DOT_RADIUS, c1);
    M5.Display.fillCircle(DOT2_X, DOT_Y, DOT_RADIUS, c2);
}
```

Reusable page switch:

```cpp
if (button::was_clicked()) {
    current_page = (current_page + 1) % PAGE_COUNT;
    page_enter_ms = millis();
    show_dots = true;
    render_current_page(show_dots, true);
}
```

Reusable auto-hide behavior:

```cpp
bool dots_now = (millis() - page_enter_ms < DOT_HIDE_MS);
if (dots_now != show_dots) {
    show_dots = dots_now;
    render_current_page(show_dots, true);
}
```

In the new project, implement your own `render_current_page(...)` and page draw
functions. Avoid copying the GPS, Ball, and Rotation page bodies unless you need
those exact features.

## How Images Are Stored

Images live in the project root `data/` folder. PlatformIO packs everything in
`data/` into a SPIFFS filesystem image.

Current structure:

```text
data/
+-- icons/
    +-- L1.png
    +-- L2.png
    +-- L3.png
    +-- L4.png
    +-- L5.png
    +-- L6.png
    +-- R1.png
    +-- R2.png
    +-- R3.png
    +-- R4.png
    +-- R5.png
    +-- R6.png
```

Runtime path rule:

```text
data/icons/L1.png  ->  "/icons/L1.png"
```

The `data/` prefix disappears at runtime because it is the root of the SPIFFS
image.

The current icons are 128x128 PNG files, matching the full AtomS3R display.
That allows a single call to draw the icon at `(0, 0)`.

## Uploading Images to the Device

Firmware upload and filesystem upload are separate steps.

Upload firmware:

```bash
pio run -e test_nav --target upload
```

Upload the `data/` folder to SPIFFS:

```bash
pio run -t uploadfs -e test_nav
```

Re-run `uploadfs` after adding, removing, renaming, or modifying files in
`data/`. You do not need to re-run `uploadfs` for normal code-only changes.

## Mounting SPIFFS

Any sketch that reads images from SPIFFS must include and mount SPIFFS:

```cpp
#include <SPIFFS.h>

static bool spiffs_ready = false;

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    spiffs_ready = SPIFFS.begin(true);
    if (!spiffs_ready) {
        ESP_LOGE("app", "SPIFFS mount failed.");
    }
}
```

`SPIFFS.begin(true)` formats the filesystem if mounting fails. That is useful
during development, but be aware it can erase files already stored in SPIFFS.

## Loading and Drawing PNG Images

The current implementation opens the PNG file from SPIFFS, reads the full file
into RAM, draws it with `M5.Display.drawPng(...)`, then frees the buffer.

Reusable function:

```cpp
static bool draw_png_from_spiffs(const char* path, int32_t x = 0, int32_t y = 0)
{
    File file = SPIFFS.open(path, "r");
    if (!file || file.size() == 0) {
        ESP_LOGE("app", "Image missing or empty: %s", path);
        if (file) file.close();
        return false;
    }

    const size_t len = file.size();
    uint8_t* buf = static_cast<uint8_t*>(malloc(len));
    if (!buf) {
        ESP_LOGE("app", "malloc(%u) failed for %s", (unsigned)len, path);
        file.close();
        return false;
    }

    file.read(buf, len);
    file.close();

    M5.Display.drawPng(buf, len, x, y);
    free(buf);
    return true;
}
```

Example:

```cpp
draw_png_from_spiffs("/icons/L1.png");
```

For tiny 128x128 PNGs this approach is simple and works well. For many large
images, consider caching, streaming, or storing fewer assets.

## Verifying Required Images

The current code verifies that every expected icon exists before relying on the
image UI.

Reusable pattern:

```cpp
static const char* const IMAGE_PATHS[] = {
    "/icons/L1.png",
    "/icons/L2.png",
    "/icons/L3.png",
};

static bool verify_image_files()
{
    for (size_t i = 0; i < sizeof(IMAGE_PATHS) / sizeof(IMAGE_PATHS[0]); ++i) {
        File file = SPIFFS.open(IMAGE_PATHS[i], "r");
        if (!file || file.size() == 0) {
            ESP_LOGE("app", "SPIFFS image missing or empty: %s", IMAGE_PATHS[i]);
            if (file) file.close();
            return false;
        }
        file.close();
    }

    return true;
}
```

Call this after `SPIFFS.begin(true)`.

## Minimal New Project Sketch

This sketch keeps button navigation, display drawing, and optional PNG loading.

```cpp
#include <M5Unified.h>
#include <SPIFFS.h>
#include "button.h"

static constexpr uint8_t PAGE_COUNT = 2;
static uint8_t current_page = 0;

static bool draw_png_from_spiffs(const char* path)
{
    File file = SPIFFS.open(path, "r");
    if (!file || file.size() == 0) {
        if (file) file.close();
        return false;
    }

    const size_t len = file.size();
    uint8_t* buf = static_cast<uint8_t*>(malloc(len));
    if (!buf) {
        file.close();
        return false;
    }

    file.read(buf, len);
    file.close();
    M5.Display.drawPng(buf, len, 0, 0);
    free(buf);
    return true;
}

static void draw_page_0()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("PAGE 0", 64, 64);
    M5.Display.setTextDatum(top_left);
}

static void draw_page_1()
{
    if (!draw_png_from_spiffs("/icons/L1.png")) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextColor(TFT_RED, TFT_BLACK);
        M5.Display.setCursor(0, 24);
        M5.Display.println("Missing image");
    }
}

static void render_current_page()
{
    switch (current_page) {
    case 0:
        draw_page_0();
        break;
    case 1:
        draw_page_1();
        break;
    }
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    SPIFFS.begin(true);
    render_current_page();
}

void loop()
{
    M5.update();
    button::update();

    if (button::was_clicked()) {
        current_page = (current_page + 1) % PAGE_COUNT;
        render_current_page();
    }

    vTaskDelay(1);
}
```

## Current Project Summary

The current repo is organized as a PlatformIO Arduino project for the M5Stack
AtomS3R. Production code in `src/main.cpp` is still mostly a placeholder.
Most real behavior currently lives in standalone test sketches under
`src/tests/`.

The parts that are already useful for a different non-rally project are:

- M5Unified startup and display access.
- The `button::` wrapper over `M5.BtnA`.
- Page switching with a single button.
- Optional page indicator dots.
- SPIFFS image storage through the root `data/` directory.
- PNG rendering with `M5.Display.drawPng(...)`.

The parts that are rally- or sensor-specific are:

- GPS parser and GPS screen.
- BMI270 accelerometer/gyro driver.
- BMM150 magnetometer driver.
- Tilt ball page.
- Steering-wheel rotation classifier.
- L1-R6 rally icon naming and grade thresholds.

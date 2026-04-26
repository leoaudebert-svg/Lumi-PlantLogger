#pragma once
#include <M5Unified.h>

// ---------------------------------------------------------------------------
// Button Driver — thin wrapper over M5.BtnA (M5Unified)
//
// Responsibilities:
//   - Encapsulate all button state queries (press, release, hold, click, etc.)
//   - Provide a single interface for main.cpp and other modules
//
// Usage contract:
//   1. Call button::update() in your main loop (after M5.update()).
//   2. Use button::was_pressed(), button::was_released(), button::is_pressed(), etc.
// ---------------------------------------------------------------------------

namespace button {
    // Call once per loop, after M5.update()
    void update();

    // Returns true if the button was pressed (rising edge)
    bool was_pressed();

    // Returns true if the button was released (falling edge)
    bool was_released();

    // Returns true if the button is currently held down
    bool is_pressed();

    // Returns true if the button was held for a long press
    bool was_long_press();

    // Returns true if the button was clicked (short press and release)
    bool was_clicked();

    // Returns true if the button was double clicked
    bool was_double_clicked();

    // Returns true if the button is currently being held (continuous)
    bool is_holding();

    // Returns the number of clicks detected in a multi-click sequence
    int get_click_count();
}

#include "button.h"

namespace button {
    // Internal: reference to the M5Unified button
    static m5::Button_Class* btn = &M5.BtnA;

    void update() {
        // No-op: M5.update() must be called in main loop before this
        // This function is here for API symmetry and future expansion
    }

    bool was_pressed() {
        return btn->wasPressed();
    }

    bool was_released() {
        return btn->wasReleased();
    }

    bool is_pressed() {
        return btn->isPressed();
    }

    bool was_long_press() {
        return btn->wasHold();
    }

    bool was_clicked() {
        return btn->wasClicked();
    }

    bool was_double_clicked() {
        return btn->wasDoubleClicked();
    }

    bool is_holding() {
        return btn->isHolding();
    }

    int get_click_count() {
        return btn->getClickCount();
    }
}

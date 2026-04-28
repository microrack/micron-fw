#include "button.h"

#include <Arduino.h>

#include "board.h"

namespace {
Button button;
}  // namespace

void Button::init() {
    pinMode(TOUCH_PIN, INPUT);

    const bool raw = digitalRead(TOUCH_PIN) == HIGH;
    last_raw_ = raw;
    stable_state_ = raw;
    last_raw_change_ms_ = millis();
    pressed_since_ms_ = raw ? millis() : 0;
    hold_reported_ = false;
}

bool Button::read_raw() const {
    return digitalRead(TOUCH_PIN) == HIGH;
}

ButtonEvent Button::handle() {
    const uint32_t now_ms = millis();
    const bool raw = digitalRead(TOUCH_PIN) == HIGH;

    if (raw != last_raw_) {
        last_raw_ = raw;
        last_raw_change_ms_ = now_ms;
    }

    if ((now_ms - last_raw_change_ms_) >= DEBOUNCE_MS && stable_state_ != last_raw_) {
        stable_state_ = last_raw_;
        if (stable_state_) {
            pressed_since_ms_ = now_ms;
            hold_reported_ = false;
            return ButtonEvent::Pressed;
        }

        pressed_since_ms_ = 0;
        hold_reported_ = false;
        return ButtonEvent::Idle;
    }

    if (stable_state_ && !hold_reported_ && (now_ms - pressed_since_ms_) >= HOLD_MS) {
        hold_reported_ = true;
        return ButtonEvent::Hold;
    }

    return ButtonEvent::Idle;
}

void button_init() {
    button.init();
}

bool button_read_raw() {
    return button.read_raw();
}

ButtonEvent button_handle() {
    return button.handle();
}

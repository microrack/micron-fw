#pragma once

#include <stdint.h>

enum class ButtonEvent {
    Idle = 0,
    Pressed,
    Hold,
};

class Button {
   public:
    void init();
    bool read_raw() const;
    ButtonEvent handle();

   private:
    static constexpr uint32_t DEBOUNCE_MS = 30;
    static constexpr uint32_t HOLD_MS = 700;

    bool last_raw_ = false;
    bool stable_state_ = false;
    uint32_t last_raw_change_ms_ = 0;
    uint32_t pressed_since_ms_ = 0;
    bool hold_reported_ = false;
};

void button_init();
bool button_read_raw();
ButtonEvent button_handle();

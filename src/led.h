#pragma once

#include <Arduino.h>

enum class LedMode : uint8_t {
    Off = 0,
    Boot = 1,
    Manual = 2,
    All = 3,
    Connecting = 4,
    Ap = 5,
    Connected = 6,
};

void init_led();
void handle_led();
void set_led_mode(LedMode mode);
void set_led(uint8_t led_index, bool state);

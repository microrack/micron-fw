#pragma once

#include <Arduino.h>

enum class LedMode : uint8_t {
    Boot = 0,
    Normal = 1,
    PreBoot = 2,
};

enum class LedNet : uint8_t {
    Connecting = 0,
    Ap = 1,
    Connected = 2,
};

void init_led();
void handle_led();
void set_led_mode(LedMode mode);
void set_led_net(LedNet net);
void set_led_gate(uint8_t idx, bool state);

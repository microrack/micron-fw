#include "board.h"

#include <Arduino.h>

#if __has_include("app_config.h")
#include "app_config.h"
#endif
#ifndef APP_HW_VERSION
#define APP_HW_VERSION "current"
#endif
#include <strings.h>

namespace {

const BoardPinsProfile* g_board_pins = nullptr;

}  // namespace

const BoardPinsProfile kBoardProfiles[] = {
    [] {
        BoardPinsProfile p = {};
        p.name = "legacy";
        p.board_led_pin = 18;
        p.board_led_count = 9;
        p.touch_pin = 14;
        p.gate_out_pins[0] = 33;
        p.gate_out_pins[1] = 34;
        p.gate_out_pins[2] = 35;
        p.gate_out_pins[3] = 36;
        p.clock_out_pin = 37;
        p.mcp4728_sda = 9;
        p.mcp4728_scl = 10;
        p.gate_led_indices[0] = 8;
        p.gate_led_indices[1] = 7;
        p.gate_led_indices[2] = 6;
        p.gate_led_indices[3] = 5;
        p.clock_led_index = 4;
        return p;
    }(),
    [] {
        BoardPinsProfile p = {};
        p.name = "current";
        p.board_led_pin = 37;
        p.board_led_count = 5;
        p.touch_pin = 36;
        p.gate_out_pins[0] = 14;
        p.gate_out_pins[1] = 17;
        p.gate_out_pins[2] = 18;
        p.gate_out_pins[3] = 21;
        p.clock_out_pin = 13;
        p.mcp4728_sda = 33;
        p.mcp4728_scl = 34;
        p.gate_led_indices[0] = 3;
        p.gate_led_indices[1] = 2;
        p.gate_led_indices[2] = 1;
        p.gate_led_indices[3] = 0;
        p.clock_led_index = 4;
        return p;
    }(),
};

const uint8_t kBoardProfilesCount =
    static_cast<uint8_t>(sizeof(kBoardProfiles) / sizeof(kBoardProfiles[0]));

void board_pins_init() {
    g_board_pins = &kBoardProfiles[1];
    const char* v = APP_HW_VERSION;
    if (v[0] == '\0') {
        return;
    }

    for (uint8_t i = 0; i < kBoardProfilesCount; ++i) {
        if (strcasecmp(v, kBoardProfiles[i].name) == 0) {
            g_board_pins = &kBoardProfiles[i];
            return;
        }
    }
}

const BoardPinsProfile* board_pins() {
    if (g_board_pins == nullptr) {
        g_board_pins = &kBoardProfiles[1];
    }
    return g_board_pins;
}

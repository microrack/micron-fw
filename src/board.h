#pragma once

#include <Arduino.h>

#define PROFILING 0

/** Maximum WS2812 count across profiles (FastLED buffer size). */
static constexpr uint16_t BOARD_LED_COUNT_MAX = 9;
static constexpr uint8_t BOARD_GATE_OUT_COUNT = 4;

/** Runtime pin map (filled from selected hardware profile). */
struct BoardPinsProfile {
    const char* name;
    uint8_t board_led_pin;
    uint16_t board_led_count;
    uint8_t touch_pin;
    uint8_t gate_out_pins[BOARD_GATE_OUT_COUNT];
    uint8_t clock_out_pin;
    int mcp4728_sda;
    int mcp4728_scl;
    uint8_t gate_led_indices[BOARD_GATE_OUT_COUNT];
    uint8_t clock_led_index;
};

/** Available pinout profiles (selected by config `hw_version`). */
extern const BoardPinsProfile kBoardProfiles[];
extern const uint8_t kBoardProfilesCount;

struct AppConfig;
void board_pins_init(const AppConfig& cfg);
const BoardPinsProfile* board_pins();

#pragma once

#include <Arduino.h>

#define PROFILING 0

static constexpr uint8_t BOARD_LED_PIN = 18;
static constexpr uint16_t BOARD_LED_COUNT = 9;
static constexpr uint8_t TOUCH_PIN = 14;

/** Digital gate outputs (active low). Indices 0..GATE_OUT_PIN_COUNT-1. */
static constexpr uint8_t GATE_OUT_PINS[] = {33, 34, 35, 36};
static constexpr uint8_t GATE_OUT_PIN_COUNT =
    static_cast<uint8_t>(sizeof(GATE_OUT_PINS) / sizeof(GATE_OUT_PINS[0]));
/** Dedicated digital clock output (active low). */
static constexpr uint8_t CLOCK_OUT_PIN = 37;

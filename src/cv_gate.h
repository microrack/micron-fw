#pragma once

#include <stdint.h>

#include "led.h"

void set_gate(uint8_t idx, LedGateColor color);

/** CV output in volts (0..5). Maps to MCP4728 channel. */
bool set_cv(uint8_t channel, float volts);

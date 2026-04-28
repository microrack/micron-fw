#pragma once

#include <stdint.h>

void init_cv_gate();
void set_gate(uint8_t idx, bool on);

/** CV output in volts (0..5). Updates MCP4728 channel 0..3; all four outputs refreshed. */
bool set_cv(uint8_t channel, float volts);

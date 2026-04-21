#pragma once

#include <stdint.h>

void init_mcp4728();

bool mcp4728_is_ready();

bool mcp4728_write_channel(uint8_t channel, uint16_t value12);

/** Fast Write: updates channels A.. sequentially; count in 1..4. */
bool mcp4728_write_all(uint8_t count, const uint16_t* values);

#pragma once

#include <stdint.h>

void init_mcp4728();

void mcp4728_write_channel(uint8_t channel, uint16_t value12);

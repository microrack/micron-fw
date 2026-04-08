#include "mcp4728.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr uint8_t MCP4728_I2C_ADDR = 0x60;
constexpr int MCP4728_SDA = 9;
constexpr int MCP4728_SCL = 10;

constexpr uint8_t MCP4728_MULTI_IR_CMD = 0x40;
}  // namespace

void init_mcp4728() {
    Wire.begin(MCP4728_SDA, MCP4728_SCL);
    Wire.setClock(400000);
    for (uint8_t ch = 0; ch < 4; ++ch) {
        mcp4728_write_channel(ch, 0);
    }
}

void mcp4728_write_channel(uint8_t channel, uint16_t value12) {
    if (channel > 3) {
        return;
    }
    value12 &= 0x0FFF;

    const uint8_t cmd = static_cast<uint8_t>(MCP4728_MULTI_IR_CMD | (channel << 1));
    const uint8_t hi = static_cast<uint8_t>(value12 >> 8);
    const uint8_t lo = static_cast<uint8_t>(value12 & 0xFF);

    Wire.beginTransmission(MCP4728_I2C_ADDR);
    Wire.write(cmd);
    Wire.write(hi);
    Wire.write(lo);
    Wire.endTransmission();
}

#include "mcp4728.h"

#include <Arduino.h>
#include <Wire.h>

namespace {

constexpr uint8_t  MCP4728_I2C_ADDR = 0x60;
constexpr int      MCP4728_SDA      = 9;
constexpr int      MCP4728_SCL      = 10;
constexpr uint32_t MCP4728_SCL_HZ   = 400000;

// Single Write command: C2 C1 C0 W1 W0 = 0 1 0 1 1
constexpr uint8_t MCP4728_SINGLE_WRITE_CMD = 0x58;

static uint8_t g_single_write_buf[4];
static uint8_t g_fast_write_buf[8];

}  // namespace

void init_mcp4728() {
    Wire.begin(MCP4728_SDA, MCP4728_SCL);
    Wire.setClock(MCP4728_SCL_HZ);

    const uint16_t zeros[4] = {0, 0, 0, 0};
    (void)mcp4728_write_all(4, zeros);
}

// Wire.endTransmission() blocks until transfer is done (via FreeRTOS semaphore
// in the legacy i2c driver). During the block the calling task is in BLOCKED
// state, so the CPU is free to run WiFi / other tasks.
// Always returns true — Wire is ready as soon as endTransmission() returns.
bool mcp4728_is_ready() {
    return true;
}

bool mcp4728_write_channel(uint8_t channel, uint16_t value12) {
    if (channel > 3) {
        return false;
    }

    value12 &= 0x0FFF;

    g_single_write_buf[0] = MCP4728_SINGLE_WRITE_CMD;
    g_single_write_buf[1] = static_cast<uint8_t>((channel << 6) | 0x00);
    g_single_write_buf[2] = static_cast<uint8_t>((value12 >> 8) & 0x0F);
    g_single_write_buf[3] = static_cast<uint8_t>(value12 & 0xFF);

    Wire.beginTransmission(MCP4728_I2C_ADDR);
    Wire.write(g_single_write_buf, sizeof(g_single_write_buf));
    return Wire.endTransmission() == 0;
}

bool mcp4728_write_all(uint8_t count, const uint16_t* values) {
    if (values == nullptr || count == 0) {
        return false;
    }
    if (count > 4) {
        count = 4;
    }

    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t v = static_cast<uint16_t>(values[i] & 0x0FFF);
        g_fast_write_buf[(2 * i)]     = static_cast<uint8_t>((v >> 8) & 0x0F);
        g_fast_write_buf[(2 * i) + 1] = static_cast<uint8_t>(v & 0xFF);
    }

    Wire.beginTransmission(MCP4728_I2C_ADDR);
    Wire.write(g_fast_write_buf, static_cast<size_t>(count) * 2U);
    return Wire.endTransmission() == 0;
}

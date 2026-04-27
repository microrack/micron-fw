#include "cv_gate.h"

#include "mcp4728.h"

namespace {

constexpr uint8_t kGateDacChannelCount = 4;
constexpr float   kCvVrefV           = 5.0f;

}  // namespace

void set_gate(uint8_t idx, LedGateColor color) {
    set_led_gate(idx, color);
    if (idx < kGateDacChannelCount) {
        const float v = (color != LedGateColor::Off) ? kCvVrefV : 0.0f;
        (void)set_cv(idx, v);
    }
}

bool set_cv(uint8_t channel, float volts) {
    if (volts < 0.0f) {
        volts = 0.0f;
    }
    if (volts > kCvVrefV) {
        volts = kCvVrefV;
    }
    // MCP4728: VOUT = VREF * code / 4096, code in 0..4095
    const float scaled = volts * (4096.0f / kCvVrefV);
    auto code = static_cast<uint32_t>(scaled + 0.5f);
    if (code > 4095U) {
        code = 4095U;
    }
    return mcp4728_write_channel(channel, static_cast<uint16_t>(code));
}

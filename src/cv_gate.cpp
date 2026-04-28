#include "cv_gate.h"

#include "board.h"
#include "mcp4728.h"

namespace {

constexpr uint8_t CV_CHANNEL_COUNT = 4;
constexpr float   CV_VREF_V        = 5.0f;

static uint16_t g_cv_codes[CV_CHANNEL_COUNT] = {};

}  // namespace

void init_cv_gate() {
    init_mcp4728();
    for (uint8_t i = 0; i < GATE_OUT_PIN_COUNT; ++i) {
        pinMode(GATE_OUT_PINS[i], OUTPUT);
        digitalWrite(GATE_OUT_PINS[i], HIGH);  // off: active-low gates idle high
    }
}

void set_gate(uint8_t idx, bool on) {
    if (idx < GATE_OUT_PIN_COUNT) {
        digitalWrite(GATE_OUT_PINS[idx], on ? LOW : HIGH);
    }
}

bool set_cv(uint8_t channel, float volts) {
    if (channel >= CV_CHANNEL_COUNT) {
        return false;
    }
    if (volts < 0.0f) {
        volts = 0.0f;
    }
    if (volts > CV_VREF_V) {
        volts = CV_VREF_V;
    }
    // MCP4728: VOUT = VREF * code / 4096, code in 0..4095
    const float scaled = volts * (4096.0f / CV_VREF_V);
    auto code = static_cast<uint32_t>(scaled + 0.5f);
    if (code > 4095U) {
        code = 4095U;
    }
    g_cv_codes[channel] = static_cast<uint16_t>(code & 0x0FFFU);
    return mcp4728_write_all(CV_CHANNEL_COUNT, g_cv_codes);
}

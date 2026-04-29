#pragma once

#include <stdint.h>

enum class CvGateMode : uint8_t {
    CvGate = 0,
    Synth = 1,
};

void init_cv_gate();
void set_cv_gate_mode(CvGateMode mode);
void set_gate(uint8_t idx, bool on);
void set_all_gates(bool on);
void set_clock(bool on);

/** CV output in volts (0..5). Writes channel value into DAC task buffer. */
bool set_cv(uint8_t channel, float volts);
void set_all_cv(float volts);
void reset_all_outputs();

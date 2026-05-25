#pragma once

#include <stdint.h>

enum class CvMode : uint8_t {
    Cv = 0,
    Synth = 1,
};

enum class GateMode : uint8_t {
    Gate = 0,
    Osc = 1,
};

void init_cv_gate();
void set_cv_mode(CvMode mode);
void set_gate_mode(GateMode mode);
void set_cv_synth_note(uint8_t channel, bool on);
/** Synth voice pitch in Hz for the given CV channel. */
bool set_cv_synth_freq(uint8_t channel, float frequency_hz);
void set_gate(uint8_t idx, bool on);
void set_all_gates(bool on);
void set_clock(bool on);

/** CV output in volts (0..5). Writes channel value into DAC task buffer. */
bool set_cv(uint8_t channel, float volts);
void set_all_cv(float volts);

/** Square wave on gate output in Osc mode. duty is 0..1 (high portion of period). */
bool set_gate_osc(uint8_t channel, float frequency_hz, float duty);
void reset_all_outputs();

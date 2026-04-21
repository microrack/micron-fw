#pragma once

#include <Arduino.h>

enum class LoopProfileSlot : uint8_t {
    ProfilingTick,
    PollLifecycle,
    MidiPoll,
    Tick,
    HandleNet,
    OtaHandle,
    HandleLed,
    Count,
};

void profiling_init();
void profiling_tick();
void profiling_add_slot(LoopProfileSlot slot, uint32_t dt_us);

#define LOOP_PROFILE(slot, expr) \
    do { \
        const uint32_t _lp_t0 = micros(); \
        (void)(expr); \
        profiling_add_slot((slot), micros() - _lp_t0); \
    } while (0)

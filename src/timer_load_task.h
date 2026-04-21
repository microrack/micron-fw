#pragma once

// Starts a 10 kHz GPTimer (hardware timer group); alarm ISR generates a sine sample and writes DAC channel A.
void timer_load_task_init();

// Global DAC sine frequency (Hz), clamped inside the implementation.
void timer_load_set_dac_sine_hz(float hz);

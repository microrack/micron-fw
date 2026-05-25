#pragma once

#include <stdint.h>

#include "led.h"

bool match_trimmed(const char* s, const char* expected);
CRGB rel_note_to_color(int rel_note);
float midi_note_to_freq_hz(uint8_t note);
float rel_note_to_freq_hz(int rel_note, uint8_t base_note);

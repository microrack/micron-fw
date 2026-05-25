#include "handler_utils.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float MIDI_A4_FREQ_HZ = 440.0f;
constexpr int MIDI_A4_NOTE = 69;

}  // namespace

bool match_trimmed(const char* s, const char* expected) {
    if (s == nullptr) {
        return false;
    }
    const size_t len = strlen(s);
    size_t trimmed = len;
    while (trimmed > 0 && s[trimmed - 1] == ' ') {
        --trimmed;
    }
    return strncmp(s, expected, trimmed) == 0 && expected[trimmed] == '\0';
}

CRGB rel_note_to_color(int rel_note) {
    const int semitone = ((rel_note % 12) + 12) % 12;
    const uint8_t hue = static_cast<uint8_t>((semitone * 255) / 12);
    return CRGB(CHSV(hue, 255, 255));
}

float midi_note_to_freq_hz(uint8_t note) {
    return MIDI_A4_FREQ_HZ *
        powf(2.0f, (static_cast<int>(note) - MIDI_A4_NOTE) / 12.0f);
}

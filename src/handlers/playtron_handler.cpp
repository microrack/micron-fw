#include "playtron_handler.h"

#include <cstring>

#include "cv_gate.h"
#include "led.h"
#include "logger.h"

namespace {
static constexpr const char* PRODUCT = "Playtron";
static constexpr uint8_t GATE_COUNT = 4;
static constexpr uint8_t NOTE_C4 = 60;
static constexpr uint8_t NOTE_DS4 = 63;
static constexpr uint8_t NOTE_E4 = 64;
static constexpr uint8_t NOTE_G4 = 67;
static constexpr uint8_t NOTE_GS4 = 68;
static constexpr uint8_t NOTE_B4 = 71;
static constexpr uint8_t NOTE_C5 = 72;
static constexpr uint8_t NOTE_DS5 = 75;

static bool match_trimmed(const char* s, const char* expected) {
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

static float midi_note_to_volts(uint8_t note) {
    const int semitones_from_c4 = static_cast<int>(note) - static_cast<int>(NOTE_C4);
    return static_cast<float>(semitones_from_c4) * (1.0f / 12.0f);
}

static CRGB note_to_color(uint8_t note) {
    if (note >= NOTE_C4 && note <= NOTE_DS4) {
        return CRGB::Blue;
    }
    if (note >= NOTE_E4 && note <= NOTE_G4) {
        return CRGB::Cyan;
    }
    if (note >= NOTE_GS4 && note <= NOTE_B4) {
        return CRGB::Orange;
    }
    if (note >= NOTE_C5 && note <= NOTE_DS5) {
        return CRGB::Red;
    }
    return CRGB::Black;
}

class PlaytronHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override {
        return match_trimmed(context.product_name, PRODUCT);
    }

    void midi(const MidiEvent& event) override {
        uint8_t note = 0;
        bool note_on = false;
        bool note_off = false;

        if (event.type == MidiEventType::NoteOn) {
            note = event.data.note_on.note;
            if (event.data.note_on.velocity != 0) {
                note_on = true;
            } else {
                note_off = true;
            }
            logger_printf(
                "Playtron NoteOn ch=%u note=%u vel=%u",
                static_cast<unsigned>(event.data.note_on.channel),
                static_cast<unsigned>(event.data.note_on.note),
                static_cast<unsigned>(event.data.note_on.velocity)
            );
        } else if (event.type == MidiEventType::NoteOff) {
            note = event.data.note_off.note;
            note_off = true;
            logger_printf(
                "Playtron NoteOff ch=%u note=%u vel=%u",
                static_cast<unsigned>(event.data.note_off.channel),
                static_cast<unsigned>(event.data.note_off.note),
                static_cast<unsigned>(event.data.note_off.velocity)
            );
        } else {
            return;
        }

        if (note_on) {
            if (held_notes_count_ < 255) {
                ++held_notes_count_;
            }

            const float volts = midi_note_to_volts(note);
            const CRGB color = note_to_color(note);
            for (uint8_t i = 0; i < GATE_COUNT; ++i) {
                (void)set_cv(i, volts);
                set_gate(i, true);
                set_led_gate(i, color);
            }
            set_led_clock(color);
            set_clock(false);
            return;
        }

        if (note_off) {
            if (held_notes_count_ > 0) {
                --held_notes_count_;
            }
            const bool has_any_note = held_notes_count_ > 0;
            for (uint8_t i = 0; i < GATE_COUNT; ++i) {
                set_gate(i, has_any_note);
                if (!has_any_note) {
                    set_led_gate(i, CRGB::Black);
                }
            }
            if (!has_any_note) {
                set_led_clock(CRGB::Black);
            }
        }
    }

    void press() override {}

    void tick(float dt_sec, uint32_t now_ms) override {
        (void)dt_sec;
        (void)now_ms;
    }

    void enter() override {
        logger_printf("PlaytronHandler: enter");
        held_notes_count_ = 0;
    }

    void exit() override {
        logger_printf("PlaytronHandler: exit");
        held_notes_count_ = 0;
        for (uint8_t i = 0; i < GATE_COUNT; ++i) {
            (void)set_cv(i, 0.0f);
            set_gate(i, false);
            set_led_gate(i, CRGB::Black);
        }
        set_led_clock(CRGB::Black);
        set_clock(false);
    }

   private:
    uint8_t held_notes_count_ = 0;
};

PlaytronHandler g_playtron_handler;
}  // namespace

GadgetHandler& playtron_handler_get() {
    return g_playtron_handler;
}

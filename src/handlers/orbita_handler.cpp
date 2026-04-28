#include "orbita_handler.h"

#include <cstring>

#include "cv_gate.h"
#include "led.h"
#include "logger.h"
#include "midi.h"

namespace {

static constexpr const char* MANUFACTURER = "PLAYTRONICA";
static constexpr const char* PRODUCT      = "ORBITA DIY DANDELION";

// Compare s against expected, ignoring trailing ASCII spaces in s.
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

// Orbita track root MIDI note per channel (1..4). (note - root) = major scale degree from tonic.
static uint8_t orbita_channel_offset(uint8_t channel_1_to_16) {
    static constexpr uint8_t OFFSETS[] = {36, 43, 50, 56};
    if (channel_1_to_16 >= 1 && channel_1_to_16 <= 4) {
        return OFFSETS[channel_1_to_16 - 1];
    }
    return 0;
}

static constexpr uint8_t MIDI_GATE_CH_FIRST = 1;
static constexpr uint8_t MIDI_GATE_CH_LAST  = 4;

static bool midi_channel_to_gate_idx(uint8_t midi_ch_1_to_16, uint8_t* out_gate_idx) {
    if (midi_ch_1_to_16 < MIDI_GATE_CH_FIRST || midi_ch_1_to_16 > MIDI_GATE_CH_LAST) {
        return false;
    }
    *out_gate_idx = static_cast<uint8_t>(midi_ch_1_to_16 - MIDI_GATE_CH_FIRST);
    return true;
}

static uint8_t g_held_notes_per_channel[MIDI_GATE_CH_LAST - MIDI_GATE_CH_FIRST + 1] = {};

// rel_note = major scale degrees from tonic (0=I, 1=II, ...), not semitones.
static int major_scale_steps_to_semitones(int steps) {
    if (steps < 0) {
        return 0;
    }
    static constexpr uint8_t MAJOR_SEMITONES_FROM_TONIC[7] = {0, 2, 4, 5, 7, 9, 11};
    const int oct = steps / 7;
    const int rem = steps % 7;
    return oct * 12 + MAJOR_SEMITONES_FROM_TONIC[rem];
}

static float rel_major_steps_to_volts(int rel_note_degrees) {
    const int semitones = major_scale_steps_to_semitones(rel_note_degrees);
    return static_cast<float>(semitones) * (1.0f / 12.0f);
}

static void orbita_sync_gates_leds() {
    for (uint8_t ch = MIDI_GATE_CH_FIRST; ch <= MIDI_GATE_CH_LAST; ++ch) {
        const uint8_t idx = static_cast<uint8_t>(ch - MIDI_GATE_CH_FIRST);
        const bool on = g_held_notes_per_channel[idx] > 0;
        set_gate(idx, on);
        set_led_gate(idx, on ? LedGateColor::Red : LedGateColor::Off);
    }
    set_led_gate(4, LedGateColor::Off);
    set_gate(4, false);
}

static void orbita_reset_outputs() {
    for (uint8_t i = 0; i < (MIDI_GATE_CH_LAST - MIDI_GATE_CH_FIRST + 1); ++i) {
        g_held_notes_per_channel[i] = 0;
        set_cv(i, 0.0f);
        set_gate(i, false);
        set_led_gate(i, LedGateColor::Off);
    }
    set_led_gate(4, LedGateColor::Off);
    set_gate(4, false);
}

class OrbitaHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override {
        return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
               match_trimmed(context.product_name, PRODUCT);
    }

    void midi(const MidiEvent& event) override {
        uint8_t midi_ch = 0;
        uint8_t note = 0;
        uint8_t velocity = 0;
        bool is_note_on = false;
        bool is_note_off = false;

        if (event.type == MidiEventType::NoteOn) {
            midi_ch = event.data.note_on.channel;
            note = event.data.note_on.note;
            velocity = event.data.note_on.velocity;
            if (velocity != 0) {
                is_note_on = true;
            } else {
                is_note_off = true;
            }
        } else if (event.type == MidiEventType::NoteOff) {
            midi_ch = event.data.note_off.channel;
            is_note_off = true;
        } else {
            return;
        }

        uint8_t gate_idx = 0;
        if (!midi_channel_to_gate_idx(midi_ch, &gate_idx)) {
            return;
        }

        if (is_note_on) {
            const int rel_note = static_cast<int>(note) -
                                 static_cast<int>(orbita_channel_offset(midi_ch));
            if (g_held_notes_per_channel[gate_idx] < 255) {
                ++g_held_notes_per_channel[gate_idx];
            }
            (void)set_cv(gate_idx, rel_major_steps_to_volts(rel_note));
            orbita_sync_gates_leds();
            logger_printf(
                "Orbita NoteOn  ch=%u note=%u rel_note=%d vel=%u",
                static_cast<unsigned>(midi_ch),
                static_cast<unsigned>(note),
                rel_note,
                static_cast<unsigned>(velocity)
            );
            return;
        }

        if (is_note_off) {
            if (g_held_notes_per_channel[gate_idx] > 0) {
                --g_held_notes_per_channel[gate_idx];
            }
            orbita_sync_gates_leds();
        }
    }

    void press() override {}

    void tick(float dt_sec, uint32_t now_ms) override {
        (void)dt_sec;
        (void)now_ms;
    }

    void enter() override {
        logger_printf("OrbitaHandler: enter");
        orbita_reset_outputs();

        // Note length (CC 24..27 = 10)
        for (uint8_t cc = 24; cc <= 27; ++cc) {
            midi_send_cc(1, cc, 10);
        }
        // Velocity (CC 28..31 = 127)
        for (uint8_t cc = 28; cc <= 31; ++cc) {
            midi_send_cc(1, cc, 127);
        }
        // Track MIDI channel (CC 32..35 = 1..4)
        for (uint8_t i = 0; i < 4; ++i) {
            midi_send_cc(1, static_cast<uint8_t>(32 + i), static_cast<uint8_t>(1 + i));
        }
        // Probability (CC 42..45 = 127)
        for (uint8_t cc = 42; cc <= 45; ++cc) {
            midi_send_cc(1, cc, 127);
        }
        // Range (CC 46..49 = 0)
        for (uint8_t cc = 46; cc <= 49; ++cc) {
            midi_send_cc(1, cc, 0);
        }
    }
    void exit() override {
        logger_printf("OrbitaHandler: exit");
        orbita_reset_outputs();
    }
};

OrbitaHandler g_orbita_handler;
}  // namespace

GadgetHandler& orbita_handler_get() {
    return g_orbita_handler;
}

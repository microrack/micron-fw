#include "orbita_handler.h"

#include <Arduino.h>

#include "cv_gate.h"
#include "handler_utils.h"
#include "led.h"
#include "logger.h"
#include "midi.h"

static constexpr const char* MANUFACTURER = "PLAYTRONICA";
static constexpr const char* PRODUCT      = "ORBITA DIY DANDELION";

bool OrbitaHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
           match_trimmed(context.product_name, PRODUCT);
}

void OrbitaHandler::midi(const MidiEvent& event) {
    if (event.type == MidiEventType::ControlChange) {
        if (event.data.control_change.channel == CC_CHANNEL) {
            handle_control_change(
                event.data.control_change.controller,
                event.data.control_change.value
            );
        }
        return;
    }
    if (event.type == MidiEventType::NoteOn) {
        const uint8_t ch = event.data.note_on.channel;
        const uint8_t note = event.data.note_on.note;
        const uint8_t velocity = event.data.note_on.velocity;
        const NoteEventAction action =
            (velocity != 0) ? NoteEventAction::NoteOn : NoteEventAction::NoteOff;
        handle_note_event(ch, note, action);
        return;
    }
    if (event.type == MidiEventType::NoteOff) {
        const uint8_t ch = event.data.note_off.channel;
        const uint8_t note = event.data.note_off.note;
        handle_note_event(ch, note, NoteEventAction::NoteOff);
        return;
    }
    if (event.type == MidiEventType::SingleByte) {
        const uint8_t status = event.data.single_byte.byte0;
        if (status == MIDI_CLOCK_STATUS) {
            handle_clock_tick(millis());
        } else if (status == MIDI_CLOCK_START_STATUS ||
                   status == MIDI_CLOCK_CONTINUE_STATUS ||
                   status == MIDI_CLOCK_STOP_STATUS) {
            // Realign the divider phase to the transport so the first emitted
            // pulse after Start/Continue lands on the very next MIDI clock.
            reset_clock_divider();
        }
        return;
    }
}

void OrbitaHandler::handle_control_change(uint8_t controller, uint8_t value) {
    if (controller != CC_CV0_CONTROLLER) {
        return;
    }
    const float volts = cc_value_to_volts(value);
    (void)set_cv(CC_CV_IDX, volts);

    // Map 0..127 CC range to the full 0..255 hue wheel (×2 keeps the relation
    // 1 CC step = 2 hue steps, near-linear and cheap to compute).
    const uint8_t clamped_value = (value > 127) ? 127 : value;
    const uint8_t hue = static_cast<uint8_t>(clamped_value << 1);
    set_led_gate(CC_CV_IDX, CRGB(CHSV(hue, 255, 255)));

    logger_printf(
        "Orbita CC -> CV idx=%u value=%u hue=%u",
        static_cast<unsigned>(CC_CV_IDX),
        static_cast<unsigned>(value),
        static_cast<unsigned>(hue)
    );
}

void OrbitaHandler::handle_note_event(uint8_t midi_ch, uint8_t note, NoteEventAction action) {
    if (midi_ch < NOTE_CH_FIRST || midi_ch > NOTE_CH_LAST) {
        return;
    }

    // ch 2 -> idx 1, ch 3 -> idx 2, ch 4 -> idx 3 (idx 0 is owned by CC).
    const uint8_t led_idx = static_cast<uint8_t>(midi_ch - 1);

    if (action == NoteEventAction::NoteOn) {
        if (common_held_notes_count_ < 255) {
            ++common_held_notes_count_;
        }
        if (held_notes_per_channel_[led_idx] < 255) {
            ++held_notes_per_channel_[led_idx];
        }
        const float volts = midi_note_to_volts(note);
        const CRGB color =
            rel_note_to_color(static_cast<int>(note) - static_cast<int>(BASE_NOTE));
        // CV/Gate 1..3 move in unison; only the source channel's LED is touched.
        for (uint8_t i = NOTE_GATE_FIRST; i <= NOTE_GATE_LAST; ++i) {
            (void)set_cv(i, volts);
            set_gate(i, true);
        }
        set_led_gate(led_idx, color);
        logger_printf(
            "Orbita NoteOn ch=%u note=%u volts=%.3f",
            static_cast<unsigned>(midi_ch),
            static_cast<unsigned>(note),
            static_cast<double>(volts)
        );
        return;
    }

    if (common_held_notes_count_ > 0) {
        --common_held_notes_count_;
    }
    if (held_notes_per_channel_[led_idx] > 0) {
        --held_notes_per_channel_[led_idx];
    }
    if (held_notes_per_channel_[led_idx] == 0) {
        set_led_gate(led_idx, CRGB::Black);
    }
    if (common_held_notes_count_ == 0) {
        for (uint8_t i = NOTE_GATE_FIRST; i <= NOTE_GATE_LAST; ++i) {
            set_gate(i, false);
        }
    }
}

void OrbitaHandler::handle_clock_tick(uint32_t now_ms) {
    if (++clock_divider_counter_ < CLOCK_DIVIDER) {
        return;
    }
    clock_divider_counter_ = 0;
    set_clock(true);
    set_led_clock(CRGB::White);
    clock_pulse_active_ = true;
    clock_pulse_end_ms_ = now_ms + CLOCK_PULSE_MS;
}

void OrbitaHandler::reset_clock_divider() {
    // Start counting from CLOCK_DIVIDER-1 so the very next clock tick emits the
    // first pulse, keeping the divided clock phase-locked to MIDI transport.
    clock_divider_counter_ = CLOCK_DIVIDER - 1;
}

void OrbitaHandler::reset_note_state() {
    common_held_notes_count_ = 0;
    for (uint8_t i = 0; i < CV_GATE_COUNT; ++i) {
        held_notes_per_channel_[i] = 0;
    }
}

void OrbitaHandler::press() {
    // No mode switching: the handler runs in a single mode.
}

void OrbitaHandler::tick(float dt_sec, uint32_t now_ms, const GadgetTickInputs& inputs) {
    (void)dt_sec;
    (void)inputs;
    if (!clock_pulse_active_) {
        return;
    }
    // Wraparound-safe comparison of two uint32_t time stamps.
    if (static_cast<int32_t>(now_ms - clock_pulse_end_ms_) >= 0) {
        clock_pulse_active_ = false;
        set_clock(false);
        set_led_clock(CRGB::Black);
    }
}

void OrbitaHandler::enter() {
    logger_printf("OrbitaHandler: enter");
    set_cv_mode(CvMode::Cv);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    clock_pulse_active_ = false;
    clock_pulse_end_ms_ = 0;
    clock_divider_counter_ = 0;
    reset_note_state();

    for (uint8_t cc = 24; cc <= 27; ++cc) {
        midi_send_cc(1, cc, 10);
    }
    for (uint8_t cc = 28; cc <= 31; ++cc) {
        midi_send_cc(1, cc, 127);
    }
    for (uint8_t i = 0; i < 4; ++i) {
        midi_send_cc(1, static_cast<uint8_t>(32 + i), static_cast<uint8_t>(1 + i));
    }
    for (uint8_t cc = 42; cc <= 45; ++cc) {
        midi_send_cc(1, cc, 127);
    }
    for (uint8_t cc = 46; cc <= 49; ++cc) {
        midi_send_cc(1, cc, 0);
    }
    midi_send_cc(1, 38, 127);
    for (uint8_t cc = 39; cc <= 41; ++cc) {
        midi_send_cc(1, cc, 0);
    }
}

void OrbitaHandler::exit() {
    logger_printf("OrbitaHandler: exit");
    set_cv_mode(CvMode::Cv);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    clock_pulse_active_ = false;
    clock_pulse_end_ms_ = 0;
    clock_divider_counter_ = 0;
    reset_note_state();
}

float OrbitaHandler::midi_note_to_volts(uint8_t note) {
    // Base note BASE_NOTE -> 1 V, +1 V per octave (12 semitones), clamped to 0..5 V.
    const float v = 1.0f + (static_cast<float>(note) - static_cast<float>(BASE_NOTE)) / 12.0f;
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 5.0f) {
        return 5.0f;
    }
    return v;
}

float OrbitaHandler::cc_value_to_volts(uint8_t value) {
    constexpr float MAX_CC = 127.0f;
    constexpr float MAX_VOLTS = 5.0f;
    const float clamped = (value > 127) ? 127.0f : static_cast<float>(value);
    return (clamped / MAX_CC) * MAX_VOLTS;
}

OrbitaHandler g_orbita_handler;

GadgetHandler& orbita_handler_get() {
    return g_orbita_handler;
}

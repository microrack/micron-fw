#include "orbita_handler.h"

#include <cmath>

#include "cv_gate.h"
#include "handler_utils.h"
#include "led.h"
#include "logger.h"
#include "midi.h"

static constexpr const char* MANUFACTURER = "PLAYTRONICA";
static constexpr const char* PRODUCT      = "ORBITA DIY DANDELION";
static constexpr float SYNTH_TRANSITION_HZ = 4.0f;

bool OrbitaHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
           match_trimmed(context.product_name, PRODUCT);
}

void OrbitaHandler::midi(const MidiEvent& event) {
    uint8_t midi_ch = 0;
    uint8_t note = 0;
    NoteEventAction action = NoteEventAction::NoteOff;

    if (event.type == MidiEventType::NoteOn) {
        midi_ch = event.data.note_on.channel;
        note = event.data.note_on.note;
        const uint8_t velocity = event.data.note_on.velocity;
        action =
            (velocity != 0) ? NoteEventAction::NoteOn : NoteEventAction::NoteOff;
    } else if (event.type == MidiEventType::NoteOff) {
        midi_ch = event.data.note_off.channel;
        action = NoteEventAction::NoteOff;
    } else {
        return;
    }

    uint8_t gate_idx_unused = 0;
    if (!midi_channel_to_gate_idx(midi_ch, &gate_idx_unused)) {
        return;
    }

    if (mode_ == OrbitaMode::Common) {
        handle_common_mode_note_event(midi_ch, note, action);
        return;
    }
    if (mode_ == OrbitaMode::Synth) {
        handle_synth_mode_note_event(midi_ch, note, action);
        return;
    }
    handle_track_mode_note_event(midi_ch, note, action);
}

void OrbitaHandler::handle_common_mode_note_event(
    uint8_t midi_ch,
    uint8_t note,
    NoteEventAction action
) {
    const int rel_note = static_cast<int>(note) - static_cast<int>(orbita_channel_offset(midi_ch));
    const int octave_steps = static_cast<int>(midi_ch - MIDI_GATE_CH_FIRST) * 7;  // 7 scale steps = 1 octave
    const int rel_note_transposed = rel_note + octave_steps;

    if (action == NoteEventAction::NoteOn) {
        if (common_mode_held_notes_count_ < 255) {
            ++common_mode_held_notes_count_;
        }

        const float volts = rel_major_steps_to_volts(rel_note_transposed);
        const CRGB color = rel_note_to_color(rel_note_transposed);
        const uint8_t active_track_idx = static_cast<uint8_t>(midi_ch - MIDI_GATE_CH_FIRST);
        set_all_cv(volts);
        set_all_gates(true);
        for (uint8_t i = 0; i < ORBITA_MIDI_GATE_COUNT; ++i) {
            held_notes_per_channel_[i] = common_mode_held_notes_count_;
            led_color_per_channel_[i] =
                (i == active_track_idx) ? color : scale_color(color, 0.2f);
            if (!mode_transition_in_progress()) {
                set_led_gate(i, led_color_per_channel_[i]);
            }
        }
        logger_printf(
            "Orbita Common NoteOn ch=%u note=%u rel_note=%d rel_note_transposed=%d",
            static_cast<unsigned>(midi_ch),
            static_cast<unsigned>(note),
            rel_note,
            rel_note_transposed
        );
        return;
    }

    if (action == NoteEventAction::NoteOff) {
        if (common_mode_held_notes_count_ > 0) {
            --common_mode_held_notes_count_;
        }
        const bool has_any_note = common_mode_held_notes_count_ > 0;
        set_all_gates(has_any_note);
        for (uint8_t i = 0; i < ORBITA_MIDI_GATE_COUNT; ++i) {
            held_notes_per_channel_[i] = common_mode_held_notes_count_;
            if (!has_any_note) {
                led_color_per_channel_[i] = CRGB::Black;
                if (!mode_transition_in_progress()) {
                    set_led_gate(i, CRGB::Black);
                }
            }
        }
    }
}

void OrbitaHandler::handle_track_mode_note_event(
    uint8_t midi_ch,
    uint8_t note,
    NoteEventAction action
) {
    uint8_t gate_idx = 0;
    if (!midi_channel_to_gate_idx(midi_ch, &gate_idx)) {
        return;
    }

    if (action == NoteEventAction::NoteOn) {
        const int rel_note = static_cast<int>(note) - static_cast<int>(orbita_channel_offset(midi_ch));
        if (held_notes_per_channel_[gate_idx] < 255) {
            ++held_notes_per_channel_[gate_idx];
        }
        led_color_per_channel_[gate_idx] = rel_note_to_color(rel_note);
        (void)set_cv(gate_idx, rel_major_steps_to_volts(rel_note));
        if (!mode_transition_in_progress()) {
            set_led_gate(gate_idx, led_color_per_channel_[gate_idx]);
        }
        sync_gates_leds();
        logger_printf(
            "Orbita NoteOn  ch=%u note=%u rel_note=%d",
            static_cast<unsigned>(midi_ch),
            static_cast<unsigned>(note),
            rel_note
        );
        return;
    }

    if (action == NoteEventAction::NoteOff) {
        if (held_notes_per_channel_[gate_idx] > 0) {
            --held_notes_per_channel_[gate_idx];
        }
        if (held_notes_per_channel_[gate_idx] == 0) {
            led_color_per_channel_[gate_idx] = CRGB::Black;
            if (!mode_transition_in_progress()) {
                set_led_gate(gate_idx, CRGB::Black);
            }
        }
        sync_gates_leds();
    }
}

void OrbitaHandler::handle_synth_mode_note_event(
    uint8_t midi_ch,
    uint8_t note,
    NoteEventAction action
) {
    uint8_t gate_idx = 0;
    if (!midi_channel_to_gate_idx(midi_ch, &gate_idx)) {
        return;
    }

    const int rel_note = static_cast<int>(note) - static_cast<int>(orbita_channel_offset(midi_ch));
    const int octave_steps = static_cast<int>(midi_ch - MIDI_GATE_CH_FIRST) * 7;  // same as Common
    const int rel_note_transposed = rel_note + octave_steps;

    if (action == NoteEventAction::NoteOn) {
        if (held_notes_per_channel_[gate_idx] < 255) {
            ++held_notes_per_channel_[gate_idx];
        }
        led_color_per_channel_[gate_idx] = rel_note_to_color(rel_note_transposed);
        (void)set_cv(gate_idx, rel_major_steps_to_volts(rel_note_transposed));
        set_gate(gate_idx, true);
        if (!mode_transition_in_progress()) {
            set_led_gate(gate_idx, led_color_per_channel_[gate_idx]);
        }
        logger_printf(
            "Orbita Synth NoteOn ch=%u note=%u rel_note_transposed=%d",
            static_cast<unsigned>(midi_ch),
            static_cast<unsigned>(note),
            rel_note_transposed
        );
        return;
    }

    if (action == NoteEventAction::NoteOff) {
        if (held_notes_per_channel_[gate_idx] > 0) {
            --held_notes_per_channel_[gate_idx];
        }
        if (held_notes_per_channel_[gate_idx] == 0) {
            set_gate(gate_idx, false);
            led_color_per_channel_[gate_idx] = CRGB::Black;
            if (!mode_transition_in_progress()) {
                set_led_gate(gate_idx, CRGB::Black);
            }
        }
    }
}

void OrbitaHandler::press() {
    switch (mode_) {
        case OrbitaMode::Track:
            mode_ = OrbitaMode::Common;
            break;
        case OrbitaMode::Common:
            mode_ = OrbitaMode::Synth;
            break;
        case OrbitaMode::Synth:
            mode_ = OrbitaMode::Track;
            break;
    }
    // Modes keep independent note accounting; reset note/output state on switch
    // to avoid stale counters affecting the new mode.
    reset_outputs();
    apply_cv_gate_mode_for_current_orbita_mode();
    mode_transition_pending_ = true;
    logger_printf(
        "OrbitaHandler: mode switched to %s",
        (mode_ == OrbitaMode::Track) ? "Track" :
        (mode_ == OrbitaMode::Common) ? "Common" : "Synth"
    );
}

void OrbitaHandler::tick(float dt_sec, uint32_t now_ms) {
    (void)dt_sec;
    if (mode_transition_pending_) {
        mode_transition_pending_ = false;
        mode_transition_active_ = true;
        mode_transition_start_ms_ = now_ms;
    }

    if (!mode_transition_active_) {
        return;
    }

    const uint32_t elapsed_ms = now_ms - mode_transition_start_ms_;
    if (elapsed_ms >= MODE_TRANSITION_MS) {
        mode_transition_active_ = false;
        restore_note_leds();
        return;
    }

    const float progress = static_cast<float>(elapsed_ms) / static_cast<float>(MODE_TRANSITION_MS);
    if (mode_ == OrbitaMode::Track) {
        render_track_mode_transition(progress);
    } else if (mode_ == OrbitaMode::Common) {
        render_common_mode_transition(progress);
    } else {
        render_synth_mode_transition(progress);
    }
}

void OrbitaHandler::enter() {
    logger_printf("OrbitaHandler: enter");
    mode_ = OrbitaMode::Track;
    mode_transition_active_ = false;
    mode_transition_pending_ = false;
    apply_cv_gate_mode_for_current_orbita_mode();
    logger_printf("OrbitaHandler: mode %s", "Track");
    reset_outputs();

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
}

void OrbitaHandler::exit() {
    logger_printf("OrbitaHandler: exit");
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_outputs();
}

void OrbitaHandler::apply_cv_gate_mode_for_current_orbita_mode() {
    if (mode_ == OrbitaMode::Synth) {
        set_cv_gate_mode(CvGateMode::Synth);
    } else {
        set_cv_gate_mode(CvGateMode::CvGate);
    }
}

bool OrbitaHandler::midi_channel_to_gate_idx(uint8_t midi_ch_1_to_16, uint8_t* out_gate_idx) {
    if (midi_ch_1_to_16 < MIDI_GATE_CH_FIRST || midi_ch_1_to_16 > MIDI_GATE_CH_LAST) {
        return false;
    }
    *out_gate_idx = static_cast<uint8_t>(midi_ch_1_to_16 - MIDI_GATE_CH_FIRST);
    return true;
}

uint8_t OrbitaHandler::orbita_channel_offset(uint8_t channel_1_to_16) {
    static constexpr uint8_t OFFSETS[] = {36, 43, 50, 56};
    if (channel_1_to_16 >= 1 && channel_1_to_16 <= 4) {
        return OFFSETS[channel_1_to_16 - 1];
    }
    return 0;
}

int OrbitaHandler::major_scale_steps_to_semitones(int steps) {
    if (steps < 0) {
        return 0;
    }
    static constexpr uint8_t MAJOR_SEMITONES_FROM_TONIC[7] = {0, 2, 4, 5, 7, 9, 11};
    const int oct = steps / 7;
    const int rem = steps % 7;
    return oct * 12 + MAJOR_SEMITONES_FROM_TONIC[rem];
}

float OrbitaHandler::rel_major_steps_to_volts(int rel_note_degrees) {
    const int semitones = major_scale_steps_to_semitones(rel_note_degrees);
    return static_cast<float>(semitones) * (1.0f / 12.0f);
}

CRGB OrbitaHandler::rel_note_to_color(int rel_note) {
    const int degree_mod = ((rel_note % 7) + 7) % 7;
    switch (degree_mod) {
        case 0:
            return CRGB::Red;
        case 1:
            return CRGB::Orange;
        case 2:
            return CRGB::Yellow;
        case 3:
            return CRGB::Green;
        case 4:
            return CRGB::Cyan;
        case 5:
            return CRGB::Blue;
        case 6:
            return CRGB::Purple;
    }
    return CRGB::Black;
}

uint8_t OrbitaHandler::lerp_u8(uint8_t a, uint8_t b, float t) {
    const float clamped = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    const float value = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * clamped;
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return static_cast<uint8_t>(value + 0.5f);
}

CRGB OrbitaHandler::lerp_color(const CRGB& from, const CRGB& to, float t) {
    return CRGB(lerp_u8(from.r, to.r, t), lerp_u8(from.g, to.g, t), lerp_u8(from.b, to.b, t));
}

CRGB OrbitaHandler::scale_color(const CRGB& color, float scale) {
    const float clamped = (scale < 0.0f) ? 0.0f : ((scale > 1.0f) ? 1.0f : scale);
    return CRGB(
        lerp_u8(0, color.r, clamped),
        lerp_u8(0, color.g, clamped),
        lerp_u8(0, color.b, clamped)
    );
}

bool OrbitaHandler::mode_transition_in_progress() const {
    return mode_transition_pending_ || mode_transition_active_;
}

void OrbitaHandler::restore_note_leds() {
    for (uint8_t i = 0; i < ORBITA_MIDI_GATE_COUNT; ++i) {
        set_led_gate(i, led_color_per_channel_[i]);
    }
    set_led_clock(CRGB::Black);
}

void OrbitaHandler::render_track_mode_transition(float progress_0_to_1) {
    const float clamped = (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float center = clamped * static_cast<float>(ORBITA_LED_COUNT - 1);
    const float color_phase = clamped * 6.0f;
    const int color_idx = static_cast<int>(color_phase);
    const float color_blend = color_phase - static_cast<float>(color_idx);
    const CRGB color_from = rel_note_to_color(color_idx);
    const CRGB color_to = rel_note_to_color((color_idx + 1) % 7);
    const CRGB color = lerp_color(color_from, color_to, color_blend);

    for (uint8_t led = 0; led < ORBITA_LED_COUNT; ++led) {
        const float distance = fabsf(static_cast<float>(led) - center);
        const float intensity = (distance >= 1.0f) ? 0.0f : (1.0f - distance);
        const CRGB led_color = scale_color(color, intensity);
        if (led < ORBITA_MIDI_GATE_COUNT) {
            set_led_gate(led, led_color);
        } else {
            set_led_clock(led_color);
        }
    }
}

void OrbitaHandler::render_common_mode_transition(float progress_0_to_1) {
    const float clamped = (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float color_phase = clamped * 6.0f;
    const int color_idx = static_cast<int>(color_phase);
    const float color_blend = color_phase - static_cast<float>(color_idx);
    const CRGB color_from = rel_note_to_color(color_idx);
    const CRGB color_to = rel_note_to_color((color_idx + 1) % 7);
    const CRGB color = lerp_color(color_from, color_to, color_blend);

    for (uint8_t led = 0; led < ORBITA_MIDI_GATE_COUNT; ++led) {
        set_led_gate(led, color);
    }
}

void OrbitaHandler::render_synth_mode_transition(float progress_0_to_1) {
    const float clamped = (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float phase = clamped * (2.0f * 3.14159265359f * SYNTH_TRANSITION_HZ);
    const float sine_0_to_1 = 0.5f + 0.5f * sinf(phase);

    // During synth transition only gate 0 is active; all others stay off.
    for (uint8_t i = 1; i < ORBITA_MIDI_GATE_COUNT; ++i) {
        set_gate(i, false);
        set_led_gate(i, CRGB::Black);
    }
    set_clock(false);
    set_led_clock(CRGB::Black);

    set_gate(0, sine_0_to_1 >= 0.5f);
    set_led_gate(0, scale_color(CRGB::White, sine_0_to_1));
}

void OrbitaHandler::sync_gates_leds() {
    for (uint8_t ch = MIDI_GATE_CH_FIRST; ch <= MIDI_GATE_CH_LAST; ++ch) {
        const uint8_t idx = static_cast<uint8_t>(ch - MIDI_GATE_CH_FIRST);
        set_gate(idx, held_notes_per_channel_[idx] > 0);
    }
    set_clock(false);
}

void OrbitaHandler::reset_outputs() {
    common_mode_held_notes_count_ = 0;
    reset_all_outputs();
    set_led_all(CRGB::Black);
    for (uint8_t i = 0; i < ORBITA_MIDI_GATE_COUNT; ++i) {
        held_notes_per_channel_[i] = 0;
        led_color_per_channel_[i] = CRGB::Black;
    }
}

OrbitaHandler g_orbita_handler;

GadgetHandler& orbita_handler_get() {
    return g_orbita_handler;
}

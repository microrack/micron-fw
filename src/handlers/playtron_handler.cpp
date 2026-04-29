#include "playtron_handler.h"

#include <cmath>

#include "cv_gate.h"
#include "handler_utils.h"
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
static constexpr int SYNTH_OCTAVE_UP_SEMITONES = 12;
static constexpr uint32_t MODE_TRANSITION_MS = 1000;
static constexpr float SYNTH_TRANSITION_HZ = 4.0f;
static constexpr CRGB NOTE_COLORS[] = {
    CRGB::Blue,
    CRGB::Cyan,
    CRGB::Orange,
    CRGB::Red,
};

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
}  // namespace

bool PlaytronHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.product_name, PRODUCT);
}

void PlaytronHandler::midi(const MidiEvent& event) {
    uint8_t note = 0;
    NoteEventAction action = NoteEventAction::NoteOff;

    if (event.type == MidiEventType::NoteOn) {
        note = event.data.note_on.note;
        action = (event.data.note_on.velocity != 0)
                     ? NoteEventAction::NoteOn
                     : NoteEventAction::NoteOff;
        logger_printf(
            "Playtron NoteOn ch=%u note=%u vel=%u",
            static_cast<unsigned>(event.data.note_on.channel),
            static_cast<unsigned>(event.data.note_on.note),
            static_cast<unsigned>(event.data.note_on.velocity)
        );
    } else if (event.type == MidiEventType::NoteOff) {
        note = event.data.note_off.note;
        action = NoteEventAction::NoteOff;
        logger_printf(
            "Playtron NoteOff ch=%u note=%u vel=%u",
            static_cast<unsigned>(event.data.note_off.channel),
            static_cast<unsigned>(event.data.note_off.note),
            static_cast<unsigned>(event.data.note_off.velocity)
        );
    } else {
        return;
    }

    if (action == NoteEventAction::NoteOn) {
        uint8_t existing_channel = 0;
        if (find_channel_for_note(note, &existing_channel)) {
            logger_printf(
                "Playtron NoteOn ignored: note=%u already active on ch=%u",
                static_cast<unsigned>(note),
                static_cast<unsigned>(existing_channel)
            );
            return;
        }

        uint8_t free_channel = 0;
        if (!find_free_channel(&free_channel)) {
            logger_printf(
                "Playtron NoteOn ignored: no free channels for note=%u",
                static_cast<unsigned>(note)
            );
            return;
        }

        const uint8_t output_note = note_to_output_note(note);
        const float volts = midi_note_to_volts(output_note);
        const CRGB color = note_to_color(note);
        channel_note_active_[free_channel] = true;
        channel_note_[free_channel] = note;
        channel_color_[free_channel] = color;

        (void)set_cv(free_channel, volts);
        set_gate(free_channel, true);
        set_led_gate(free_channel, color);
        set_led_clock(current_clock_color());
        set_clock(false);
        return;
    }

    if (action == NoteEventAction::NoteOff) {
        uint8_t active_channel = 0;
        if (!find_channel_for_note(note, &active_channel)) {
            logger_printf(
                "Playtron NoteOff ignored: note=%u not active",
                static_cast<unsigned>(note)
            );
            return;
        }

        channel_note_active_[active_channel] = false;
        channel_note_[active_channel] = 0;
        channel_color_[active_channel] = CRGB::Black;
        set_gate(active_channel, false);
        set_led_gate(active_channel, CRGB::Black);
        set_led_clock(current_clock_color());
    }
}

void PlaytronHandler::press() {
    mode_ = (mode_ == PlaytronMode::CvGate) ? PlaytronMode::Synth : PlaytronMode::CvGate;
    apply_cv_gate_mode_for_current_mode();
    refresh_active_channels_cv();
    mode_transition_pending_ = true;
    logger_printf(
        "PlaytronHandler: mode switched to %s",
        (mode_ == PlaytronMode::CvGate) ? "CvGate" : "Synth"
    );
}

void PlaytronHandler::tick(float dt_sec, uint32_t now_ms) {
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
    if (mode_ == PlaytronMode::CvGate) {
        render_cvgate_mode_transition(progress);
    } else {
        render_synth_mode_transition(progress);
    }
}

void PlaytronHandler::enter() {
    logger_printf("PlaytronHandler: enter");
    mode_ = PlaytronMode::CvGate;
    clear_channel_state();
    mode_transition_active_ = false;
    mode_transition_pending_ = false;
    apply_cv_gate_mode_for_current_mode();
}

void PlaytronHandler::exit() {
    logger_printf("PlaytronHandler: exit");
    mode_ = PlaytronMode::CvGate;
    clear_channel_state();
    mode_transition_active_ = false;
    mode_transition_pending_ = false;
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_all_outputs();
    set_led_all(CRGB::Black);
}

void PlaytronHandler::apply_cv_gate_mode_for_current_mode() {
    if (mode_ == PlaytronMode::Synth) {
        set_cv_gate_mode(CvGateMode::Synth);
    } else {
        set_cv_gate_mode(CvGateMode::CvGate);
    }
}

void PlaytronHandler::restore_note_leds() {
    set_led_all(CRGB::Black);
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        if (channel_note_active_[i]) {
            set_led_gate(i, channel_color_[i]);
        }
    }
    set_led_clock(current_clock_color());
}

void PlaytronHandler::render_cvgate_mode_transition(float progress_0_to_1) {
    const float clamped =
        (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float phase = clamped * static_cast<float>(GATE_COUNT);
    const uint8_t gate_idx = static_cast<uint8_t>(phase) % GATE_COUNT;
    const uint8_t color_idx =
        static_cast<uint8_t>(phase * static_cast<float>(sizeof(NOTE_COLORS) / sizeof(NOTE_COLORS[0]))) %
        static_cast<uint8_t>(sizeof(NOTE_COLORS) / sizeof(NOTE_COLORS[0]));

    set_led_all(CRGB::Black);
    set_led_gate(gate_idx, NOTE_COLORS[color_idx]);
    set_led_clock(NOTE_COLORS[color_idx]);
}

void PlaytronHandler::render_synth_mode_transition(float progress_0_to_1) {
    const float clamped =
        (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float phase = clamped * (2.0f * 3.14159265359f * SYNTH_TRANSITION_HZ);
    const float sine_0_to_1 = 0.5f + 0.5f * sinf(phase);

    for (uint8_t i = 1; i < GATE_COUNT; ++i) {
        set_gate(i, false);
        set_led_gate(i, CRGB::Black);
    }
    set_clock(false);
    set_led_clock(CRGB::Black);

    set_gate(0, sine_0_to_1 >= 0.5f);
    const uint8_t brightness = static_cast<uint8_t>(sine_0_to_1 * 255.0f);
    set_led_gate(0, CRGB(brightness, brightness, brightness));
}

uint8_t PlaytronHandler::note_to_output_note(uint8_t note) const {
    if (mode_ != PlaytronMode::Synth) {
        return note;
    }
    const uint16_t shifted = static_cast<uint16_t>(note) + static_cast<uint16_t>(SYNTH_OCTAVE_UP_SEMITONES);
    return (shifted > 127U) ? 127U : static_cast<uint8_t>(shifted);
}

bool PlaytronHandler::find_free_channel(uint8_t* out_channel) const {
    if (out_channel == nullptr) {
        return false;
    }
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        if (!channel_note_active_[i]) {
            *out_channel = i;
            return true;
        }
    }
    return false;
}

bool PlaytronHandler::find_channel_for_note(uint8_t note, uint8_t* out_channel) const {
    if (out_channel == nullptr) {
        return false;
    }
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        if (channel_note_active_[i] && channel_note_[i] == note) {
            *out_channel = i;
            return true;
        }
    }
    return false;
}

void PlaytronHandler::refresh_active_channels_cv() {
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        if (!channel_note_active_[i]) {
            continue;
        }
        const uint8_t output_note = note_to_output_note(channel_note_[i]);
        const float volts = midi_note_to_volts(output_note);
        (void)set_cv(i, volts);
        set_gate(i, true);
    }
    set_clock(false);
}

CRGB PlaytronHandler::current_clock_color() const {
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        if (channel_note_active_[i]) {
            return channel_color_[i];
        }
    }
    return CRGB::Black;
}

void PlaytronHandler::clear_channel_state() {
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        channel_note_active_[i] = false;
        channel_note_[i] = 0;
        channel_color_[i] = CRGB::Black;
    }
}

namespace {
PlaytronHandler g_playtron_handler;
}  // namespace

GadgetHandler& playtron_handler_get() {
    return g_playtron_handler;
}

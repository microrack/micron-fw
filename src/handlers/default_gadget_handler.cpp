#include "default_gadget_handler.h"

#include <cmath>

#include "cv_gate.h"
#include "led.h"
#include "logger.h"

namespace {

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

class DefaultGadgetHandler : public GadgetHandler {
   public:
    enum class Mode : uint8_t {
        CvGate = 0,
        Synth = 1,
    };

    bool probe(const UsbDeviceContext& context) override {
        (void)context;
        return true;
    }
    void midi(const MidiEvent& event) override;
    void press() override;
    void tick(float dt_sec, uint32_t now_ms) override;
    void enter() override;
    void exit() override;

   private:
    enum class NoteEventAction : uint8_t {
        NoteOn = 0,
        NoteOff = 1,
    };

    static constexpr uint8_t kMaxHeldNotes = 16;

    void apply_cv_gate_mode_for_current_mode();
    void apply_clock_led_for_current_mode();
    void restore_note_leds();
    void render_cvgate_mode_transition(float progress_0_to_1);
    void render_synth_mode_transition(float progress_0_to_1);
    uint8_t note_to_output_note(uint8_t note) const;
    bool is_note_held(uint8_t note) const;
    bool add_held_note(uint8_t note);
    bool remove_held_note(uint8_t note);
    void apply_voice_outputs(uint8_t note);
    void release_voice_outputs();
    void refresh_active_voice_cv();
    void clear_voice_state();

    Mode mode_ = Mode::CvGate;
    uint8_t held_notes_[kMaxHeldNotes] = {};
    uint8_t held_count_ = 0;
    uint8_t last_note_ = 0;
    bool has_last_note_ = false;
    CRGB last_color_ = CRGB::Black;
    bool mode_transition_active_ = false;
    bool mode_transition_pending_ = false;
    uint32_t mode_transition_start_ms_ = 0;
};

void DefaultGadgetHandler::midi(const MidiEvent& event) {
    uint8_t note = 0;
    NoteEventAction action = NoteEventAction::NoteOff;

    if (event.type == MidiEventType::NoteOn) {
        note = event.data.note_on.note;
        action = (event.data.note_on.velocity != 0)
                     ? NoteEventAction::NoteOn
                     : NoteEventAction::NoteOff;
        logger_printf(
            "DefaultGadgetHandler NoteOn ch=%u note=%u vel=%u",
            static_cast<unsigned>(event.data.note_on.channel),
            static_cast<unsigned>(event.data.note_on.note),
            static_cast<unsigned>(event.data.note_on.velocity)
        );
    } else if (event.type == MidiEventType::NoteOff) {
        note = event.data.note_off.note;
        action = NoteEventAction::NoteOff;
        logger_printf(
            "DefaultGadgetHandler NoteOff ch=%u note=%u vel=%u",
            static_cast<unsigned>(event.data.note_off.channel),
            static_cast<unsigned>(event.data.note_off.note),
            static_cast<unsigned>(event.data.note_off.velocity)
        );
    } else {
        return;
    }

    if (action == NoteEventAction::NoteOn) {
        if (is_note_held(note)) {
            logger_printf(
                "DefaultGadgetHandler NoteOn ignored: note=%u already held",
                static_cast<unsigned>(note)
            );
            return;
        }
        if (!add_held_note(note)) {
            logger_printf(
                "DefaultGadgetHandler NoteOn ignored: held table full (note=%u)",
                static_cast<unsigned>(note)
            );
            return;
        }
        apply_voice_outputs(note);
        return;
    }

    if (action == NoteEventAction::NoteOff) {
        if (!remove_held_note(note)) {
            logger_printf(
                "DefaultGadgetHandler NoteOff ignored: note=%u not held",
                static_cast<unsigned>(note)
            );
            return;
        }
        if (held_count_ == 0) {
            release_voice_outputs();
        }
        // Otherwise: gates remain ON, CV holds the last NoteOn voltage,
        // gate LEDs keep the last NoteOn color.
    }
}

void DefaultGadgetHandler::press() {
    mode_ = (mode_ == Mode::CvGate) ? Mode::Synth : Mode::CvGate;
    apply_cv_gate_mode_for_current_mode();
    refresh_active_voice_cv();
    mode_transition_pending_ = true;
    logger_printf(
        "DefaultGadgetHandler: mode switched to %s",
        (mode_ == Mode::CvGate) ? "CvGate" : "Synth"
    );
}

void DefaultGadgetHandler::tick(float dt_sec, uint32_t now_ms) {
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
    if (mode_ == Mode::CvGate) {
        render_cvgate_mode_transition(progress);
    } else {
        render_synth_mode_transition(progress);
    }
}

void DefaultGadgetHandler::enter() {
    logger_printf("DefaultGadgetHandler: enter");
    mode_ = Mode::CvGate;
    clear_voice_state();
    mode_transition_active_ = false;
    mode_transition_pending_ = false;
    apply_cv_gate_mode_for_current_mode();
    apply_clock_led_for_current_mode();
}

void DefaultGadgetHandler::exit() {
    logger_printf("DefaultGadgetHandler: exit");
    mode_ = Mode::CvGate;
    clear_voice_state();
    mode_transition_active_ = false;
    mode_transition_pending_ = false;
    set_cv_mode(CvMode::Cv);
    reset_all_outputs();
    set_led_all(CRGB::Black);
}

void DefaultGadgetHandler::apply_cv_gate_mode_for_current_mode() {
    if (mode_ == Mode::Synth) {
        set_cv_mode(CvMode::Synth);
    } else {
        set_cv_mode(CvMode::Cv);
    }
}

void DefaultGadgetHandler::apply_clock_led_for_current_mode() {
    if (mode_ == Mode::Synth) {
        set_led_clock(CRGB::White);
    } else {
        set_led_clock(CRGB::Purple);
    }
}

void DefaultGadgetHandler::restore_note_leds() {
    set_led_all(CRGB::Black);
    if (held_count_ != 0 && has_last_note_) {
        for (uint8_t i = 0; i < GATE_COUNT; ++i) {
            set_led_gate(i, last_color_);
        }
    }
    apply_clock_led_for_current_mode();
}

void DefaultGadgetHandler::render_cvgate_mode_transition(float progress_0_to_1) {
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

void DefaultGadgetHandler::render_synth_mode_transition(float progress_0_to_1) {
    const float clamped =
        (progress_0_to_1 < 0.0f) ? 0.0f : ((progress_0_to_1 > 1.0f) ? 1.0f : progress_0_to_1);
    const float phase = clamped * (2.0f * 3.14159265359f * SYNTH_TRANSITION_HZ);
    const float sine_0_to_1 = 0.5f + 0.5f * sinf(phase);

    for (uint8_t i = 1; i < GATE_COUNT; ++i) {
        set_gate(i, false);
        set_cv_synth_note(i, false);
        set_led_gate(i, CRGB::Black);
    }
    set_clock(false);
    set_led_clock(CRGB::Black);

    const bool gate_on = sine_0_to_1 >= 0.5f;
    set_gate(0, gate_on);
    set_cv_synth_note(0, gate_on);
    const uint8_t brightness = static_cast<uint8_t>(sine_0_to_1 * 255.0f);
    set_led_gate(0, CRGB(brightness, brightness, brightness));
}

uint8_t DefaultGadgetHandler::note_to_output_note(uint8_t note) const {
    if (mode_ != Mode::Synth) {
        return note;
    }
    const uint16_t shifted = static_cast<uint16_t>(note) + static_cast<uint16_t>(SYNTH_OCTAVE_UP_SEMITONES);
    return (shifted > 127U) ? 127U : static_cast<uint8_t>(shifted);
}

bool DefaultGadgetHandler::is_note_held(uint8_t note) const {
    for (uint8_t i = 0; i < held_count_; ++i) {
        if (held_notes_[i] == note) {
            return true;
        }
    }
    return false;
}

bool DefaultGadgetHandler::add_held_note(uint8_t note) {
    if (held_count_ >= kMaxHeldNotes) {
        return false;
    }
    held_notes_[held_count_++] = note;
    return true;
}

bool DefaultGadgetHandler::remove_held_note(uint8_t note) {
    for (uint8_t i = 0; i < held_count_; ++i) {
        if (held_notes_[i] != note) {
            continue;
        }
        for (uint8_t j = static_cast<uint8_t>(i + 1); j < held_count_; ++j) {
            held_notes_[j - 1] = held_notes_[j];
        }
        --held_count_;
        held_notes_[held_count_] = 0;
        return true;
    }
    return false;
}

void DefaultGadgetHandler::apply_voice_outputs(uint8_t note) {
    const uint8_t output_note = note_to_output_note(note);
    const float volts = midi_note_to_volts(output_note);
    const CRGB color = note_to_color(note);

    last_note_ = note;
    has_last_note_ = true;
    last_color_ = color;

    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        (void)set_cv(i, volts);
        set_gate(i, true);
        set_cv_synth_note(i, true);
        set_led_gate(i, color);
    }
    set_clock(false);
}

void DefaultGadgetHandler::release_voice_outputs() {
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        set_gate(i, false);
        set_cv_synth_note(i, false);
        set_led_gate(i, CRGB::Black);
    }
    set_clock(false);
}

void DefaultGadgetHandler::refresh_active_voice_cv() {
    if (held_count_ > 0 && has_last_note_) {
        const uint8_t output_note = note_to_output_note(last_note_);
        const float volts = midi_note_to_volts(output_note);
        for (uint8_t i = 0; i < GATE_COUNT; ++i) {
            (void)set_cv(i, volts);
            set_gate(i, true);
            set_cv_synth_note(i, true);
        }
    } else {
        for (uint8_t i = 0; i < GATE_COUNT; ++i) {
            set_gate(i, false);
            set_cv_synth_note(i, false);
        }
    }
    set_clock(false);
}

void DefaultGadgetHandler::clear_voice_state() {
    for (uint8_t i = 0; i < kMaxHeldNotes; ++i) {
        held_notes_[i] = 0;
    }
    held_count_ = 0;
    last_note_ = 0;
    has_last_note_ = false;
    last_color_ = CRGB::Black;
}

DefaultGadgetHandler g_default_handler;
}  // namespace

GadgetHandler& default_gadget_handler_get() {
    return g_default_handler;
}

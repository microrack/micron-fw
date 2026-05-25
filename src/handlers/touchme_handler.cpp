#include "touchme_handler.h"

#include <Arduino.h>

#include "cv_gate.h"
#include "handler_utils.h"
#include "led.h"
#include "logger.h"

static constexpr const char* MANUFACTURER = "Playtronica";
static constexpr const char* PRODUCT = "TouchMe";

bool TouchMeHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
           match_trimmed(context.product_name, PRODUCT);
}

float TouchMeHandler::midi_note_to_volts(uint8_t note) {
    const float v = (static_cast<float>(note) - static_cast<float>(BASE_NOTE)) / 12.0f;
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 5.0f) {
        return 5.0f;
    }
    return v;
}

float TouchMeHandler::velocity_to_volts(uint8_t velocity) {
    const uint8_t v = (velocity > 127) ? 127 : velocity;
    return (static_cast<float>(v) / 127.0f) * 5.0f;
}

void TouchMeHandler::refresh_fast_led() {
    if (!touch_active_ || held_count_ == 0) {
        set_led_gate(FAST, CRGB::Black);
        return;
    }
    const int rel_note =
        static_cast<int>(last_fast_note_) - static_cast<int>(BASE_NOTE);
    CRGB color = rel_note_to_color(rel_note);
    if (fast_gate_pulse_active_) {
        color.nscale8(51);
    }
    set_led_gate(FAST, color);
}

void TouchMeHandler::refresh_random_led() {
    if (!touch_active_ || held_count_ == 0) {
        set_led_gate(RANDOM, CRGB::Black);
        return;
    }
    const uint8_t v =
        (last_random_velocity_ > 127) ? 127 : last_random_velocity_;
    const uint8_t scale = static_cast<uint8_t>(
        (static_cast<uint16_t>(v) * 255u) / 127u
    );
    CRGB color = CRGB::Yellow;
    color.nscale8(scale);
    set_led_gate(RANDOM, color);
}

void TouchMeHandler::apply_random_note(uint8_t velocity, uint32_t now_ms) {
    last_random_velocity_ = (velocity > 127) ? 127 : velocity;
    (void)set_cv(RANDOM, velocity_to_volts(last_random_velocity_));
    random_gate_pulse_active_ = true;
    random_gate_pulse_end_ms_ = now_ms + GATE_PULSE_MS;
    set_gate(RANDOM, true);
    refresh_random_led();
}

void TouchMeHandler::update_random_from_held_notes() {
    if (held_count_ == 0) {
        refresh_random_led();
        return;
    }
    uint8_t voice_velocity = 0;
    bool found = false;
    for (uint8_t i = 0; i < NOTE_COUNT; ++i) {
        if (!held_notes_[i]) {
            continue;
        }
        const uint8_t vel = held_velocities_[i];
        if (!found || vel >= voice_velocity) {
            voice_velocity = vel;
            found = true;
        }
    }
    if (!found) {
        refresh_random_led();
        return;
    }
    last_random_velocity_ = voice_velocity;
    (void)set_cv(RANDOM, velocity_to_volts(voice_velocity));
    refresh_random_led();
}

uint8_t TouchMeHandler::cc_quarter_from_normalized(uint8_t normalized) {
    const uint8_t v = (normalized > 127) ? 127 : normalized;
    if (v >= 127) {
        return CC_QUARTER_COUNT - 1;
    }
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(v) * CC_QUARTER_COUNT) / 128u
    );
}

void TouchMeHandler::refresh_slow_led() {
    if (!touch_active_ || !slow_cc_quarter_valid_) {
        set_led_gate(SLOW, CRGB::Black);
        return;
    }
    const int rel_note =
        static_cast<int>(last_slow_note_) - static_cast<int>(BASE_NOTE);
    CRGB color = rel_note_to_color(rel_note);
    if (slow_gate_pulse_active_) {
        color.nscale8(51);
    }
    set_led_gate(SLOW, color);
}

void TouchMeHandler::apply_slow_note(uint8_t note, uint32_t now_ms) {
    last_slow_note_ = note;
    (void)set_cv(SLOW, midi_note_to_volts(note));
    slow_gate_pulse_active_ = true;
    slow_gate_pulse_end_ms_ = now_ms + GATE_PULSE_MS;
    set_gate(SLOW, true);
    refresh_slow_led();
}

void TouchMeHandler::update_slow_from_cc(uint8_t normalized, uint32_t now_ms) {
    const uint8_t quarter = cc_quarter_from_normalized(normalized);
    if (!slow_cc_quarter_valid_) {
        slow_cc_quarter_valid_ = true;
        slow_cc_quarter_ = quarter;
        return;
    }
    if (quarter == slow_cc_quarter_) {
        return;
    }
    slow_cc_quarter_ = quarter;
    slow_zone_pending_ = true;
    slow_zone_trigger_at_ms_ = now_ms + SLOW_ZONE_DEBOUNCE_MS;
}

void TouchMeHandler::tick_slow_zone_pending(uint32_t now_ms) {
    if (!slow_zone_pending_ || !touch_active_) {
        return;
    }
    if (static_cast<int32_t>(now_ms - slow_zone_trigger_at_ms_) < 0) {
        return;
    }
    slow_zone_pending_ = false;
    apply_slow_note(last_touch_note_, now_ms);
}

void TouchMeHandler::apply_fast_note(uint8_t note, uint32_t now_ms) {
    last_fast_note_ = note;
    (void)set_cv(FAST, midi_note_to_volts(note));
    fast_gate_pulse_active_ = true;
    fast_gate_pulse_end_ms_ = now_ms + GATE_PULSE_MS;
    set_gate(FAST, true);
    refresh_fast_led();
}

void TouchMeHandler::update_fast_from_held_notes() {
    if (held_count_ == 0) {
        refresh_fast_led();
        return;
    }
    uint8_t voice_note = 0;
    bool found = false;
    for (uint8_t i = 0; i < NOTE_COUNT; ++i) {
        if (!held_notes_[i]) {
            continue;
        }
        if (!found || i > voice_note) {
            voice_note = i;
            found = true;
        }
    }
    if (!found) {
        refresh_fast_led();
        return;
    }
    last_fast_note_ = voice_note;
    (void)set_cv(FAST, midi_note_to_volts(voice_note));
    refresh_fast_led();
}

uint8_t TouchMeHandler::normalize_cc(uint8_t raw) {
    if (raw > 127) {
        raw = 127;
    }
    if (raw > cc_max_) {
        cc_max_ = raw;
    }
    if (cc_max_ == 0) {
        return 0;
    }
    const uint16_t scaled =
        (static_cast<uint16_t>(raw) * 127u + cc_max_ / 2) / cc_max_;
    return static_cast<uint8_t>(scaled > 127 ? 127 : scaled);
}

void TouchMeHandler::handle_touch_cc(uint8_t raw, uint32_t now_ms) {
    const uint8_t normalized = normalize_cc(raw);
    logger_printf("TouchMe CC %u", static_cast<unsigned>(normalized));
    last_touch_cc_ = normalized;
    if (touch_active_) {
        apply_cont_from_cc(normalized);
        update_slow_from_cc(normalized, now_ms);
    }
}

void TouchMeHandler::apply_cont_from_cc(uint8_t normalized) {
    last_touch_cc_ = normalized;
    const float volts =
        (static_cast<float>(last_touch_cc_) / 127.0f) * 5.0f;
    (void)set_cv(CONT, volts);
    const uint8_t brightness = static_cast<uint8_t>(
        10 + (static_cast<uint16_t>(last_touch_cc_) * 245) / 127
    );
    set_led_gate(CONT, CRGB(brightness, brightness, brightness));
}

void TouchMeHandler::touch_on_outputs() {
    slow_cc_quarter_valid_ = false;
    slow_cc_quarter_ = 0;
    slow_zone_pending_ = false;
    slow_zone_trigger_at_ms_ = 0;
    set_gate(CONT, true);
    apply_cont_from_cc(last_touch_cc_);
}

void TouchMeHandler::touch_off_outputs() {
    set_gate(CONT, false);
    (void)set_cv(CONT, 0.0f);
    set_led_gate(CONT, CRGB::Black);

    fast_gate_pulse_active_ = false;
    set_gate(FAST, false);
    (void)set_cv(FAST, 0.0f);
    set_led_gate(FAST, CRGB::Black);

    slow_gate_pulse_active_ = false;
    slow_cc_quarter_valid_ = false;
    slow_zone_pending_ = false;
    slow_zone_trigger_at_ms_ = 0;
    set_gate(SLOW, false);
    (void)set_cv(SLOW, 0.0f);
    set_led_gate(SLOW, CRGB::Black);

    random_gate_pulse_active_ = false;
    set_gate(RANDOM, false);
    (void)set_cv(RANDOM, 0.0f);
    set_led_gate(RANDOM, CRGB::Black);
}

void TouchMeHandler::reset_touch_state() {
    held_count_ = 0;
    for (uint8_t i = 0; i < NOTE_COUNT; ++i) {
        held_notes_[i] = false;
        held_velocities_[i] = 0;
    }
    touch_active_ = false;
    all_notes_off_since_ms_ = 0;
    last_fast_note_ = BASE_NOTE;
    fast_gate_pulse_active_ = false;
    fast_gate_pulse_end_ms_ = 0;
    last_slow_note_ = BASE_NOTE;
    slow_gate_pulse_active_ = false;
    slow_gate_pulse_end_ms_ = 0;
    slow_cc_quarter_valid_ = false;
    slow_cc_quarter_ = 0;
    slow_zone_pending_ = false;
    slow_zone_trigger_at_ms_ = 0;
    last_touch_note_ = BASE_NOTE;
    last_random_velocity_ = 0;
    random_gate_pulse_active_ = false;
    random_gate_pulse_end_ms_ = 0;
}

void TouchMeHandler::on_note_pressed(uint8_t note, uint8_t velocity, uint32_t now_ms) {
    if (note >= NOTE_COUNT || held_notes_[note]) {
        return;
    }
    const bool was_empty = (held_count_ == 0);
    held_notes_[note] = true;
    held_velocities_[note] = (velocity > 127) ? 127 : velocity;
    last_touch_note_ = note;
    ++held_count_;
    all_notes_off_since_ms_ = 0;
    if (was_empty && !touch_active_) {
        logger_printf("touch on");
        touch_active_ = true;
        touch_on_outputs();
    }
    if (touch_active_) {
        apply_fast_note(note, now_ms);
        apply_random_note(velocity, now_ms);
    }
}

void TouchMeHandler::on_note_released(uint8_t note, uint32_t now_ms) {
    if (note >= NOTE_COUNT || !held_notes_[note]) {
        return;
    }
    held_notes_[note] = false;
    held_velocities_[note] = 0;
    if (held_count_ == 0) {
        return;
    }
    --held_count_;
    if (touch_active_) {
        update_fast_from_held_notes();
        update_random_from_held_notes();
    }
    if (held_count_ == 0 && touch_active_) {
        all_notes_off_since_ms_ = now_ms;
    }
}

void TouchMeHandler::midi(const MidiEvent& event) {
    const uint32_t now_ms = millis();

    if (event.type == MidiEventType::NoteOff) {
        on_note_released(event.data.note_off.note, now_ms);
        return;
    }
    if (event.type == MidiEventType::NoteOn) {
        const uint8_t note = event.data.note_on.note;
        const uint8_t velocity = event.data.note_on.velocity;
        if (velocity == 0) {
            on_note_released(note, now_ms);
            return;
        }
        on_note_pressed(note, velocity, now_ms);
        logger_printf(
            "TouchMe note note=%u vel=%u",
            static_cast<unsigned>(note),
            static_cast<unsigned>(velocity)
        );
        return;
    }
    if (event.type == MidiEventType::ControlChange &&
        event.data.control_change.controller == TOUCH_CC) {
        handle_touch_cc(event.data.control_change.value, now_ms);
    }
}

void TouchMeHandler::press() {}

void TouchMeHandler::tick(float dt_sec, uint32_t now_ms) {
    (void)dt_sec;
    tick_slow_zone_pending(now_ms);
    if (fast_gate_pulse_active_ &&
        static_cast<int32_t>(now_ms - fast_gate_pulse_end_ms_) >= 0) {
        fast_gate_pulse_active_ = false;
        set_gate(FAST, false);
        refresh_fast_led();
    }
    if (slow_gate_pulse_active_ &&
        static_cast<int32_t>(now_ms - slow_gate_pulse_end_ms_) >= 0) {
        slow_gate_pulse_active_ = false;
        set_gate(SLOW, false);
        refresh_slow_led();
    }
    if (random_gate_pulse_active_ &&
        static_cast<int32_t>(now_ms - random_gate_pulse_end_ms_) >= 0) {
        random_gate_pulse_active_ = false;
        set_gate(RANDOM, false);
        refresh_random_led();
    }
    if (held_count_ != 0 || !touch_active_ || all_notes_off_since_ms_ == 0) {
        return;
    }
    if (static_cast<int32_t>(now_ms - all_notes_off_since_ms_) >=
        static_cast<int32_t>(TOUCH_OFF_DEBOUNCE_MS)) {
        logger_printf("touch off");
        touch_active_ = false;
        all_notes_off_since_ms_ = 0;
        touch_off_outputs();
    }
}

void TouchMeHandler::enter() {
    logger_printf("TouchMeHandler: enter");
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    last_touch_cc_ = 0;
    cc_max_ = DEFAULT_CC_MAX;
    reset_touch_state();
}

void TouchMeHandler::exit() {
    logger_printf("TouchMeHandler: exit");
    touch_off_outputs();
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    last_touch_cc_ = 0;
    cc_max_ = DEFAULT_CC_MAX;
    reset_touch_state();
}

TouchMeHandler g_touchme_handler;

GadgetHandler& touchme_handler_get() {
    return g_touchme_handler;
}

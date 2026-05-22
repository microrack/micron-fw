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
    const float v =
        1.0f + (static_cast<float>(note) - static_cast<float>(kBaseNote)) / 12.0f;
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 5.0f) {
        return 5.0f;
    }
    return v;
}

void TouchMeHandler::refresh_fast_led() {
    if (!touch_active_ || held_count_ == 0) {
        set_led_gate(FAST, CRGB::Black);
        return;
    }
    const int rel_note =
        static_cast<int>(last_fast_note_) - static_cast<int>(kBaseNote);
    CRGB color = rel_note_to_color(rel_note);
    if (fast_gate_pulse_active_) {
        color.nscale8(51);
    }
    set_led_gate(FAST, color);
}

void TouchMeHandler::apply_fast_note(uint8_t note, uint32_t now_ms) {
    last_fast_note_ = note;
    (void)set_cv(FAST, midi_note_to_volts(note));
    fast_gate_pulse_active_ = true;
    fast_gate_pulse_end_ms_ = now_ms + kFastGatePulseMs;
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
    for (uint8_t i = 0; i < kNoteCount; ++i) {
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

void TouchMeHandler::handle_touch_cc(uint8_t raw) {
    const uint8_t normalized = normalize_cc(raw);
    logger_printf("TouchMe CC %u", static_cast<unsigned>(normalized));
    last_touch_cc_ = normalized;
    if (touch_active_) {
        apply_cont_from_cc(normalized);
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
}

void TouchMeHandler::reset_touch_state() {
    held_count_ = 0;
    for (uint8_t i = 0; i < kNoteCount; ++i) {
        held_notes_[i] = false;
    }
    touch_active_ = false;
    all_notes_off_since_ms_ = 0;
    last_fast_note_ = kBaseNote;
    fast_gate_pulse_active_ = false;
    fast_gate_pulse_end_ms_ = 0;
}

void TouchMeHandler::on_note_pressed(uint8_t note, uint32_t now_ms) {
    if (note >= kNoteCount || held_notes_[note]) {
        return;
    }
    const bool was_empty = (held_count_ == 0);
    held_notes_[note] = true;
    ++held_count_;
    all_notes_off_since_ms_ = 0;
    if (was_empty && !touch_active_) {
        logger_printf("touch on");
        touch_active_ = true;
        touch_on_outputs();
    }
    if (touch_active_) {
        apply_fast_note(note, now_ms);
    }
}

void TouchMeHandler::on_note_released(uint8_t note, uint32_t now_ms) {
    if (note >= kNoteCount || !held_notes_[note]) {
        return;
    }
    held_notes_[note] = false;
    if (held_count_ == 0) {
        return;
    }
    --held_count_;
    if (touch_active_) {
        update_fast_from_held_notes();
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
        on_note_pressed(note, now_ms);
        logger_printf(
            "TouchMe note note=%u vel=%u",
            static_cast<unsigned>(note),
            static_cast<unsigned>(velocity)
        );
        return;
    }
    if (event.type == MidiEventType::ControlChange &&
        event.data.control_change.controller == TOUCH_CC) {
        handle_touch_cc(event.data.control_change.value);
    }
}

void TouchMeHandler::press() {}

void TouchMeHandler::tick(float dt_sec, uint32_t now_ms) {
    (void)dt_sec;
    if (fast_gate_pulse_active_ &&
        static_cast<int32_t>(now_ms - fast_gate_pulse_end_ms_) >= 0) {
        fast_gate_pulse_active_ = false;
        set_gate(FAST, false);
        refresh_fast_led();
    }
    if (held_count_ != 0 || !touch_active_ || all_notes_off_since_ms_ == 0) {
        return;
    }
    if (static_cast<int32_t>(now_ms - all_notes_off_since_ms_) >=
        static_cast<int32_t>(kTouchOffDebounceMs)) {
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
    cc_max_ = kDefaultCcMax;
    reset_touch_state();
}

void TouchMeHandler::exit() {
    logger_printf("TouchMeHandler: exit");
    touch_off_outputs();
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    last_touch_cc_ = 0;
    cc_max_ = kDefaultCcMax;
    reset_touch_state();
}

TouchMeHandler g_touchme_handler;

GadgetHandler& touchme_handler_get() {
    return g_touchme_handler;
}

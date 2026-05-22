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

void TouchMeHandler::apply_cont_from_cc(uint8_t value) {
    last_touch_cc_ = (value > 127) ? 127 : value;
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
}

void TouchMeHandler::reset_touch_state() {
    held_count_ = 0;
    for (uint8_t i = 0; i < kNoteCount; ++i) {
        held_notes_[i] = false;
    }
    touch_active_ = false;
    all_notes_off_since_ms_ = 0;
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
    (void)now_ms;
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
        const uint8_t value = event.data.control_change.value;
        logger_printf("TouchMe CC %u", static_cast<unsigned>(value));
        if (touch_active_) {
            apply_cont_from_cc(value);
        } else {
            last_touch_cc_ = (value > 127) ? 127 : value;
        }
    }
}

void TouchMeHandler::press() {}

void TouchMeHandler::tick(float dt_sec, uint32_t now_ms) {
    (void)dt_sec;
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
    reset_touch_state();
}

void TouchMeHandler::exit() {
    logger_printf("TouchMeHandler: exit");
    touch_off_outputs();
    set_cv_gate_mode(CvGateMode::CvGate);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    reset_touch_state();
}

TouchMeHandler g_touchme_handler;

GadgetHandler& touchme_handler_get() {
    return g_touchme_handler;
}

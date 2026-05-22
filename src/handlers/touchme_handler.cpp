#include "touchme_handler.h"

#include <Arduino.h>

#include "handler_utils.h"
#include "logger.h"

static constexpr const char* MANUFACTURER = "Playtronica";
static constexpr const char* PRODUCT = "TouchMe";
static constexpr uint8_t TOUCH_CC = 90;

bool TouchMeHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
           match_trimmed(context.product_name, PRODUCT);
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
        logger_printf(
            "TouchMe CC %u",
            static_cast<unsigned>(event.data.control_change.value)
        );
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
    }
}

void TouchMeHandler::enter() {
    logger_printf("TouchMeHandler: enter");
    reset_touch_state();
}

void TouchMeHandler::exit() {
    logger_printf("TouchMeHandler: exit");
    reset_touch_state();
}

TouchMeHandler g_touchme_handler;

GadgetHandler& touchme_handler_get() {
    return g_touchme_handler;
}

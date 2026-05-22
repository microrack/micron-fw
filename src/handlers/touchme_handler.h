#pragma once

#include <stdint.h>

#include "gadget_handler.h"

class TouchMeHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override;
    void midi(const MidiEvent& event) override;
    void press() override;
    void tick(float dt_sec, uint32_t now_ms) override;
    void enter() override;
    void exit() override;

   private:
    static constexpr uint8_t FAST = 0;
    static constexpr uint8_t SLOW = 1;
    static constexpr uint8_t CONT = 2;
    static constexpr uint8_t RANDOM = 3;

    static constexpr uint8_t kNoteCount = 128;
    static constexpr uint32_t kTouchOffDebounceMs = 10;

    void reset_touch_state();
    void on_note_pressed(uint8_t note, uint32_t now_ms);
    void on_note_released(uint8_t note, uint32_t now_ms);

    uint8_t held_count_ = 0;
    bool held_notes_[kNoteCount] = {};
    bool touch_active_ = false;
    uint32_t all_notes_off_since_ms_ = 0;
};

GadgetHandler& touchme_handler_get();

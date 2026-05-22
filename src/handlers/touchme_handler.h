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

    static constexpr uint8_t kBaseNote = 48;
    static constexpr uint8_t kNoteCount = 128;
    static constexpr uint32_t kTouchOffDebounceMs = 10;
    static constexpr uint32_t kFastGatePulseMs = 50;
    static constexpr uint8_t TOUCH_CC = 90;
    static constexpr uint8_t kDefaultCcMax = 40;

    void reset_touch_state();
    void on_note_pressed(uint8_t note, uint32_t now_ms);
    void on_note_released(uint8_t note, uint32_t now_ms);
    uint8_t normalize_cc(uint8_t raw);
    void handle_touch_cc(uint8_t raw);
    void apply_cont_from_cc(uint8_t normalized);
    void touch_on_outputs();
    void touch_off_outputs();
    void apply_fast_note(uint8_t note, uint32_t now_ms);
    void update_fast_from_held_notes();
    void refresh_fast_led();
    static float midi_note_to_volts(uint8_t note);

    uint8_t held_count_ = 0;
    bool held_notes_[kNoteCount] = {};
    bool touch_active_ = false;
    uint32_t all_notes_off_since_ms_ = 0;
    uint8_t last_touch_cc_ = 0;
    uint8_t cc_max_ = kDefaultCcMax;
    uint8_t last_fast_note_ = kBaseNote;
    bool fast_gate_pulse_active_ = false;
    uint32_t fast_gate_pulse_end_ms_ = 0;
};

GadgetHandler& touchme_handler_get();

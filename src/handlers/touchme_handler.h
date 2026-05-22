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

    static constexpr uint8_t BASE_NOTE = 48;
    static constexpr uint8_t NOTE_COUNT = 128;
    static constexpr uint32_t TOUCH_OFF_DEBOUNCE_MS = 10;
    static constexpr uint32_t GATE_PULSE_MS = 50;
    static constexpr uint8_t TOUCH_CC = 90;
    static constexpr uint8_t DEFAULT_CC_MAX = 40;
    static constexpr uint8_t CC_QUARTER_COUNT = 4;
    static constexpr uint32_t SLOW_ZONE_DEBOUNCE_MS = 10;

    void reset_touch_state();
    void on_note_pressed(uint8_t note, uint8_t velocity, uint32_t now_ms);
    void on_note_released(uint8_t note, uint32_t now_ms);
    uint8_t normalize_cc(uint8_t raw);
    void handle_touch_cc(uint8_t raw, uint32_t now_ms);
    void apply_cont_from_cc(uint8_t normalized);
    void touch_on_outputs();
    void touch_off_outputs();
    void apply_fast_note(uint8_t note, uint32_t now_ms);
    void apply_slow_note(uint8_t note, uint32_t now_ms);
    void apply_random_note(uint8_t velocity, uint32_t now_ms);
    void update_fast_from_held_notes();
    void update_random_from_held_notes();
    void update_slow_from_cc(uint8_t normalized, uint32_t now_ms);
    void tick_slow_zone_pending(uint32_t now_ms);
    void refresh_fast_led();
    void refresh_slow_led();
    void refresh_random_led();
    static uint8_t cc_quarter_from_normalized(uint8_t normalized);
    static float midi_note_to_volts(uint8_t note);
    static float velocity_to_volts(uint8_t velocity);

    uint8_t held_count_ = 0;
    bool held_notes_[NOTE_COUNT] = {};
    uint8_t held_velocities_[NOTE_COUNT] = {};
    bool touch_active_ = false;
    uint32_t all_notes_off_since_ms_ = 0;
    uint8_t last_touch_cc_ = 0;
    uint8_t cc_max_ = DEFAULT_CC_MAX;
    uint8_t last_fast_note_ = BASE_NOTE;
    bool fast_gate_pulse_active_ = false;
    uint32_t fast_gate_pulse_end_ms_ = 0;
    uint8_t last_slow_note_ = BASE_NOTE;
    bool slow_gate_pulse_active_ = false;
    uint32_t slow_gate_pulse_end_ms_ = 0;
    bool slow_cc_quarter_valid_ = false;
    uint8_t slow_cc_quarter_ = 0;
    bool slow_zone_pending_ = false;
    uint32_t slow_zone_trigger_at_ms_ = 0;
    uint8_t last_touch_note_ = BASE_NOTE;
    uint8_t last_random_velocity_ = 0;
    bool random_gate_pulse_active_ = false;
    uint32_t random_gate_pulse_end_ms_ = 0;
};

GadgetHandler& touchme_handler_get();

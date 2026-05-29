#pragma once

#include <stdint.h>

#include "gadget_handler.h"

class TouchMeHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override;
    void midi(const MidiEvent& event) override;
    void press() override;
    void tick(float dt_sec, uint32_t now_ms, const GadgetTickInputs& inputs) override;
    void enter() override;
    void exit() override;

   private:
    static constexpr uint8_t FAST = 0;
    static constexpr uint8_t SLOW = 1;
    static constexpr uint8_t CONT = 2;
    static constexpr uint8_t RANDOM_OSC = 3;

    static constexpr uint8_t BASE_NOTE = 48;
    static constexpr uint8_t NOTE_COUNT = 128;
    static constexpr uint32_t TOUCH_OFF_DEBOUNCE_MS = 10;
    static constexpr uint32_t GATE_PULSE_MS = 50;
    static constexpr uint8_t TOUCH_CC = 90;
    static constexpr uint8_t DEFAULT_CC_MAX = 40;
    static constexpr float CLOCK_BPM_DEFAULT = 120.0f;
    static constexpr float CLOCK_DUTY = 0.25f;
    static constexpr float TAP_TEMPO_FIRST_INTERVAL_SEC = 10.0f;
    static constexpr float TAP_TEMPO_RESET_THRESHOLD_SEC = 2.0f;
    static constexpr uint8_t TAP_TEMPO_INTERVAL_MAX = 8;
    static constexpr int SLOW_ZONE_MIN =
        (-static_cast<int>(BASE_NOTE)) / 4;
    static constexpr int SLOW_ZONE_MAX =
        (static_cast<int>(NOTE_COUNT) - 1 - static_cast<int>(BASE_NOTE)) / 4;
    static constexpr int SLOW_ZONE_COUNT =
        SLOW_ZONE_MAX - SLOW_ZONE_MIN + 1;

    void reset_touch_state();
    void reset_slow_zone_latches();
    int slow_pitch_for_zone(int zone, int rel_note);
    void on_note_pressed(int rel_note, uint8_t velocity, uint32_t now_ms);
    void on_note_released(int rel_note, uint32_t now_ms);
    static bool rel_note_index(int rel_note, uint8_t* index_out);
    uint8_t normalize_cc(uint8_t raw);
    void handle_touch_cc(uint8_t raw, uint32_t now_ms);
    void apply_cont_from_cc(uint8_t normalized);
    void touch_on_outputs();
    void touch_off_outputs();
    void apply_fast_note(int rel_note, uint32_t now_ms);
    void apply_random_note(uint8_t velocity, uint32_t now_ms);
    void set_fast_pitch(int rel_note);
    void set_slow_pitch(int rel_note);
    void pulse_fast_gate(uint32_t now_ms);
    void pulse_slow_gate(uint32_t now_ms);
    void update_held_pitch(int rel_note, uint32_t now_ms);
    void update_slow_zone(int rel_note, uint32_t now_ms, bool pulse_gate);
    int highest_held_rel_note() const;
    void update_random_from_held_notes();
    void refresh_fast_led();
    void refresh_slow_led();
    void refresh_random_led();
    void update_clock(float dt_sec);
    void on_tap_tempo_interval(float interval_sec);
    void reset_tap_tempo_state();
    static float median_tap_interval_sec(const float* intervals, uint8_t count);
    static float rel_note_to_volts(int rel_note);
    static float velocity_to_volts(uint8_t velocity);

    uint8_t held_count_ = 0;
    bool held_notes_[NOTE_COUNT] = {};
    uint8_t held_velocities_[NOTE_COUNT] = {};
    bool touch_active_ = false;
    uint32_t all_notes_off_since_ms_ = 0;
    uint8_t last_touch_cc_ = 0;
    uint8_t cc_max_ = DEFAULT_CC_MAX;
    int last_fast_note_ = 0;
    bool fast_gate_pulse_active_ = false;
    uint32_t fast_gate_pulse_end_ms_ = 0;
    int last_slow_note_ = 0;
    bool slow_gate_pulse_active_ = false;
    uint32_t slow_gate_pulse_end_ms_ = 0;
    bool slow_zone_valid_ = false;
    int last_slow_zone_ = 0;
    int slow_zone_pitch_[SLOW_ZONE_COUNT] = {};
    bool slow_zone_pitch_valid_[SLOW_ZONE_COUNT] = {};
    uint8_t last_random_velocity_ = 0;
    bool random_gate_pulse_active_ = false;
    uint32_t random_gate_pulse_end_ms_ = 0;
    float clock_bpm_ = CLOCK_BPM_DEFAULT;
    float clock_phase_sec_ = 0.0f;
    bool clock_high_ = false;
    uint32_t last_tap_press_ms_ = 0;
    float tap_intervals_[TAP_TEMPO_INTERVAL_MAX] = {};
    uint8_t tap_interval_count_ = 0;
};

GadgetHandler& touchme_handler_get();

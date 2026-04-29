#pragma once

#include "gadget_handler.h"
#include "led.h"

class OrbitaHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override;
    void midi(const MidiEvent& event) override;
    void press() override;
    void tick(float dt_sec, uint32_t now_ms) override;
    void enter() override;
    void exit() override;

   private:
    enum class OrbitaMode : uint8_t {
        Track,
        Common,
    };
    enum class NoteEventAction : uint8_t {
        NoteOn,
        NoteOff,
    };

    static constexpr uint8_t MIDI_GATE_CH_FIRST = 1;
    static constexpr uint8_t MIDI_GATE_CH_LAST = 4;
    static constexpr uint8_t ORBITA_MIDI_GATE_COUNT = MIDI_GATE_CH_LAST - MIDI_GATE_CH_FIRST + 1;
    static constexpr uint8_t ORBITA_LED_COUNT = 5;
    static constexpr uint32_t MODE_TRANSITION_MS = 1000;

    static bool midi_channel_to_gate_idx(uint8_t midi_ch_1_to_16, uint8_t* out_gate_idx);
    static uint8_t orbita_channel_offset(uint8_t channel_1_to_16);
    static int major_scale_steps_to_semitones(int steps);
    static float rel_major_steps_to_volts(int rel_note_degrees);
    static CRGB rel_note_to_color(int rel_note);
    static uint8_t lerp_u8(uint8_t a, uint8_t b, float t);
    static CRGB lerp_color(const CRGB& from, const CRGB& to, float t);
    static CRGB scale_color(const CRGB& color, float scale);

    bool mode_transition_in_progress() const;
    void restore_note_leds();
    void render_track_mode_transition(float progress_0_to_1);
    void render_common_mode_transition(float progress_0_to_1);
    void sync_gates_leds();
    void handle_common_mode_note_event(
        uint8_t midi_ch,
        uint8_t note,
        NoteEventAction action
    );
    void handle_track_mode_note_event(
        uint8_t midi_ch,
        uint8_t note,
        NoteEventAction action
    );
    void reset_outputs();

    OrbitaMode mode_ = OrbitaMode::Track;
    bool mode_transition_active_ = false;
    bool mode_transition_pending_ = false;
    uint32_t mode_transition_start_ms_ = 0;
    uint8_t held_notes_per_channel_[ORBITA_MIDI_GATE_COUNT] = {};
    uint8_t common_mode_held_notes_count_ = 0;
    CRGB led_color_per_channel_[ORBITA_MIDI_GATE_COUNT] = {};
};

GadgetHandler& orbita_handler_get();

#pragma once

#include "gadget_handler.h"
#include "led.h"

class PlaytronHandler : public GadgetHandler {
   public:
    enum class PlaytronMode : uint8_t {
        CvGate = 0,
        Synth = 1,
    };

    bool probe(const UsbDeviceContext& context) override;
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

    PlaytronMode mode_ = PlaytronMode::CvGate;
    uint8_t held_notes_[kMaxHeldNotes] = {};
    uint8_t held_count_ = 0;
    uint8_t last_note_ = 0;
    bool has_last_note_ = false;
    CRGB last_color_ = CRGB::Black;
    bool mode_transition_active_ = false;
    bool mode_transition_pending_ = false;
    uint32_t mode_transition_start_ms_ = 0;
};

GadgetHandler& playtron_handler_get();

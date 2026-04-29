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

    void apply_cv_gate_mode_for_current_mode();
    void restore_note_leds();
    void render_cvgate_mode_transition(float progress_0_to_1);
    void render_synth_mode_transition(float progress_0_to_1);
    uint8_t note_to_output_note(uint8_t note) const;
    bool find_free_channel(uint8_t* out_channel) const;
    bool find_channel_for_note(uint8_t note, uint8_t* out_channel) const;
    void refresh_active_channels_cv();
    CRGB current_clock_color() const;
    void clear_channel_state();

    PlaytronMode mode_ = PlaytronMode::CvGate;
    bool channel_note_active_[4] = {};
    uint8_t channel_note_[4] = {};
    CRGB channel_color_[4] = {};
    bool mode_transition_active_ = false;
    bool mode_transition_pending_ = false;
    uint32_t mode_transition_start_ms_ = 0;
};

GadgetHandler& playtron_handler_get();

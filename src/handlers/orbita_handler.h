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
    enum class NoteEventAction : uint8_t {
        NoteOn,
        NoteOff,
    };

    // Channel routing:
    //   ch 1 carries CC -> CV 0.
    //   ch 2..4 carry notes -> all of CV/Gate 1..3 in unison; the source
    //   channel only selects the octave shift and the highlighted LED.
    static constexpr uint8_t CC_CHANNEL = 1;
    static constexpr uint8_t NOTE_CH_FIRST = 2;
    static constexpr uint8_t NOTE_CH_LAST = 4;

    static constexpr uint8_t CV_GATE_COUNT = 4;
    static constexpr uint8_t CC_CV_IDX = 0;
    static constexpr uint8_t CC_CV0_CONTROLLER = 102;

    // CV/Gate outputs driven by notes (CV/Gate 0 is reserved for CC).
    static constexpr uint8_t NOTE_GATE_FIRST = 1;
    static constexpr uint8_t NOTE_GATE_LAST = 3;

    // Base MIDI note that maps to 1 V on the CV outputs; +1 V per octave above.
    static constexpr uint8_t BASE_NOTE = 48;

    static constexpr uint8_t MIDI_CLOCK_STATUS = 0xF8;
    static constexpr uint8_t MIDI_CLOCK_START_STATUS = 0xFA;
    static constexpr uint8_t MIDI_CLOCK_CONTINUE_STATUS = 0xFB;
    static constexpr uint8_t MIDI_CLOCK_STOP_STATUS = 0xFC;
    static constexpr uint32_t CLOCK_PULSE_MS = 50;
    // MIDI Clock is 24 PPQN. Divide by 4 -> 6 PPQN (one tick per 16th note).
    static constexpr uint8_t CLOCK_DIVIDER = 4;

    void handle_control_change(uint8_t controller, uint8_t value);
    void handle_note_event(uint8_t midi_ch, uint8_t note, NoteEventAction action);
    void handle_clock_tick(uint32_t now_ms);
    void reset_clock_divider();
    void reset_note_state();

    static float midi_note_to_volts(uint8_t note);
    static float cc_value_to_volts(uint8_t value);

    bool clock_pulse_active_ = false;
    uint32_t clock_pulse_end_ms_ = 0;
    uint8_t clock_divider_counter_ = 0;
    // Global held-notes counter drives the synchronous CV/Gate 1..3 outputs.
    uint8_t common_held_notes_count_ = 0;
    // Per-channel held-notes counters drive each note LED independently so
    // every gate LED reflects only its source channel's notes.
    uint8_t held_notes_per_channel_[CV_GATE_COUNT] = {};
};

GadgetHandler& orbita_handler_get();

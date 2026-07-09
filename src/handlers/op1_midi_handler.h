#pragma once

#include "gadget_handler.h"

class Op1MidiHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override;
    void midi(const MidiEvent& event) override;
    void press() override;
    void tick(float dt_sec, uint32_t now_ms, const GadgetTickInputs& inputs) override;
    void enter() override;
    void exit() override;

   private:
    static constexpr uint8_t CV_IDX = 0;
    static constexpr uint8_t CLOCK_DIVIDER = 12;
    static constexpr uint32_t CLOCK_PULSE_MS = 50;
    static constexpr float CLOCK_PULSE_VOLTS = 5.0f;
    static constexpr uint8_t MIDI_CLOCK_STATUS = 0xF8;
    static constexpr uint8_t MIDI_CLOCK_START_STATUS = 0xFA;
    static constexpr uint8_t MIDI_CLOCK_CONTINUE_STATUS = 0xFB;
    static constexpr uint8_t MIDI_CLOCK_STOP_STATUS = 0xFC;

    void handle_clock_tick(uint32_t now_ms);
    void start_clock_pulse(uint32_t now_ms);
    void stop_clock_pulse();
    void reset_clock_divider();

    bool clock_running_ = false;
    bool clock_pulse_active_ = false;
    uint32_t clock_pulse_end_ms_ = 0;
    uint8_t clock_divider_counter_ = 0;
};

GadgetHandler& op1_midi_handler_get();

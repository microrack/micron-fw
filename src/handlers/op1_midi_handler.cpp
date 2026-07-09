#include "op1_midi_handler.h"

#include <Arduino.h>

#include "cv_gate.h"
#include "handler_utils.h"
#include "led.h"
#include "logger.h"

static constexpr const char* MANUFACTURER = "Teenage Engineering AB";
static constexpr const char* PRODUCT      = "OP-1 Midi Device";

bool Op1MidiHandler::probe(const UsbDeviceContext& context) {
    return match_trimmed(context.manufacturer_name, MANUFACTURER) &&
           match_trimmed(context.product_name, PRODUCT);
}

void Op1MidiHandler::midi(const MidiEvent& event) {
    if (event.type != MidiEventType::SingleByte) {
        return;
    }

    const uint8_t status = event.data.single_byte.byte0;
    switch (status) {
        case MIDI_CLOCK_STATUS:
            if (!clock_running_) {
                break;
            }
            handle_clock_tick(millis());
            // logger_printf("OP-1 MIDI: clock");
            break;
        case MIDI_CLOCK_START_STATUS:
            clock_running_ = true;
            reset_clock_divider();
            start_clock_pulse(millis());
            logger_printf("OP-1 MIDI: start");
            break;
        case MIDI_CLOCK_CONTINUE_STATUS:
            clock_running_ = true;
            reset_clock_divider();
            start_clock_pulse(millis());
            logger_printf("OP-1 MIDI: continue");
            break;
        case MIDI_CLOCK_STOP_STATUS:
            clock_running_ = false;
            reset_clock_divider();
            stop_clock_pulse();
            logger_printf("OP-1 MIDI: stop");
            break;
        default:
            break;
    }
}

void Op1MidiHandler::press() {}

void Op1MidiHandler::tick(float dt_sec, uint32_t now_ms, const GadgetTickInputs& inputs) {
    (void)dt_sec;
    (void)inputs;

    if (!clock_pulse_active_) {
        return;
    }
    if (static_cast<int32_t>(now_ms - clock_pulse_end_ms_) >= 0) {
        stop_clock_pulse();
    }
}

void Op1MidiHandler::enter() {
    logger_printf("Op1MidiHandler: enter");
    set_cv_mode(CvMode::Cv);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    clock_running_ = false;
    clock_pulse_active_ = false;
    clock_pulse_end_ms_ = 0;
    clock_divider_counter_ = 0;
}

void Op1MidiHandler::exit() {
    logger_printf("Op1MidiHandler: exit");
    set_cv_mode(CvMode::Cv);
    reset_all_outputs();
    set_led_all(CRGB::Black);
    clock_running_ = false;
    clock_pulse_active_ = false;
    clock_pulse_end_ms_ = 0;
    clock_divider_counter_ = 0;
}

void Op1MidiHandler::handle_clock_tick(uint32_t now_ms) {
    if (++clock_divider_counter_ < CLOCK_DIVIDER) {
        return;
    }

    clock_divider_counter_ = 0;
    start_clock_pulse(now_ms);
}

void Op1MidiHandler::start_clock_pulse(uint32_t now_ms) {
    (void)set_cv(CV_IDX, CLOCK_PULSE_VOLTS);
    set_led_gate(CV_IDX, CRGB::White);
    clock_pulse_active_ = true;
    clock_pulse_end_ms_ = now_ms + CLOCK_PULSE_MS;
}

void Op1MidiHandler::stop_clock_pulse() {
    (void)set_cv(CV_IDX, 0.0f);
    set_led_gate(CV_IDX, CRGB::Black);
    clock_pulse_active_ = false;
    clock_pulse_end_ms_ = 0;
}

void Op1MidiHandler::reset_clock_divider() {
    clock_divider_counter_ = 0;
}

Op1MidiHandler g_op1_midi_handler;

GadgetHandler& op1_midi_handler_get() {
    return g_op1_midi_handler;
}

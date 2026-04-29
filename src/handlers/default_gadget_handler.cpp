#include "default_gadget_handler.h"

#include "cv_gate.h"
#include "led.h"
#include "logger.h"
#include "midi.h"

namespace {
// MIDI channels 1..4 -> gates 0..3.
static constexpr uint8_t GATE_CHANNEL_FIRST = 1;
static constexpr uint8_t GATE_CHANNEL_LAST = 4;
static uint8_t g_pressed_notes_per_channel[GATE_CHANNEL_LAST - GATE_CHANNEL_FIRST + 1] = {0};

static bool channel_maps_to_gate(uint8_t midi_channel_1_to_16, uint8_t* out_gate_idx) {
    if (midi_channel_1_to_16 < GATE_CHANNEL_FIRST || midi_channel_1_to_16 > GATE_CHANNEL_LAST) {
        return false;
    }
    *out_gate_idx = static_cast<uint8_t>(midi_channel_1_to_16 - GATE_CHANNEL_FIRST);
    return true;
}

constexpr uint8_t GATE_COUNT = 4;
constexpr float PRESS_VOLTAGE_STEPS[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
constexpr uint8_t CV_CHANNEL_COUNT = GATE_CHANNEL_LAST - GATE_CHANNEL_FIRST + 1;
static uint8_t g_press_voltage_step_idx = 0;

void update_led_gates_from_channel_counts() {
    for (uint8_t ch = GATE_CHANNEL_FIRST; ch <= GATE_CHANNEL_LAST; ++ch) {
        const uint8_t idx = static_cast<uint8_t>(ch - GATE_CHANNEL_FIRST);
        const bool on = g_pressed_notes_per_channel[idx] > 0;
        set_led_gate(idx, on ? CRGB::White : CRGB::Black);
        set_gate(idx, on);
    }
    set_led_clock(CRGB::Black);
    set_clock(false);
}

static void turn_all_gates_off() {
    for (uint8_t i = 0; i < (GATE_CHANNEL_LAST - GATE_CHANNEL_FIRST + 1); ++i) {
        g_pressed_notes_per_channel[i] = 0;
    }
    for (uint8_t i = 0; i < GATE_COUNT; ++i) {
        set_led_gate(i, CRGB::Black);
        set_gate(i, false);
    }
    set_led_clock(CRGB::Black);
    set_clock(false);
}

void handle_note_event(const MidiEvent& event) {
    uint8_t midi_channel = 0;
    bool note_on = false;
    bool note_off = false;

    if (event.type == MidiEventType::NoteOn) {
        midi_channel = event.data.note_on.channel;
        const uint8_t note = event.data.note_on.note;
        const uint8_t velocity = event.data.note_on.velocity;
        logger_printf(
            "DefaultGadgetHandler note: NoteOn ch=%u note=%u vel=%u",
            static_cast<unsigned>(midi_channel),
            static_cast<unsigned>(note),
            static_cast<unsigned>(velocity)
        );
        if (velocity != 0) {
            note_on = true;
        } else {
            note_off = true;
        }
    } else if (event.type == MidiEventType::NoteOff) {
        midi_channel = event.data.note_off.channel;
        logger_printf(
            "DefaultGadgetHandler note: NoteOff ch=%u note=%u vel=%u",
            static_cast<unsigned>(event.data.note_off.channel),
            static_cast<unsigned>(event.data.note_off.note),
            static_cast<unsigned>(event.data.note_off.velocity)
        );
        note_off = true;
    } else {
        return;
    }

    uint8_t gate_idx = 0;
    if (!channel_maps_to_gate(midi_channel, &gate_idx)) {
        return;
    }

    if (note_on) {
        if (g_pressed_notes_per_channel[gate_idx] < 255) {
            ++g_pressed_notes_per_channel[gate_idx];
        }
        update_led_gates_from_channel_counts();
        return;
    }

    if (note_off) {
        if (g_pressed_notes_per_channel[gate_idx] > 0) {
            --g_pressed_notes_per_channel[gate_idx];
        }
        update_led_gates_from_channel_counts();
    }
}

static void log_midi_event(const MidiEvent& event) {
    logger_printf("DefaultGadgetHandler MIDI: %s", midi_event_type_to_str(event.type));
}

class DefaultGadgetHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override {
        (void)context;
        return true;
    }

    void midi(const MidiEvent& event) override {
        handle_note_event(event);
        log_midi_event(event);
    }

    void press() override {
        const float voltage = PRESS_VOLTAGE_STEPS[g_press_voltage_step_idx];
        logger_printf(
            "DefaultGadgetHandler: button press, set all CV channels to %.1fV",
            static_cast<double>(voltage)
        );

        for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
            const bool ok = set_cv(i, voltage);
            logger_printf(
                "DefaultGadgetHandler: CV ch=%u voltage=%.1fV result=%s",
                static_cast<unsigned>(i),
                static_cast<double>(voltage),
                ok ? "ok" : "fail"
            );
        }

        g_press_voltage_step_idx =
            static_cast<uint8_t>((g_press_voltage_step_idx + 1) %
                                 (sizeof(PRESS_VOLTAGE_STEPS) / sizeof(PRESS_VOLTAGE_STEPS[0])));
    }

    void tick(float dt_sec, uint32_t now_ms) override {
        (void)dt_sec;
        (void)now_ms;
    }

    void enter() override { logger_printf("DefaultGadgetHandler: enter"); }

    void exit() override {
        logger_printf("DefaultGadgetHandler: exit");
        turn_all_gates_off();
        for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
            (void)set_cv(i, 0.0f);
        }
    }
};

DefaultGadgetHandler g_default_handler;
}  // namespace

GadgetHandler& default_gadget_handler_get() {
    return g_default_handler;
}

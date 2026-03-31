#include "midi.h"

#include "led.h"
#include "logger.h"

namespace {
static constexpr uint8_t LED_TRIGGER_COUNT = 5;
uint8_t g_pressed_notes_count = 0;

const char* midi_cin_to_type(uint8_t cin) {
    switch (cin) {
        case 0x8:
            return "NoteOff";
        case 0x9:
            return "NoteOn";
        case 0xA:
            return "PolyAftertouch";
        case 0xB:
            return "ControlChange";
        case 0xC:
            return "ProgramChange";
        case 0xD:
            return "ChannelAftertouch";
        case 0xE:
            return "PitchBend";
        case 0x2:
            return "SysCommon2B";
        case 0x3:
            return "SysCommon3B";
        case 0x4:
            return "SysExStartCont";
        case 0x5:
            return "SysExEnd1B";
        case 0x6:
            return "SysExEnd2B";
        case 0x7:
            return "SysExEnd3B";
        case 0xF:
            return "SingleByte";
        default:
            return "Unknown";
    }
}

void update_led_triggers_from_pressed_count() {
    for (uint8_t i = 0; i < LED_TRIGGER_COUNT; ++i) {
        set_led_trigger(i, i < g_pressed_notes_count);
    }
}

void handle_note_event(uint8_t cin, uint8_t velocity) {
    if (cin == 0x9 && velocity != 0) {
        if (g_pressed_notes_count < 255) {
            ++g_pressed_notes_count;
        }
        update_led_triggers_from_pressed_count();
        return;
    }

    if (cin == 0x8 || (cin == 0x9 && velocity == 0)) {
        if (g_pressed_notes_count > 0) {
            --g_pressed_notes_count;
        }
        update_led_triggers_from_pressed_count();
    }
}
}  // namespace

void midi_on_usb_packet(const uint8_t packet[4]) {
    if (packet == nullptr) {
        return;
    }

    const uint8_t cable = (packet[0] >> 4) & 0x0F;
    const uint8_t cin = packet[0] & 0x0F;
    handle_note_event(cin, packet[3]);
    logger_printf(
        "MIDI pkt: cable=%u cin=0x%X type=%s data=%02X %02X %02X",
        cable,
        cin,
        midi_cin_to_type(cin),
        packet[1],
        packet[2],
        packet[3]
    );
}

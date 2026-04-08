#include "midi.h"

#include "gate.h"
#include "logger.h"

namespace {
// MIDI каналы 1..4 -> гейты 0..3 (см. led: idx 0 = LED 8 .. idx 4 = LED 4).
static constexpr uint8_t GATE_CHANNEL_FIRST = 1;
static constexpr uint8_t GATE_CHANNEL_LAST = 4;
static uint8_t g_pressed_notes_per_channel[GATE_CHANNEL_LAST - GATE_CHANNEL_FIRST + 1] = {0};

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

static bool channel_maps_to_gate(uint8_t midi_channel_1_to_16, uint8_t* out_gate_idx) {
    if (midi_channel_1_to_16 < GATE_CHANNEL_FIRST || midi_channel_1_to_16 > GATE_CHANNEL_LAST) {
        return false;
    }
    *out_gate_idx = static_cast<uint8_t>(midi_channel_1_to_16 - GATE_CHANNEL_FIRST);
    return true;
}

void update_led_gates_from_channel_counts() {
    for (uint8_t ch = GATE_CHANNEL_FIRST; ch <= GATE_CHANNEL_LAST; ++ch) {
        const uint8_t idx = static_cast<uint8_t>(ch - GATE_CHANNEL_FIRST);
        set_gate(idx, g_pressed_notes_per_channel[idx] > 0);
    }
    set_gate(4, false);
}

void handle_note_event(uint8_t cin, uint8_t status_byte, uint8_t velocity) {
    const uint8_t msg_type = (status_byte >> 4) & 0x0F;
    if (msg_type != 0x8 && msg_type != 0x9) {
        return;
    }

    const uint8_t midi_channel = (status_byte & 0x0F) + 1;
    uint8_t gate_idx = 0;
    if (!channel_maps_to_gate(midi_channel, &gate_idx)) {
        return;
    }

    if (cin == 0x9 && velocity != 0) {
        if (g_pressed_notes_per_channel[gate_idx] < 255) {
            ++g_pressed_notes_per_channel[gate_idx];
        }
        update_led_gates_from_channel_counts();
        return;
    }

    if (cin == 0x8 || (cin == 0x9 && velocity == 0)) {
        if (g_pressed_notes_per_channel[gate_idx] > 0) {
            --g_pressed_notes_per_channel[gate_idx];
        }
        update_led_gates_from_channel_counts();
    }
}
}  // namespace

void midi_on_usb_packet(const uint8_t packet[4]) {
    if (packet == nullptr) {
        return;
    }

    const uint8_t cable = (packet[0] >> 4) & 0x0F;
    const uint8_t cin = packet[0] & 0x0F;
    handle_note_event(cin, packet[1], packet[3]);
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

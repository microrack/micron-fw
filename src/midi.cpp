#include "midi.h"

#include "logger.h"

namespace {
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
}  // namespace

void midi_on_usb_packet(const uint8_t packet[4]) {
    if (packet == nullptr) {
        return;
    }

    const uint8_t cable = (packet[0] >> 4) & 0x0F;
    const uint8_t cin = packet[0] & 0x0F;
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

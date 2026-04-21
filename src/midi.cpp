#include "midi.h"

#include "gadget_handler.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
}

struct MidiUsbRawPacket {
    uint8_t bytes[4];
};

static QueueHandle_t g_midi_usb_queue = nullptr;
static constexpr UBaseType_t MIDI_USB_QUEUE_LENGTH = 64;

static MidiEventType midi_event_type_from_cin(uint8_t cin) {
    switch (cin) {
        case 0x8:
            return MidiEventType::NoteOff;
        case 0x9:
            return MidiEventType::NoteOn;
        case 0xA:
            return MidiEventType::PolyAftertouch;
        case 0xB:
            return MidiEventType::ControlChange;
        case 0xC:
            return MidiEventType::ProgramChange;
        case 0xD:
            return MidiEventType::ChannelAftertouch;
        case 0xE:
            return MidiEventType::PitchBend;
        case 0x2:
            return MidiEventType::SysCommon2B;
        case 0x3:
            return MidiEventType::SysCommon3B;
        case 0x4:
            return MidiEventType::SysExStartCont;
        case 0x5:
            return MidiEventType::SysExEnd1B;
        case 0x6:
            return MidiEventType::SysExEnd2B;
        case 0x7:
            return MidiEventType::SysExEnd3B;
        case 0xF:
            return MidiEventType::SingleByte;
        default:
            return MidiEventType::Unknown;
    }
}

const char* midi_event_type_to_str(MidiEventType type) {
    switch (type) {
        case MidiEventType::NoteOff:
            return "NoteOff";
        case MidiEventType::NoteOn:
            return "NoteOn";
        case MidiEventType::PolyAftertouch:
            return "PolyAftertouch";
        case MidiEventType::ControlChange:
            return "ControlChange";
        case MidiEventType::ProgramChange:
            return "ProgramChange";
        case MidiEventType::ChannelAftertouch:
            return "ChannelAftertouch";
        case MidiEventType::PitchBend:
            return "PitchBend";
        case MidiEventType::SysCommon2B:
            return "SysCommon2B";
        case MidiEventType::SysCommon3B:
            return "SysCommon3B";
        case MidiEventType::SysExStartCont:
            return "SysExStartCont";
        case MidiEventType::SysExEnd1B:
            return "SysExEnd1B";
        case MidiEventType::SysExEnd2B:
            return "SysExEnd2B";
        case MidiEventType::SysExEnd3B:
            return "SysExEnd3B";
        case MidiEventType::SingleByte:
            return "SingleByte";
        case MidiEventType::Unknown:
        default:
            return "Unknown";
    }
}

MidiEvent midi_parse_usb_packet(const uint8_t packet[4]) {
    MidiEvent event = {};
    if (packet == nullptr) {
        return event;
    }

    const uint8_t cin = packet[0] & 0x0F;
    const uint8_t status = packet[1];
    const uint8_t data1 = packet[2];
    const uint8_t data2 = packet[3];
    const uint8_t channel = static_cast<uint8_t>((status & 0x0F) + 1);

    event.type = midi_event_type_from_cin(cin);

    switch (event.type) {
        case MidiEventType::NoteOn:
            event.data.note_on.channel = channel;
            event.data.note_on.note = data1;
            event.data.note_on.velocity = data2;
            break;
        case MidiEventType::NoteOff:
            event.data.note_off.channel = channel;
            event.data.note_off.note = data1;
            event.data.note_off.velocity = data2;
            break;
        case MidiEventType::PolyAftertouch:
            event.data.poly_aftertouch.channel = channel;
            event.data.poly_aftertouch.note = data1;
            event.data.poly_aftertouch.pressure = data2;
            break;
        case MidiEventType::ControlChange:
            event.data.control_change.channel = channel;
            event.data.control_change.controller = data1;
            event.data.control_change.value = data2;
            break;
        case MidiEventType::ProgramChange:
            event.data.program_change.channel = channel;
            event.data.program_change.program = data1;
            break;
        case MidiEventType::ChannelAftertouch:
            event.data.channel_aftertouch.channel = channel;
            event.data.channel_aftertouch.pressure = data1;
            break;
        case MidiEventType::PitchBend:
            event.data.pitch_bend.channel = channel;
            event.data.pitch_bend.value14 =
                static_cast<uint16_t>((static_cast<uint16_t>(data2) << 7) | (data1 & 0x7F));
            break;
        case MidiEventType::SysCommon2B:
            event.data.sys_common_2b.status = status;
            event.data.sys_common_2b.data1 = data1;
            break;
        case MidiEventType::SysCommon3B:
            event.data.sys_common_3b.status = status;
            event.data.sys_common_3b.data1 = data1;
            event.data.sys_common_3b.data2 = data2;
            break;
        case MidiEventType::SysExStartCont:
        case MidiEventType::SysExEnd3B:
            event.data.sysex_3b.byte0 = status;
            event.data.sysex_3b.byte1 = data1;
            event.data.sysex_3b.byte2 = data2;
            break;
        case MidiEventType::SysExEnd2B:
            event.data.sysex_2b.byte0 = status;
            event.data.sysex_2b.byte1 = data1;
            break;
        case MidiEventType::SysExEnd1B:
            event.data.sysex_1b.byte0 = status;
            break;
        case MidiEventType::SingleByte:
            event.data.single_byte.byte0 = status;
            break;
        case MidiEventType::Unknown:
        default:
            event.data.unknown.status = status;
            event.data.unknown.data1 = data1;
            event.data.unknown.data2 = data2;
            break;
    }

    return event;
}

void midi_input_init() {
    if (g_midi_usb_queue != nullptr) {
        return;
    }
    g_midi_usb_queue = xQueueCreate(MIDI_USB_QUEUE_LENGTH, sizeof(MidiUsbRawPacket));
}

void midi_input_poll() {
    if (g_midi_usb_queue == nullptr) {
        return;
    }
    MidiUsbRawPacket pkt = {};
    while (xQueueReceive(g_midi_usb_queue, &pkt, 0) == pdTRUE) {
        const MidiEvent event = midi_parse_usb_packet(pkt.bytes);
        gadget_handler_get().midi(event);
    }
}

bool midi_input_try_enqueue_usb_packet(const uint8_t packet[4]) {
    if (packet == nullptr || g_midi_usb_queue == nullptr) {
        return false;
    }
    MidiUsbRawPacket pkt = {};
    pkt.bytes[0] = packet[0];
    pkt.bytes[1] = packet[1];
    pkt.bytes[2] = packet[2];
    pkt.bytes[3] = packet[3];
    return xQueueSend(g_midi_usb_queue, &pkt, 0) == pdTRUE;
}

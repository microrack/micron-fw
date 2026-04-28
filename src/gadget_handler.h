#pragma once

#include <stddef.h>
#include <stdint.h>

struct UsbDeviceContext {
    uint16_t vid = 0;
    uint16_t pid = 0;
    const char* product_name = "";
    const char* manufacturer_name = "";
};

enum class MidiEventType : uint8_t {
    Unknown = 0,
    NoteOff,
    NoteOn,
    PolyAftertouch,
    ControlChange,
    ProgramChange,
    ChannelAftertouch,
    PitchBend,
    SysCommon2B,
    SysCommon3B,
    SysExStartCont,
    SysExEnd1B,
    SysExEnd2B,
    SysExEnd3B,
    SingleByte,
};

struct MidiEvent {
    MidiEventType type = MidiEventType::Unknown;

    /// Semantic payload for `type` only (no duplicate "raw" copy of the same fields).
    union Data {
        struct {
            uint8_t channel;  // 1..16
            uint8_t note;
            uint8_t velocity;
        } note_on;

        struct {
            uint8_t channel;  // 1..16
            uint8_t note;
            uint8_t velocity;
        } note_off;

        struct {
            uint8_t channel;  // 1..16
            uint8_t note;
            uint8_t pressure;
        } poly_aftertouch;

        struct {
            uint8_t channel;  // 1..16
            uint8_t controller;
            uint8_t value;
        } control_change;

        struct {
            uint8_t channel;  // 1..16
            uint8_t program;
        } program_change;

        struct {
            uint8_t channel;  // 1..16
            uint8_t pressure;
        } channel_aftertouch;

        struct {
            uint8_t channel;  // 1..16
            uint16_t value14;  // 14-bit value, LSB+MSB
        } pitch_bend;

        struct {
            uint8_t status;
            uint8_t data1;
        } sys_common_2b;

        struct {
            uint8_t status;
            uint8_t data1;
            uint8_t data2;
        } sys_common_3b;

        struct {
            uint8_t byte0;
            uint8_t byte1;
            uint8_t byte2;
        } sysex_3b;

        struct {
            uint8_t byte0;
            uint8_t byte1;
        } sysex_2b;

        struct {
            uint8_t byte0;
        } sysex_1b;

        struct {
            uint8_t byte0;
        } single_byte;

        /// Fallback: raw MIDI bytes from the USB-MIDI packet (status + data bytes).
        struct {
            uint8_t status;
            uint8_t data1;
            uint8_t data2;
        } unknown;
    } data = {};
};

class GadgetHandler {
   public:
    virtual ~GadgetHandler() = default;

    virtual bool probe(const UsbDeviceContext& context) = 0;
    virtual void midi(const MidiEvent& event) = 0;
    virtual void press() = 0;
    virtual void tick(float dt_sec, uint32_t now_ms) = 0;

    /// Called from the main loop once the USB gadget path is ready (MIDI stream up).
    virtual void enter() = 0;
    /// Called from the main loop when the USB device is gone; `current` is cleared after this.
    virtual void exit() = 0;
};

constexpr size_t GADGET_HANDLER_MAX_COUNT = 8;

bool gadget_handler_register(GadgetHandler* handler);
void gadget_handler_reset_registry();
size_t gadget_handler_count();
GadgetHandler* gadget_handler_at(size_t idx);

void gadget_handler_set_current(GadgetHandler* handler);
GadgetHandler* gadget_handler_current();
GadgetHandler& gadget_handler_get();

/// USB client task: device fully ready (after successful MIDI IN setup). Schedules `enter()` on the main loop.
void gadget_handler_on_usb_gadget_ready(GadgetHandler* handler);
/// USB client task: device removed or stream broken. Schedules `exit()` and clearing `current` on the main loop.
void gadget_handler_on_usb_disconnect();

/// Main loop: run pending `exit()` (and clear `current`), then pending `enter()` for the active handler.
void gadget_handler_poll_lifecycle();

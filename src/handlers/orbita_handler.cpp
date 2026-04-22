#include "orbita_handler.h"

#include <cstring>

#include "logger.h"
#include "midi.h"

namespace {

static constexpr const char* kManufacturer = "PLAYTRONICA";
static constexpr const char* kProduct      = "ORBITA DIY DANDELION";

// Compare s against expected, ignoring trailing ASCII spaces in s.
static bool match_trimmed(const char* s, const char* expected) {
    if (s == nullptr) {
        return false;
    }
    const size_t len = strlen(s);
    size_t trimmed = len;
    while (trimmed > 0 && s[trimmed - 1] == ' ') {
        --trimmed;
    }
    return strncmp(s, expected, trimmed) == 0 && expected[trimmed] == '\0';
}

class OrbitaHandler : public GadgetHandler {
   public:
    bool probe(const UsbDeviceContext& context) override {
        return match_trimmed(context.manufacturer_name, kManufacturer) &&
               match_trimmed(context.product_name, kProduct);
    }

    void midi(const MidiEvent& event) override {
        if (event.type == MidiEventType::NoteOn) {
            logger_printf(
                "Orbita NoteOn  ch=%u note=%u vel=%u",
                static_cast<unsigned>(event.data.note_on.channel),
                static_cast<unsigned>(event.data.note_on.note),
                static_cast<unsigned>(event.data.note_on.velocity)
            );
        }
    }

    void tick(float dt_sec, uint32_t now_ms) override {
        (void)dt_sec;
        (void)now_ms;
    }

    void enter() override {
        logger_printf("OrbitaHandler: enter");

        // Note length (CC 24..27 = 10)
        for (uint8_t cc = 24; cc <= 27; ++cc) {
            midi_send_cc(1, cc, 10);
        }
        // Velocity (CC 28..31 = 127)
        for (uint8_t cc = 28; cc <= 31; ++cc) {
            midi_send_cc(1, cc, 127);
        }
        // Track MIDI channel (CC 32..35 = 1..4)
        for (uint8_t i = 0; i < 4; ++i) {
            midi_send_cc(1, static_cast<uint8_t>(32 + i), static_cast<uint8_t>(1 + i));
        }
        // Probability (CC 42..45 = 127)
        for (uint8_t cc = 42; cc <= 45; ++cc) {
            midi_send_cc(1, cc, 127);
        }
        // Range (CC 46..49 = 0)
        for (uint8_t cc = 46; cc <= 49; ++cc) {
            midi_send_cc(1, cc, 0);
        }
    }
    void exit()  override { logger_printf("OrbitaHandler: exit");  }
};

OrbitaHandler g_orbita_handler;
}  // namespace

GadgetHandler& orbita_handler_get() {
    return g_orbita_handler;
}

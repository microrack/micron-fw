#pragma once

#include <stdint.h>

#include "gadget_handler.h"

const char* midi_event_type_to_str(MidiEventType type);
MidiEvent midi_parse_usb_packet(const uint8_t packet[4]);

void midi_input_init();
void midi_input_poll();
bool midi_input_try_enqueue_usb_packet(const uint8_t packet[4]);

bool midi_send_cc(uint8_t channel_1_to_16, uint8_t controller, uint8_t value);

#pragma once

#include <Arduino.h>
#include <stddef.h>

struct UsbHostConfig {};

void usb_host_init(const UsbHostConfig& config);

// Send a raw USB MIDI packet over the MIDI OUT bulk endpoint.
// Returns false if no device is connected or the OUT endpoint is unavailable.
bool usb_midi_send_packet(const uint8_t* data, size_t size);

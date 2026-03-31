#pragma once

#include <Arduino.h>

struct UsbHostConfig {
    void (*stack_message_cb)(const char* message);
};

void usb_host_init(const UsbHostConfig& config);

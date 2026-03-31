#pragma once

#include <Arduino.h>

struct UsbHostConfig {};

void usb_host_init(const UsbHostConfig& config);

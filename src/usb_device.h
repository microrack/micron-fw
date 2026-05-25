#pragma once

#include <stddef.h>
#include <stdint.h>

void usb_device_init();
bool usb_device_ready();
void usb_device_write(const char* message);

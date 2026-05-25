#include "usb_device.h"

#if __has_include("app_config.h")
#include "app_config.h"
#endif
#ifndef APP_USB_DEVICE
#define APP_USB_DEVICE 0
#endif

#if APP_USB_DEVICE

#include <Arduino.h>

#include "USB.h"
#include "USBCDC.h"

namespace {
USBCDC g_usb_serial;
constexpr uint32_t USB_SERIAL_BAUD = 921600;
constexpr uint32_t USB_HOST_WAIT_MS = 3000;
}  // namespace

void usb_device_init() {
    g_usb_serial.begin(USB_SERIAL_BAUD);
    USB.begin();
    const uint32_t deadline_ms = millis() + USB_HOST_WAIT_MS;
    while (!g_usb_serial && millis() < deadline_ms) {
        delay(10);
    }
}

bool usb_device_ready() {
    return static_cast<bool>(g_usb_serial);
}

void usb_device_write(const char* message) {
    if (message == nullptr || !g_usb_serial) {
        return;
    }
    g_usb_serial.println(message);
}

#else

void usb_device_init() {}
bool usb_device_ready() { return false; }
void usb_device_write(const char* message) { (void)message; }

#endif

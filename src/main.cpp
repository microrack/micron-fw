#include <Arduino.h>

#include "board.h"
#include "led.h"
#include "usb_host.h"

static void usb_stack_message_cb(const char* message) {
    (void)message;
}

static void check_boot_mode_pin() {
    pinMode(TOUCH_PIN, INPUT);
    init_led();
    set_led_mode(LedMode::All);
    handle_led();
    delay(500);

    if (digitalRead(TOUCH_PIN) == 1) {
        set_led_mode(LedMode::Boot);
        while (1) {
            handle_led();
        }
    }
}

void setup() {
    // check_boot_mode_pin MUST be called first
    check_boot_mode_pin();

    set_led_mode(LedMode::Manual);
    const UsbHostConfig usb_host_config = {
        .stack_message_cb = usb_stack_message_cb,
    };
    usb_host_init(usb_host_config);
}

void loop() {
    const int touch_state = digitalRead(TOUCH_PIN);
    if (touch_state == 0) {
        set_led(0, false);
    } else {
        set_led(0, true);
    }
    handle_led();
}

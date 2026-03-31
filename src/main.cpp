#include <Arduino.h>

#include "board.h"
#include "led.h"
#include "net.h"
#include "usb_host.h"

static void usb_stack_message_cb(const char* message) {
    send_message(message);
}

static void check_boot_mode_pin() {
    pinMode(TOUCH_PIN, INPUT);
    init_led();
    set_led_mode(LedMode::All);
    handle_led();
    delay(1000);

    if (digitalRead(TOUCH_PIN) == 1) {
        set_led_mode(LedMode::Boot);
        while (1) {
            handle_led();
        }
    }
}
void setup() {
    // check boot mode pin must be called first
    check_boot_mode_pin();

    set_led_mode(LedMode::Manual);
    net_init();
    const UsbHostConfig usb_host_config = {
        .stack_message_cb = usb_stack_message_cb,
    };
    usb_host_init(usb_host_config);
}

// loop must contain only unblocking operations except short I/O operations
void loop() {
    handle_net();

    const int touch_state = digitalRead(TOUCH_PIN);
    if (touch_state == 0) {
        set_led(0, false);
    } else {
        set_led(0, true);
    }
    handle_led();

    static uint32_t last_send_ms = 0;
    const uint32_t now_ms = millis();
    if (now_ms - last_send_ms >= 1000) {
        last_send_ms = now_ms;
        send_message("hello");
    }
}

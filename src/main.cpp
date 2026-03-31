#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "led.h"
#include "net.h"
#include "usb_host.h"

static AppConfig g_app_config = {
    .usb = false,
    .wifi = false,
};

static void usb_stack_message_cb(const char* message) {
    send_message(message);
}

static void check_boot_mode_pin() {
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
    pinMode(TOUCH_PIN, INPUT);
    init_led();
    g_app_config = config_init();

    if (g_app_config.usb) {
        check_boot_mode_pin();
        
        const UsbHostConfig usb_host_config = {
            .stack_message_cb = usb_stack_message_cb,
        };
        usb_host_init(usb_host_config);
    }
    
    set_led_mode(LedMode::Manual);
    if (g_app_config.wifi) {
        net_init();
    }
    
}

// loop must contain only unblocking operations except short I/O operations
void loop() {
    if (g_app_config.wifi) {
        handle_net();
    }

    const int touch_state = digitalRead(TOUCH_PIN);
    if (touch_state == 0) {
        set_led(0, false);
    } else {
        set_led(0, true);
    }
    handle_led();

    static uint32_t last_send_ms = 0;
    const uint32_t now_ms = millis();
    if (g_app_config.wifi && now_ms - last_send_ms >= 1000) {
        last_send_ms = now_ms;
        send_message("hello");
    }
}

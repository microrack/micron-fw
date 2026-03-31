#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "led.h"
#include "logger.h"
#include "net.h"
#include "usb_host.h"

static AppConfig g_app_config = {
    .usb = false,
    .wifi = false,
};

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
    logger_init();
    g_app_config = config_init();

    if (g_app_config.usb) {
        check_boot_mode_pin();
        const UsbHostConfig usb_host_config = {};
        usb_host_init(usb_host_config);
    }
    
    net_init(g_app_config);
    
}

// loop must contain only unblocking operations except short I/O operations
void loop() {
    handle_net();

    switch (net_get_state()) {
        case NetState::Connecting:
            set_led_mode(LedMode::Connecting);
            break;
        case NetState::Ap:
            set_led_mode(LedMode::Ap);
            break;
        case NetState::Client:
            set_led_mode(LedMode::Connected);
            break;
    }

    static uint32_t last_send_ms = 0;
    const uint32_t now_ms = millis();
    if (g_app_config.wifi && now_ms - last_send_ms >= 1000) {
        last_send_ms = now_ms;
        log_printf("hello");
    }

    if (g_app_config.wifi) {
        char log_message[160];
        while (logger_get_next(log_message, sizeof(log_message))) {
            send_message(log_message);
        }
    }

    handle_led();
}

#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "led.h"
#include "logger.h"
#include "net.h"
#include "ota.h"
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
    ota_init();
    g_app_config = config_init();

    logger_printf("app config: usb: %d, wifi: %d, ssid: %s\n",
        g_app_config.usb, g_app_config.wifi, g_app_config.ssid);

    if (g_app_config.usb) {
        check_boot_mode_pin();
        const UsbHostConfig usb_host_config = {};
        usb_host_init(usb_host_config);
    }
    
    net_init(g_app_config);
    
}

// loop must contain only unblocking operations except short I/O operations
void loop() {
    static int prev_touch_state = 0;
    const int touch_state = digitalRead(TOUCH_PIN);
    const bool pressed_edge = (prev_touch_state == 0) && (touch_state == 1);
    prev_touch_state = touch_state;

    if (pressed_edge && g_app_config.wifi) {
        const NetState state = net_get_state();
        if (state == NetState::Ap) {
            net_start_client();
        } else {
            net_start_ap();
        }
    }

    handle_net();
    ota_handle(net_get_state(), g_app_config.wifi);

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
    
    handle_led();
}

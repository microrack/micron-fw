#include <Arduino.h>

#include "board.h"
#include "config.h"
#include "gate.h"
#include "led.h"
#include "logger.h"
#include "mcp4728.h"
#include "net.h"
#include "ota.h"
#include "usb_host.h"

static AppConfig g_app_config = {
    .usb = false,
    .wifi = false,
};

namespace {
constexpr uint8_t GATE_DAC_CHANNEL_COUNT = 4;
constexpr uint16_t DAC_FULL = 0xFFF;
constexpr uint32_t BLINK_ON_MS = 100;
constexpr uint32_t BLINK_CYCLE_MS = 500;  // 100 ms on + 400 ms off
}  // namespace

void set_gate(uint8_t idx, bool state) {
    set_led_gate(idx, state);
    if (idx < GATE_DAC_CHANNEL_COUNT) {
        mcp4728_write_channel(idx, state ? DAC_FULL : 0);
    }
}

static void check_boot_mode_pin() {
    set_led_mode(LedMode::PreBoot);
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
    pinMode(BLINK_PIN, OUTPUT);
    init_mcp4728();
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

    set_led_mode(LedMode::Normal);
    net_init(g_app_config);
}

// loop must contain only unblocking operations except short I/O operations
void loop() {
    const uint32_t blink_phase = millis() % BLINK_CYCLE_MS;
    digitalWrite(BLINK_PIN, blink_phase < BLINK_ON_MS ? HIGH : LOW);

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
            set_led_net(LedNet::Connecting);
            break;
        case NetState::Ap:
            set_led_net(LedNet::Ap);
            break;
        case NetState::Client:
            set_led_net(LedNet::Connected);
            break;
    }

    handle_led();
}

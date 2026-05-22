#include <Arduino.h>

#include "board.h"

#if __has_include("app_config.h")
#include "app_config.h"
#endif
#ifndef APP_WIFI_ENABLED
#define APP_WIFI_ENABLED 0
#endif
#include "button.h"
#include "handlers/default_gadget_handler.h"
#include "handlers/orbita_handler.h"
#include "handlers/playtron_handler.h"
#include "handlers/touchme_handler.h"
#include "cv_gate.h"
#include "gadget_handler.h"
#include "led.h"
#include "logger.h"
#include "midi.h"
#include "net.h"
#include "ota.h"
#include "profiling.h"
#include "usb_host.h"

static void check_boot_mode_pin() {
    set_led_mode(LedMode::PreBoot);
    handle_led();
    delay(1000);

    if (button_read_raw()) {
        set_led_mode(LedMode::Boot);
        while (1) {
            handle_led();
        }
    }
}

void setup() {
    board_pins_init();

    button_init();
    init_cv_gate();
    init_led();
    logger_init();
    profiling_init();
    ota_init();

    logger_printf("app: wifi: %d, hw: %s\n", APP_WIFI_ENABLED, board_pins()->name);

    gadget_handler_reset_registry();
    gadget_handler_set_current(nullptr);
    (void)gadget_handler_register(&orbita_handler_get());
    (void)gadget_handler_register(&playtron_handler_get());
    (void)gadget_handler_register(&touchme_handler_get());
    (void)gadget_handler_register(&default_gadget_handler_get());
    midi_input_init();

    check_boot_mode_pin();
    const UsbHostConfig usb_host_config = {};
    usb_host_init(usb_host_config);

    set_led_mode(LedMode::Normal);
    net_init();
}

// Short delay at end of each iteration reduces CPU load and yields to the scheduler.
void loop() {
    LOOP_PROFILE(LoopProfileSlot::ProfilingTick, profiling_tick());
    LOOP_PROFILE(LoopProfileSlot::PollLifecycle, gadget_handler_poll_lifecycle());

    static uint32_t prev_tick_ms = millis();
    const uint32_t now_ms = millis();
    const float dt_sec = static_cast<float>(now_ms - prev_tick_ms) / 1000.0f;
    prev_tick_ms = now_ms;
    LOOP_PROFILE(LoopProfileSlot::MidiPoll, midi_input_poll());
    LOOP_PROFILE(LoopProfileSlot::Tick, gadget_handler_get().tick(dt_sec, now_ms));

    const ButtonEvent button_event = button_handle();

    if (button_event == ButtonEvent::Pressed) {
        gadget_handler_get().press();
    }

    if (button_event == ButtonEvent::Hold && APP_WIFI_ENABLED) {
        const NetState state = net_get_state();
        if (state == NetState::Ap) {
            net_start_client();
        } else {
            net_start_ap();
        }
    }

    LOOP_PROFILE(LoopProfileSlot::HandleNet, handle_net());
    LOOP_PROFILE(LoopProfileSlot::OtaHandle, ota_handle(net_get_state(), APP_WIFI_ENABLED));

    if (APP_WIFI_ENABLED) {
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
    }

    LOOP_PROFILE(LoopProfileSlot::HandleLed, handle_led());

    delay(1);
}

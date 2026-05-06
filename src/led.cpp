#include <Arduino.h>
#include <FastLED.h>

#include "board.h"
#include "led.h"

static constexpr uint8_t LED_BRIGHTNESS = 96;
static constexpr uint16_t STEP_MS = 50;
static constexpr uint16_t CONNECTING_BLINK_MS = 150;
static constexpr uint32_t NET_AP_CONNECTED_SPLASH_MS = 1000;
static constexpr uint32_t SHOW_MIN_INTERVAL_US = 10000;
static constexpr uint8_t GATE_COUNT = 4;

static CRGB leds[BOARD_LED_COUNT_MAX];
static LedMode led_mode = LedMode::Boot;
static LedNet led_net = LedNet::Connecting;
static CRGB gate_colors[GATE_COUNT]{};
static CRGB clock_color = CRGB::Black;
static uint32_t mode_last_ms = 0;
static bool connecting_led_on = false;
static uint8_t boot_color_index = 0;
static uint32_t last_show_us = 0;
static uint32_t net_ap_connected_splash_start_ms = 0;

static void show_leds_throttled() {
    const uint32_t t = micros();
    if (last_show_us != 0 && (t - last_show_us) < SHOW_MIN_INTERVAL_US) {
        return;
    }
    FastLED.show();
    last_show_us = t;
}

// Cycle through base colors on each LED in turn (STEP_MS per step).
static constexpr uint8_t COLOR_COUNT = 7;
static const CRGB COLORS[COLOR_COUNT] = {
    CRGB::Red,
    CRGB::Green,
    CRGB::Blue,
    CRGB::Yellow,
    CRGB::Cyan,
    CRGB::Magenta,
    CRGB::White,
};

void set_led_mode(LedMode mode) {
    if (led_mode == mode) {
        return;
    }

    led_mode = mode;
    switch (led_mode) {
        case LedMode::Boot:
            mode_last_ms = 0;
            connecting_led_on = false;
            boot_color_index = 0;
            break;
        case LedMode::PreBoot:
            mode_last_ms = 0;
            connecting_led_on = false;
            break;
        case LedMode::Normal:
            mode_last_ms = 0;
            connecting_led_on = false;
            break;
        default:
            break;
    }
}

void set_led_net(LedNet net) {
    if (led_net == net) {
        return;
    }

    led_net = net;
    if (led_net == LedNet::Connecting) {
        mode_last_ms = 0;
        connecting_led_on = false;
    } else if (led_net == LedNet::Ap || led_net == LedNet::Connected) {
        net_ap_connected_splash_start_ms = millis();
    }
}

void set_led_gate(uint8_t idx, CRGB color) {
    if (idx >= GATE_COUNT) {
        return;
    }
    gate_colors[idx] = color;
}

void set_led_clock(CRGB color) {
    clock_color = color;
}

void set_led_all(CRGB color) {
    for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
        gate_colors[idx] = color;
    }
    clock_color = color;
}

static void add_leds_for_pin(uint8_t data_pin) {
    switch (data_pin) {
        case 18:
            FastLED.addLeds<WS2812, 18, GRB>(
                leds, BOARD_LED_COUNT_MAX);
            break;
        case 37:
            FastLED.addLeds<WS2812, 37, GRB>(
                leds, BOARD_LED_COUNT_MAX);
            break;
        default:
            FastLED.addLeds<WS2812, 37, GRB>(
                leds, BOARD_LED_COUNT_MAX);
            break;
    }
}

void init_led() {
    add_leds_for_pin(board_pins()->board_led_pin);
    FastLED.setBrightness(LED_BRIGHTNESS);
    fill_solid(leds, board_pins()->board_led_count, CRGB::Black);
    FastLED.show();
    last_show_us = micros();
}

void handle_led() {
    const uint32_t now = millis();
    const BoardPinsProfile* pins = board_pins();
    const uint16_t n_led = board_pins()->board_led_count;

    switch (led_mode) {
        case LedMode::Boot: {
            if (now - mode_last_ms < STEP_MS) {
                return;
            }
            mode_last_ms = now;
            connecting_led_on = !connecting_led_on;
            if (connecting_led_on) {
                boot_color_index = static_cast<uint8_t>((boot_color_index + 1) % COLOR_COUNT);
            }

            fill_solid(leds, n_led, CRGB::Black);
            if (connecting_led_on) {
                for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
                    const uint8_t led_idx = pins->gate_led_indices[idx];
                    if (led_idx < n_led) {
                        leds[led_idx] = COLORS[boot_color_index];
                    }
                }
            }
            break;
        }

        case LedMode::PreBoot: {
            if (now - mode_last_ms >= CONNECTING_BLINK_MS) {
                mode_last_ms = now;
                connecting_led_on = !connecting_led_on;
            }
            fill_solid(leds, n_led, CRGB::Black);
            if (connecting_led_on) {
                for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
                    const uint8_t led_idx = pins->gate_led_indices[idx];
                    if (led_idx < n_led) {
                        leds[led_idx] = CRGB::White;
                    }
                }
            }
            break;
        }

        case LedMode::Normal: {
            fill_solid(leds, n_led, CRGB::Black);

            const bool net_status_phase =
                (led_net == LedNet::Connecting) ||
                ((led_net == LedNet::Ap || led_net == LedNet::Connected) &&
                 (now - net_ap_connected_splash_start_ms) < NET_AP_CONNECTED_SPLASH_MS);

            if (net_status_phase) {
                if (now - mode_last_ms >= CONNECTING_BLINK_MS) {
                    mode_last_ms = now;
                    connecting_led_on = !connecting_led_on;
                }
                if (pins->clock_led_index < n_led && connecting_led_on) {
                    leds[pins->clock_led_index] =
                        (led_net == LedNet::Ap) ? CRGB::Red : CRGB::Green;
                }
            } else {
                for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
                    const uint8_t led_idx = pins->gate_led_indices[idx];
                    if (led_idx < n_led) {
                        leds[led_idx] = gate_colors[idx];
                    }
                }
                if (pins->clock_led_index < n_led) {
                    leds[pins->clock_led_index] = clock_color;
                }
            }
            break;
        }
    }

    show_leds_throttled();
}

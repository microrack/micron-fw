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
static constexpr uint8_t CLOCK_INDEX = 4;
// Only LEDs 4..8 are driven; 0..3 stay off. In Normal mode, net uses LED 4 only
// while showing net status; gate view uses the usual map (incl. LED 4 for gate 4).
static constexpr uint8_t LED_ACTIVE_FIRST = 4;
static constexpr uint8_t LED_ACTIVE_COUNT = 5;
static constexpr uint8_t LED_NET_INDICATOR = 4;

static CRGB leds[BOARD_LED_COUNT];
static LedMode led_mode = LedMode::Boot;
static LedNet led_net = LedNet::Connecting;
static CRGB gate_colors[GATE_COUNT]{};
static CRGB clock_color = CRGB::Black;
static uint32_t mode_last_ms = 0;
static bool connecting_led_on = false;
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

void init_led() {
    FastLED.addLeds<WS2812, BOARD_LED_PIN, GRB>(leds, BOARD_LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
    FastLED.show();
    last_show_us = micros();
}

void handle_led() {
    const uint32_t now = millis();

    switch (led_mode) {
        case LedMode::Boot: {
            if (now - mode_last_ms < STEP_MS) {
                return;
            }
            mode_last_ms = now;

            const uint32_t step = now / STEP_MS;
            const uint32_t phase = step % (LED_ACTIVE_COUNT * COLOR_COUNT);
            const uint8_t led_index =
                static_cast<uint8_t>(LED_ACTIVE_FIRST + (phase / COLOR_COUNT));
            const uint8_t color_index = phase % COLOR_COUNT;

            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[led_index] = COLORS[color_index];
            break;
        }

        case LedMode::PreBoot:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            fill_solid(leds + LED_ACTIVE_FIRST, LED_ACTIVE_COUNT, CRGB::White);
            break;

        case LedMode::Normal: {
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);

            const bool net_status_phase =
                (led_net == LedNet::Connecting) ||
                ((led_net == LedNet::Ap || led_net == LedNet::Connected) &&
                 (now - net_ap_connected_splash_start_ms) < NET_AP_CONNECTED_SPLASH_MS);

            if (net_status_phase) {
                if (led_net == LedNet::Connecting) {
                    if (now - mode_last_ms >= CONNECTING_BLINK_MS) {
                        mode_last_ms = now;
                        connecting_led_on = !connecting_led_on;
                    }
                    leds[LED_NET_INDICATOR] =
                        connecting_led_on ? CRGB::Green : CRGB::Black;
                } else {
                    leds[LED_NET_INDICATOR] =
                        (led_net == LedNet::Ap) ? CRGB::Red : CRGB::Green;
                }
            } else {
                for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
                    const uint8_t led_idx = (BOARD_LED_COUNT - 1) - idx;
                    leds[led_idx] = gate_colors[idx];
                }
                leds[(BOARD_LED_COUNT - 1) - CLOCK_INDEX] = clock_color;
            }
            break;
        }
    }

    show_leds_throttled();
}

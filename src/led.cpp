#include <Arduino.h>
#include <FastLED.h>

#include "board.h"
#include "led.h"

static constexpr uint8_t LED_BRIGHTNESS = 96;
static constexpr uint16_t STEP_MS = 50;
static constexpr uint16_t CONNECTING_BLINK_MS = 150;
static constexpr uint8_t GATE_COUNT = 5;

static CRGB leds[BOARD_LED_COUNT];
static LedMode led_mode = LedMode::Boot;
static LedNet led_net = LedNet::Connecting;
static bool gate_states[GATE_COUNT] = {false};
static uint32_t mode_last_ms = 0;
static bool connecting_led_on = false;

// Перебор базовых цветов на каждом диоде по очереди, шаг 50 мс.
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
    }
}

void set_led_gate(uint8_t idx, bool state) {
    if (idx >= GATE_COUNT) {
        return;
    }
    gate_states[idx] = state;
}

void init_led() {
    FastLED.addLeds<WS2812, BOARD_LED_PIN, GRB>(leds, BOARD_LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
    FastLED.show();
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
            const uint32_t phase = step % (BOARD_LED_COUNT * COLOR_COUNT);
            const uint8_t led_index = phase / COLOR_COUNT;
            const uint8_t color_index = phase % COLOR_COUNT;

            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[led_index] = COLORS[color_index];
            break;
        }

        case LedMode::PreBoot:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::White);
            break;

        case LedMode::Normal:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);

            switch (led_net) {
                case LedNet::Connecting:
                    if (now - mode_last_ms >= CONNECTING_BLINK_MS) {
                        mode_last_ms = now;
                        connecting_led_on = !connecting_led_on;
                    }
                    leds[0] = connecting_led_on ? CRGB::Green : CRGB::Black;
                    break;
                case LedNet::Ap:
                    leds[0] = CRGB::Red;
                    break;
                case LedNet::Connected:
                    leds[0] = CRGB::Green;
                    break;
            }

            for (uint8_t idx = 0; idx < GATE_COUNT; ++idx) {
                const uint8_t led_idx = (BOARD_LED_COUNT - 1) - idx;
                leds[led_idx] = gate_states[idx] ? CRGB::White : CRGB::Black;
            }
            break;
    }

    FastLED.show();
}

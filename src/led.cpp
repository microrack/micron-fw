#include <Arduino.h>
#include <FastLED.h>

#include "board.h"
#include "led.h"

static constexpr uint8_t LED_BRIGHTNESS = 96;
static constexpr uint16_t STEP_MS = 50;
static constexpr uint16_t CONNECTING_BLINK_MS = 500;

static CRGB leds[BOARD_LED_COUNT];
static bool manual_led_state[BOARD_LED_COUNT] = {false};
static LedMode led_mode = LedMode::Boot;
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
        case LedMode::Connecting:
            mode_last_ms = 0;
            connecting_led_on = false;
            break;
        default:
            break;
    }
}

void set_led(uint8_t led_index, bool state) {
    if (led_index >= BOARD_LED_COUNT) {
        return;
    }
    manual_led_state[led_index] = state;
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
        case LedMode::Off:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::White);
            break;

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

        case LedMode::Connecting:
            if (now - mode_last_ms >= CONNECTING_BLINK_MS) {
                mode_last_ms = now;
                connecting_led_on = !connecting_led_on;
            }
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[0] = connecting_led_on ? CRGB::Green : CRGB::Black;
            break;

        case LedMode::Ap:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[0] = CRGB::Red;
            break;

        case LedMode::Connected:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[0] = CRGB::Green;
            break;

        case LedMode::Manual:
            for (uint8_t i = 0; i < BOARD_LED_COUNT; i++) {
                leds[i] = manual_led_state[i] ? CRGB::White : CRGB::Black;
            }
            break;

        case LedMode::All:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::White);
            break;
    }

    FastLED.show();
}

#include <Arduino.h>
#include <FastLED.h>

#include "board.h"
#include "led.h"

static constexpr uint8_t LED_BRIGHTNESS = 96;
static constexpr uint16_t STEP_MS = 50;

static CRGB leds[BOARD_LED_COUNT];
static bool manual_led_state[BOARD_LED_COUNT] = {false};
static LedMode led_mode = LedMode::Boot;

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
    led_mode = mode;
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
    static uint32_t last_ms = 0;
    const uint32_t now = millis();

    switch (led_mode) {
        case LedMode::Off:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::White);
            FastLED.show();
            break;

        case LedMode::Boot: {
            if (now - last_ms < STEP_MS) {
                return;
            }
            last_ms = now;

            const uint32_t step = now / STEP_MS;
            const uint32_t phase = step % (BOARD_LED_COUNT * COLOR_COUNT);
            const uint8_t led_index = phase / COLOR_COUNT;
            const uint8_t color_index = phase % COLOR_COUNT;

            fill_solid(leds, BOARD_LED_COUNT, CRGB::Black);
            leds[led_index] = COLORS[color_index];
            FastLED.show();
            break;
        }

        case LedMode::Manual:
            for (uint8_t i = 0; i < BOARD_LED_COUNT; i++) {
                leds[i] = manual_led_state[i] ? CRGB::White : CRGB::Black;
            }
            FastLED.show();
            break;

        case LedMode::All:
            fill_solid(leds, BOARD_LED_COUNT, CRGB::White);
            FastLED.show();
            break;
    }
}

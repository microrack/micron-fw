#include "timer_load_task.h"

#include <Arduino.h>
#include <atomic>
#include <cmath>

#include "esp_attr.h"
#include "esp_check.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cpu_affinity.h"
#include "mcp4728.h"

namespace {

static constexpr uint64_t kTimerPeriodUs = 100;
static constexpr uint32_t kSampleRateHz = 1000000U / static_cast<uint32_t>(kTimerPeriodUs);
static constexpr float kInvSampleRate = 1.0f / static_cast<float>(kSampleRateHz);

// 1 MHz ticks: period kTimerPeriodUs µs
static constexpr uint64_t kGptimerResolutionHz = 1000000;
static constexpr uint64_t kGptimerAlarmTicks = kTimerPeriodUs;

static constexpr uint32_t kSineLutSize = 256;
static constexpr uint16_t kDacMid = 2048;
static constexpr uint16_t kDacAmp = 2047;

static constexpr float kDefaultSineHz = 100.0f;
static constexpr float kMinSineHz = 20.0f;
static constexpr float kMaxSineHz = static_cast<float>(kSampleRateHz) / 2.0f - 100.0f;  // margin below Nyquist

static gptimer_handle_t g_gptimer = nullptr;
static TaskHandle_t g_timer_worker_task = nullptr;

static uint16_t g_sine_lut[kSineLutSize];
static float g_phase = 0.0f;

static std::atomic<float> g_dac_sine_hz{kDefaultSineHz};

static void fill_sine_lut() {
    for (uint32_t i = 0; i < kSineLutSize; ++i) {
        const float t = (2.0f * static_cast<float>(M_PI) * static_cast<float>(i)) / static_cast<float>(kSineLutSize);
        const float s = sinf(t);
        const int32_t v = static_cast<int32_t>(static_cast<float>(kDacMid) + static_cast<float>(kDacAmp) * s);
        g_sine_lut[i] = static_cast<uint16_t>(v < 0 ? 0 : (v > 0xFFF ? 0xFFF : v));
    }
}

// ISR: just counts ticks. Task notification value accumulates missed ticks automatically.
static bool IRAM_ATTR gptimer_on_alarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* user_ctx) {
    (void)timer;
    (void)edata;
    (void)user_ctx;
    BaseType_t task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(g_timer_worker_task, &task_woken);
    return task_woken == pdTRUE;
}

static void timer_worker_task(void* arg) {
    (void)arg;
    while (true) {
        // pdTRUE: take ALL accumulated ticks at once, clear counter.
        // ticks > 1 means the I2C transmit in the previous iteration took longer than one timer period;
        // we advance phase by the full elapsed tick count to keep the output frequency correct.
        const uint32_t ticks = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        const float hz = g_dac_sine_hz.load(std::memory_order_relaxed);
        g_phase += hz * kInvSampleRate * static_cast<float>(ticks);
        while (g_phase >= 1.0f) {
            g_phase -= 1.0f;
        }

        uint32_t idx = static_cast<uint32_t>(g_phase * static_cast<float>(kSineLutSize));
        if (idx >= kSineLutSize) {
            idx = kSineLutSize - 1;
        }
        const uint16_t sample = g_sine_lut[idx];

        // Wire.endTransmission() blocks the task (not the CPU) until done.
        // If the transfer takes longer than one timer period, the next
        // ulTaskNotifyTake returns ticks > 1 and phase is compensated above.
        (void)mcp4728_write_all(1, &sample);
    }
}

}  // namespace

void timer_load_set_dac_sine_hz(float hz) {
    if (hz < kMinSineHz) {
        hz = kMinSineHz;
    } else if (hz > kMaxSineHz) {
        hz = kMaxSineHz;
    }
    g_dac_sine_hz.store(hz, std::memory_order_relaxed);
}

void timer_load_task_init() {
    fill_sine_lut();
    // Priority 21: above USB daemon/client (20) so they cannot cause audio glitches.
    // Pinned to kAppTaskCore (Core 1): WiFi runs on Core 0 and is completely isolated.
    xTaskCreatePinnedToCore(timer_worker_task, "timer_load_worker", 4096, nullptr, 21, &g_timer_worker_task, kAppTaskCore);
    assert(g_timer_worker_task != nullptr);

    gptimer_config_t timer_cfg = {};
    timer_cfg.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_cfg.direction = GPTIMER_COUNT_UP;
    timer_cfg.resolution_hz = static_cast<uint32_t>(kGptimerResolutionHz);
    // Non-zero: request a higher CPU interrupt priority than the default pool (1..3).
    timer_cfg.intr_priority = 3;
    timer_cfg.flags.intr_shared = false;

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &g_gptimer));

    gptimer_event_callbacks_t cbs = {};
    cbs.on_alarm = gptimer_on_alarm;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(g_gptimer, &cbs, nullptr));

    static gptimer_alarm_config_t alarm_cfg = {};
    alarm_cfg.reload_count = 0;
    alarm_cfg.alarm_count = kGptimerAlarmTicks;
    alarm_cfg.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(g_gptimer, &alarm_cfg));

    ESP_ERROR_CHECK(gptimer_enable(g_gptimer));
    ESP_ERROR_CHECK(gptimer_start(g_gptimer));
}

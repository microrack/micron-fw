#include "cv_gate.h"

#include "board.h"
#include "cpu_affinity.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mcp4728.h"
#include <cmath>

namespace {

constexpr uint8_t CV_CHANNEL_COUNT = 4;
constexpr float   CV_VREF_V        = 5.0f;
constexpr uint64_t CV_DAC_TIMER_PERIOD_US = 100;  // 10 kHz synth/cv update tick
constexpr uint64_t CV_DAC_TIMER_RES_HZ = 1000000;
constexpr float CV_SYNTH_SAMPLE_RATE_HZ =
    static_cast<float>(CV_DAC_TIMER_RES_HZ) / static_cast<float>(CV_DAC_TIMER_PERIOD_US);
constexpr float C2_FREQ_HZ = 65.405f;
constexpr float CV_SYNTH_RISE_SEC = 0.050f;
constexpr float CV_SYNTH_FALL_SEC = 0.200f;
constexpr float CV_SYNTH_RISE_STEP =
    1.0f / (CV_SYNTH_RISE_SEC * CV_SYNTH_SAMPLE_RATE_HZ);
constexpr float CV_SYNTH_FALL_STEP =
    1.0f / (CV_SYNTH_FALL_SEC * CV_SYNTH_SAMPLE_RATE_HZ);

enum class SynthEnvelopeState : uint8_t {
    Idle = 0,
    Rise,
    RiseThenFall,
    Sustain,
    Fall,
};

static uint16_t g_cv_codes[CV_CHANNEL_COUNT] = {};
static float g_cv_phase_increments[CV_CHANNEL_COUNT] = {};
static float g_cv_amplitudes[CV_CHANNEL_COUNT] = {};
static float g_cv_phase[CV_CHANNEL_COUNT] = {};
static SynthEnvelopeState g_cv_envelope_states[CV_CHANNEL_COUNT] = {};
static bool g_cv_dirty = true;
static CvGateMode g_mode = CvGateMode::CvGate;
static portMUX_TYPE g_cv_codes_lock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t g_cv_dac_task = nullptr;
static gptimer_handle_t g_cv_dac_timer = nullptr;

bool IRAM_ATTR cv_dac_timer_on_alarm(
    gptimer_handle_t,
    const gptimer_alarm_event_data_t*,
    void*
) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(g_cv_dac_task, &woken);
    return woken == pdTRUE;
}

void cv_dac_worker_task(void*) {
    uint16_t local_codes[CV_CHANNEL_COUNT] = {};
    float local_phase_increments[CV_CHANNEL_COUNT] = {};
    float local_amplitudes[CV_CHANNEL_COUNT] = {};
    CvGateMode local_mode = CvGateMode::CvGate;
    bool local_dirty = false;

    while (true) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        taskENTER_CRITICAL(&g_cv_codes_lock);
        for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
            local_codes[i] = g_cv_codes[i];
            local_phase_increments[i] = g_cv_phase_increments[i];
        }
        local_mode = g_mode;
        local_dirty = g_cv_dirty;
        if (g_mode == CvGateMode::CvGate) {
            g_cv_dirty = false;
        }
        taskEXIT_CRITICAL(&g_cv_codes_lock);

        if (local_mode == CvGateMode::CvGate) {
            if (local_dirty) {
                (void)mcp4728_write_all(CV_CHANNEL_COUNT, local_codes);
            }
            continue;
        }

        // Synth mode: update rise/fall envelopes, then sum 4 normalized triangle oscillators.
        float weighted_sum = 0.0f;
        taskENTER_CRITICAL(&g_cv_codes_lock);
        for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
            switch (g_cv_envelope_states[i]) {
                case SynthEnvelopeState::Idle:
                    g_cv_amplitudes[i] = 0.0f;
                    break;
                case SynthEnvelopeState::Rise:
                    g_cv_amplitudes[i] += CV_SYNTH_RISE_STEP;
                    if (g_cv_amplitudes[i] >= 1.0f) {
                        g_cv_amplitudes[i] = 1.0f;
                        g_cv_envelope_states[i] = SynthEnvelopeState::Sustain;
                    }
                    break;
                case SynthEnvelopeState::RiseThenFall:
                    g_cv_amplitudes[i] += CV_SYNTH_RISE_STEP;
                    if (g_cv_amplitudes[i] >= 1.0f) {
                        g_cv_amplitudes[i] = 1.0f;
                        g_cv_envelope_states[i] = SynthEnvelopeState::Fall;
                    }
                    break;
                case SynthEnvelopeState::Sustain:
                    g_cv_amplitudes[i] = 1.0f;
                    break;
                case SynthEnvelopeState::Fall:
                    g_cv_amplitudes[i] -= CV_SYNTH_FALL_STEP;
                    if (g_cv_amplitudes[i] <= 0.0f) {
                        g_cv_amplitudes[i] = 0.0f;
                        g_cv_envelope_states[i] = SynthEnvelopeState::Idle;
                    }
                    break;
            }
            local_amplitudes[i] = g_cv_amplitudes[i];
            const float amp = local_amplitudes[i];
            if (amp <= 0.0f) {
                continue;
            }
            g_cv_phase[i] += local_phase_increments[i];
            while (g_cv_phase[i] >= 1.0f) {
                g_cv_phase[i] -= 1.0f;
            }
            while (g_cv_phase[i] < 0.0f) {
                g_cv_phase[i] += 1.0f;
            }
            const float tri = 1.0f - 4.0f * fabsf(g_cv_phase[i] - 0.5f);  // -1..1
            weighted_sum += tri * amp;
        }
        taskEXIT_CRITICAL(&g_cv_codes_lock);

        float clamped = weighted_sum * 0.25f;
        if (clamped > 1.0f) {
            clamped = 1.0f;
        } else if (clamped < -1.0f) {
            clamped = -1.0f;
        }

        int32_t sample = static_cast<int32_t>(2048.0f + clamped * 2047.0f);
        if (sample < 0) {
            sample = 0;
        } else if (sample > 4095) {
            sample = 4095;
        }
        const uint16_t sample_u12 = static_cast<uint16_t>(sample);
        (void)mcp4728_write_all(1, &sample_u12);
    }
}

}  // namespace

void init_cv_gate() {
    init_mcp4728();
    for (uint8_t i = 0; i < GATE_OUT_PIN_COUNT; ++i) {
        pinMode(GATE_OUT_PINS[i], OUTPUT);
        digitalWrite(GATE_OUT_PINS[i], HIGH);  // off: active-low gates idle high
    }
    pinMode(CLOCK_OUT_PIN, OUTPUT);
    digitalWrite(CLOCK_OUT_PIN, HIGH);  // off: active-low clock idle high

    xTaskCreatePinnedToCore(
        cv_dac_worker_task,
        "cv_dac_worker",
        2048,
        nullptr,
        20,
        &g_cv_dac_task,
        APP_TASK_CORE
    );

    gptimer_config_t timer_cfg = {};
    timer_cfg.clk_src = GPTIMER_CLK_SRC_DEFAULT;
    timer_cfg.direction = GPTIMER_COUNT_UP;
    timer_cfg.resolution_hz = static_cast<uint32_t>(CV_DAC_TIMER_RES_HZ);
    timer_cfg.intr_priority = 3;
    timer_cfg.flags.intr_shared = false;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_cfg, &g_cv_dac_timer));

    gptimer_event_callbacks_t cbs = {};
    cbs.on_alarm = cv_dac_timer_on_alarm;
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(g_cv_dac_timer, &cbs, nullptr));

    gptimer_alarm_config_t alarm_cfg = {};
    alarm_cfg.reload_count = 0;
    alarm_cfg.alarm_count = CV_DAC_TIMER_PERIOD_US;
    alarm_cfg.flags.auto_reload_on_alarm = true;
    ESP_ERROR_CHECK(gptimer_set_alarm_action(g_cv_dac_timer, &alarm_cfg));
    ESP_ERROR_CHECK(gptimer_enable(g_cv_dac_timer));
    ESP_ERROR_CHECK(gptimer_start(g_cv_dac_timer));
}

void set_cv_gate_mode(CvGateMode mode) {
    taskENTER_CRITICAL(&g_cv_codes_lock);
    g_mode = mode;
    g_cv_dirty = true;
    if (mode == CvGateMode::Synth) {
        for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
            g_cv_phase[i] = 0.0f;
            g_cv_amplitudes[i] = 0.0f;
            g_cv_envelope_states[i] = SynthEnvelopeState::Idle;
        }
    }
    taskEXIT_CRITICAL(&g_cv_codes_lock);
}

void set_gate(uint8_t idx, bool on) {
    if (idx < GATE_OUT_PIN_COUNT) {
        digitalWrite(GATE_OUT_PINS[idx], on ? LOW : HIGH);
    }
    if (idx < CV_CHANNEL_COUNT) {
        taskENTER_CRITICAL(&g_cv_codes_lock);
        if (on) {
            g_cv_envelope_states[idx] = SynthEnvelopeState::Rise;
        } else {
            if (g_cv_envelope_states[idx] == SynthEnvelopeState::Rise) {
                g_cv_envelope_states[idx] = SynthEnvelopeState::RiseThenFall;
            } else {
                g_cv_envelope_states[idx] = SynthEnvelopeState::Fall;
            }
        }
        taskEXIT_CRITICAL(&g_cv_codes_lock);
    }
}

void set_all_gates(bool on) {
    for (uint8_t i = 0; i < GATE_OUT_PIN_COUNT; ++i) {
        set_gate(i, on);
    }
}

void set_clock(bool on) {
    digitalWrite(CLOCK_OUT_PIN, on ? LOW : HIGH);
}

bool set_cv(uint8_t channel, float volts) {
    if (channel >= CV_CHANNEL_COUNT) {
        return false;
    }
    if (volts < 0.0f) {
        volts = 0.0f;
    }
    if (volts > CV_VREF_V) {
        volts = CV_VREF_V;
    }
    // MCP4728: VOUT = VREF * code / 4096, code in 0..4095
    const float scaled = volts * (4096.0f / CV_VREF_V);
    auto code = static_cast<uint32_t>(scaled + 0.5f);
    if (code > 4095U) {
        code = 4095U;
    }
    const float phase_increment = (C2_FREQ_HZ * powf(2.0f, volts)) / CV_SYNTH_SAMPLE_RATE_HZ;
    taskENTER_CRITICAL(&g_cv_codes_lock);
    g_cv_codes[channel] = static_cast<uint16_t>(code & 0x0FFFU);
    g_cv_phase_increments[channel] = phase_increment;
    g_cv_dirty = true;
    taskEXIT_CRITICAL(&g_cv_codes_lock);

    return true;
}

void set_all_cv(float volts) {
    for (uint8_t i = 0; i < CV_CHANNEL_COUNT; ++i) {
        (void)set_cv(i, volts);
    }
}

void reset_all_outputs() {
    set_all_gates(false);
    set_clock(false);
    set_all_cv(0.0f);
}

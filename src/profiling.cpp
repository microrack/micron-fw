#include "profiling.h"

#include <atomic>
#include <cfloat>

#include "esp_freertos_hooks.h"
#include "logger.h"
#include "soc/soc_caps.h"

static constexpr uint32_t kReportPeriodMs = 5000;

#if SOC_CPU_CORES_NUM >= 2
static constexpr uint8_t kCpuCores = 2;
#else
static constexpr uint8_t kCpuCores = 1;
#endif

static std::atomic<uint32_t> g_cpu_idle_hook_ticks[kCpuCores];

static bool cpu_idle_hook_core0(void) {
    g_cpu_idle_hook_ticks[0].fetch_add(1, std::memory_order_relaxed);
    return false;
}

#if SOC_CPU_CORES_NUM >= 2
static bool cpu_idle_hook_core1(void) {
    g_cpu_idle_hook_ticks[1].fetch_add(1, std::memory_order_relaxed);
    return false;
}
#endif

static uint64_t g_loop_profile_acc_us[static_cast<size_t>(LoopProfileSlot::Count)] = {};
static uint32_t g_loop_profile_max_us[static_cast<size_t>(LoopProfileSlot::Count)] = {};

static const char* loop_profile_slot_name(LoopProfileSlot slot) {
    switch (slot) {
        case LoopProfileSlot::ProfilingTick:
            return "profiling_tick";
        case LoopProfileSlot::PollLifecycle:
            return "gadget_handler_poll_lifecycle";
        case LoopProfileSlot::MidiPoll:
            return "midi_input_poll";
        case LoopProfileSlot::Tick:
            return "tick";
        case LoopProfileSlot::HandleNet:
            return "handle_net";
        case LoopProfileSlot::OtaHandle:
            return "ota_handle";
        case LoopProfileSlot::HandleLed:
            return "handle_led";
        default:
            return "?";
    }
}

static void loop_profile_report_and_reset(uint32_t window_ms) {
    if (window_ms == 0) {
        return;
    }
    const uint64_t wall_us = static_cast<uint64_t>(window_ms) * 1000ULL;
    if (wall_us == 0) {
        return;
    }

    uint64_t accounted_us = 0;
    for (size_t i = 0; i < static_cast<size_t>(LoopProfileSlot::Count); ++i) {
        accounted_us += g_loop_profile_acc_us[i];
    }

    logger_printf(
        "loop profile %% of wall (%ums): accounted=%.1f%% of wall",
        static_cast<unsigned>(window_ms),
        static_cast<double>(100.0 * static_cast<double>(accounted_us) / static_cast<double>(wall_us))
    );

    for (size_t i = 0; i < static_cast<size_t>(LoopProfileSlot::Count); ++i) {
        const LoopProfileSlot slot = static_cast<LoopProfileSlot>(i);
        const uint64_t acc = g_loop_profile_acc_us[i];
        const float pct_wall = static_cast<float>(100.0 * static_cast<double>(acc) / static_cast<double>(wall_us));
        const float pct_accounted =
            accounted_us > 0
                ? static_cast<float>(100.0 * static_cast<double>(acc) / static_cast<double>(accounted_us))
                : 0.0f;
        const uint32_t max_us = g_loop_profile_max_us[i];
        logger_printf(
            "  %s: %.2f%% wall  %.1f%% of measured  max iter=%lu us",
            loop_profile_slot_name(slot),
            static_cast<double>(pct_wall),
            static_cast<double>(pct_accounted),
            static_cast<unsigned long>(max_us)
        );
    }

    for (size_t i = 0; i < static_cast<size_t>(LoopProfileSlot::Count); ++i) {
        g_loop_profile_acc_us[i] = 0;
        g_loop_profile_max_us[i] = 0;
    }
}

void profiling_add_slot(LoopProfileSlot slot, uint32_t dt_us) {
    const auto i = static_cast<size_t>(slot);
    if (i < static_cast<size_t>(LoopProfileSlot::Count)) {
        g_loop_profile_acc_us[i] += static_cast<uint64_t>(dt_us);
        if (dt_us > g_loop_profile_max_us[i]) {
            g_loop_profile_max_us[i] = dt_us;
        }
    }
}

void profiling_init() {
    if (esp_register_freertos_idle_hook_for_cpu(cpu_idle_hook_core0, 0) != ESP_OK) {
        logger_printf("CPU load: idle hook core0 failed");
    }
#if SOC_CPU_CORES_NUM >= 2
    if (esp_register_freertos_idle_hook_for_cpu(cpu_idle_hook_core1, 1) != ESP_OK) {
        logger_printf("CPU load: idle hook core1 failed");
    }
#endif
}

void profiling_tick() {
    static uint32_t prev_loop_us = 0;
    static uint32_t last_report_ms = 0;
    static float hz_min = FLT_MAX;
    static float hz_max = 0.0f;
    static double hz_sum = 0.0;
    static uint32_t hz_samples = 0;
    static bool skip_next_loop_hz_sample = false;

    static uint32_t prev_idle_ticks[kCpuCores] = {};
    static uint32_t peak_idle_delta[kCpuCores] = {};

    const uint32_t now_us = micros();
    const uint32_t now_ms = millis();

    if (prev_loop_us != 0) {
        if (skip_next_loop_hz_sample) {
            skip_next_loop_hz_sample = false;
        } else {
            uint32_t dt_us = now_us - prev_loop_us;
            if (dt_us < 1) {
                dt_us = 1;
            }
            const float hz = 1e6f / static_cast<float>(dt_us);
            if (hz < hz_min) {
                hz_min = hz;
            }
            if (hz > hz_max) {
                hz_max = hz;
            }
            hz_sum += static_cast<double>(hz);
            ++hz_samples;
        }
    }
    prev_loop_us = now_us;

    if (last_report_ms == 0) {
        last_report_ms = now_ms;
    }

    if (static_cast<uint32_t>(now_ms - last_report_ms) < kReportPeriodMs) {
        return;
    }

    const uint32_t report_window_ms = now_ms - last_report_ms;

    if (hz_samples > 0) {
        const float avg = static_cast<float>(hz_sum / static_cast<double>(hz_samples));
        logger_printf(
            "loop Hz: min=%.1f avg=%.1f max=%.1f (n=%lu)",
            hz_min,
            avg,
            hz_max,
            static_cast<unsigned long>(hz_samples)
        );
    } else {
        logger_printf("loop Hz: no samples");
    }

    for (uint8_t c = 0; c < kCpuCores; ++c) {
        const uint32_t idle_now = g_cpu_idle_hook_ticks[c].load(std::memory_order_relaxed);
        const uint32_t delta = idle_now - prev_idle_ticks[c];
        prev_idle_ticks[c] = idle_now;
        if (delta > peak_idle_delta[c]) {
            peak_idle_delta[c] = delta;
        }
        float load_pct = 0.0f;
        if (peak_idle_delta[c] > 0) {
            load_pct = 100.0f * (1.0f - static_cast<float>(delta) / static_cast<float>(peak_idle_delta[c]));
            if (load_pct < 0.0f) {
                load_pct = 0.0f;
            }
            if (load_pct > 100.0f) {
                load_pct = 100.0f;
            }
        }
        logger_printf(
            "CPU core%u load ~%.0f%% (idle ticks/%ums=%lu peak=%lu)",
            static_cast<unsigned>(c),
            static_cast<double>(load_pct),
            static_cast<unsigned>(kReportPeriodMs),
            static_cast<unsigned long>(delta),
            static_cast<unsigned long>(peak_idle_delta[c])
        );
    }

    loop_profile_report_and_reset(report_window_ms);

    hz_min = FLT_MAX;
    hz_max = 0.0f;
    hz_sum = 0.0;
    hz_samples = 0;
    last_report_ms = now_ms;
    skip_next_loop_hz_sample = true;
}

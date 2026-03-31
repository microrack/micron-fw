#include "logger.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
}

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {
constexpr size_t kLogCapacity = 64;
constexpr size_t kLogMessageLen = 160;

SemaphoreHandle_t g_log_mutex = nullptr;
char g_log_ring[kLogCapacity][kLogMessageLen] = {};
size_t g_log_head = 0;
size_t g_log_tail = 0;
size_t g_log_count = 0;

bool lock_logs() {
    return (g_log_mutex != nullptr) && (xSemaphoreTake(g_log_mutex, portMAX_DELAY) == pdTRUE);
}

void unlock_logs() {
    xSemaphoreGive(g_log_mutex);
}
}  // namespace

void logger_init() {
    if (g_log_mutex == nullptr) {
        g_log_mutex = xSemaphoreCreateMutex();
    }
}

void logger_printf(const char* fmt, ...) {
    if (fmt == nullptr) {
        return;
    }
    if (!lock_logs()) {
        return;
    }

    char formatted[kLogMessageLen];
    va_list args;
    va_start(args, fmt);
    vsnprintf(formatted, sizeof(formatted), fmt, args);
    va_end(args);

    if (g_log_count == kLogCapacity) {
        g_log_tail = (g_log_tail + 1) % kLogCapacity;
        --g_log_count;
    }

    strncpy(g_log_ring[g_log_head], formatted, kLogMessageLen - 1);
    g_log_ring[g_log_head][kLogMessageLen - 1] = '\0';
    g_log_head = (g_log_head + 1) % kLogCapacity;
    ++g_log_count;

    unlock_logs();
}

bool logger_get_next(char* out_message, size_t out_message_size) {
    if (out_message == nullptr || out_message_size == 0) {
        return false;
    }
    if (!lock_logs()) {
        return false;
    }
    if (g_log_count == 0) {
        unlock_logs();
        return false;
    }

    strncpy(out_message, g_log_ring[g_log_tail], out_message_size - 1);
    out_message[out_message_size - 1] = '\0';
    g_log_tail = (g_log_tail + 1) % kLogCapacity;
    --g_log_count;

    unlock_logs();
    return true;
}

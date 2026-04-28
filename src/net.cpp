#include "net.h"
#include "cpu_affinity.h"
#include "logger.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
}

static constexpr const char* AP_SSID = "PLTRNK";
static constexpr uint16_t SERVER_PORT = 2323;
static constexpr size_t MAX_CLIENTS = 4;
static constexpr size_t MAX_MESSAGE_LEN = 160;
static constexpr uint32_t CLIENT_CONNECT_TIMEOUT_MS = 10000;

static constexpr UBaseType_t LOG_SEND_TASK_PRIORITY = 5;
static constexpr uint32_t LOG_SEND_TASK_STACK_WORDS = 4096;
static constexpr TickType_t LOG_SEND_WAIT_TICKS = pdMS_TO_TICKS(500);

static WiFiServer g_server(SERVER_PORT);
static WiFiClient g_clients[MAX_CLIENTS];
static bool g_net_enabled = false;
static bool g_server_started = false;
static NetState g_net_state = NetState::Ap;
static char g_sta_ssid[33] = {};
static char g_sta_password[65] = {};
static uint32_t g_connect_started_ms = 0;

static SemaphoreHandle_t g_clients_mutex = nullptr;
static SemaphoreHandle_t g_log_send_wakeup = nullptr;
static TaskHandle_t g_log_send_task = nullptr;

static void broadcast_one_message_locked(const char* message) {
    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        if (g_clients[i] && g_clients[i].connected()) {
            g_clients[i].print(message);
            g_clients[i].print('\n');
        }
    }
}

static bool prune_clients_and_has_any_locked() {
    bool has_any = false;
    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        if (!g_clients[i]) {
            continue;
        }
        if (!g_clients[i].connected()) {
            g_clients[i].stop();
            continue;
        }
        has_any = true;
    }
    return has_any;
}

static void stop_server_and_clients_locked() {
    if (g_server_started) {
        g_server.stop();
        g_server_started = false;
    }

    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        if (g_clients[i]) {
            g_clients[i].stop();
        }
    }
}

static void stop_server_and_clients() {
    if (g_clients_mutex == nullptr) {
        return;
    }
    if (xSemaphoreTake(g_clients_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    stop_server_and_clients_locked();
    xSemaphoreGive(g_clients_mutex);
}

static void start_server_if_needed() {
    if (g_server_started) {
        return;
    }
    g_server.begin();
    g_server.setNoDelay(true);
    g_server_started = true;
}

static void accept_new_clients() {
    WiFiClient new_client = g_server.accept();
    if (!new_client) {
        return;
    }

    if (g_clients_mutex == nullptr || xSemaphoreTake(g_clients_mutex, portMAX_DELAY) != pdTRUE) {
        new_client.stop();
        return;
    }

    bool accepted = false;
    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        if (!g_clients[i] || !g_clients[i].connected()) {
            if (g_clients[i]) {
                g_clients[i].stop();
            }
            g_clients[i] = new_client;
            accepted = true;
            break;
        }
    }

    if (!accepted) {
        new_client.stop();
    }

    xSemaphoreGive(g_clients_mutex);

    if (accepted && g_log_send_wakeup != nullptr) {
        xSemaphoreGive(g_log_send_wakeup);
    }
}

static void log_tcp_send_task(void* arg) {
    (void)arg;
    char log_message[MAX_MESSAGE_LEN];

    for (;;) {
        if (g_log_send_wakeup != nullptr) {
            xSemaphoreTake(g_log_send_wakeup, LOG_SEND_WAIT_TICKS);
        } else {
            vTaskDelay(LOG_SEND_WAIT_TICKS);
        }

        if (!g_net_enabled) {
            continue;
        }

        if (g_clients_mutex == nullptr) {
            continue;
        }

        if (xSemaphoreTake(g_clients_mutex, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const bool has_clients = prune_clients_and_has_any_locked();
        xSemaphoreGive(g_clients_mutex);

        if (!has_clients) {
            continue;
        }

        while (logger_get_next(log_message, sizeof(log_message))) {
            if (xSemaphoreTake(g_clients_mutex, portMAX_DELAY) != pdTRUE) {
                break;
            }
            broadcast_one_message_locked(log_message);
            xSemaphoreGive(g_clients_mutex);
        }
    }
}

static void net_notify_log_pending() {
    if (!g_net_enabled || g_log_send_wakeup == nullptr) {
        return;
    }
    xSemaphoreGive(g_log_send_wakeup);
}

static void ensure_log_forwarder_started() {
    if (g_log_send_task != nullptr) {
        return;
    }
    if (g_clients_mutex == nullptr) {
        g_clients_mutex = xSemaphoreCreateMutex();
    }
    if (g_log_send_wakeup == nullptr) {
        g_log_send_wakeup = xSemaphoreCreateBinary();
    }

    logger_set_output_notify(net_notify_log_pending);

    xTaskCreatePinnedToCore(
        log_tcp_send_task,
        "log_tcp",
        LOG_SEND_TASK_STACK_WORDS,
        nullptr,
        LOG_SEND_TASK_PRIORITY,
        &g_log_send_task,
        APP_TASK_CORE
    );
}

void net_init(const AppConfig& config) {
    g_net_enabled = config.wifi;
    strncpy(g_sta_ssid, config.ssid, sizeof(g_sta_ssid) - 1);
    g_sta_ssid[sizeof(g_sta_ssid) - 1] = '\0';
    strncpy(g_sta_password, config.password, sizeof(g_sta_password) - 1);
    g_sta_password[sizeof(g_sta_password) - 1] = '\0';

    if (!g_net_enabled) {
        logger_set_output_notify(nullptr);
        if (g_log_send_task != nullptr) {
            vTaskDelete(g_log_send_task);
            g_log_send_task = nullptr;
        }
        stop_server_and_clients();
        WiFi.mode(WIFI_OFF);
        return;
    }

    ensure_log_forwarder_started();

    if (strlen(g_sta_ssid) == 0) {
        net_start_ap();
    } else {
        net_start_client();
    }
}

void net_start_ap() {
    if (!g_net_enabled) {
        return;
    }

    ensure_log_forwarder_started();

    stop_server_and_clients();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    start_server_if_needed();
    g_net_state = NetState::Ap;
}

void net_start_client() {
    if (!g_net_enabled) {
        return;
    }
    if (strlen(g_sta_ssid) == 0) {
        net_start_ap();
        return;
    }

    ensure_log_forwarder_started();

    stop_server_and_clients();
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_sta_ssid, g_sta_password);
    g_connect_started_ms = millis();
    g_net_state = NetState::Connecting;
}

NetState net_get_state() {
    return g_net_state;
}

void handle_net() {
    if (!g_net_enabled) {
        return;
    }

    if (g_net_state == NetState::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            start_server_if_needed();
            g_net_state = NetState::Client;
            logger_printf("WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
        } else if (millis() - g_connect_started_ms >= CLIENT_CONNECT_TIMEOUT_MS) {
            net_start_ap();
        }
    }

    accept_new_clients();
}

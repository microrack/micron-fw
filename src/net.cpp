#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace {
constexpr const char* kApSsid = "PLTRNK";
constexpr uint16_t kServerPort = 2323;
constexpr size_t kMaxClients = 4;
constexpr size_t kRingCapacity = 32;
constexpr size_t kMaxMessageLen = 64;
constexpr uint32_t kClientConnectTimeoutMs = 10000;

WiFiServer g_server(kServerPort);
WiFiClient g_clients[kMaxClients];
uint32_t g_client_next_seq[kMaxClients] = {};

char g_message_ring[kRingCapacity][kMaxMessageLen] = {};
uint32_t g_next_seq = 0;
SemaphoreHandle_t g_ring_mutex = nullptr;
bool g_net_enabled = false;
bool g_server_started = false;
NetState g_net_state = NetState::Ap;
char g_sta_ssid[33] = {};
char g_sta_password[65] = {};
uint32_t g_connect_started_ms = 0;

bool lock_ring() {
    return (g_ring_mutex != nullptr) && (xSemaphoreTake(g_ring_mutex, portMAX_DELAY) == pdTRUE);
}

void unlock_ring() {
    xSemaphoreGive(g_ring_mutex);
}

void stop_server_and_clients() {
    if (g_server_started) {
        g_server.stop();
        g_server_started = false;
    }

    for (size_t i = 0; i < kMaxClients; ++i) {
        if (g_clients[i]) {
            g_clients[i].stop();
        }
        g_client_next_seq[i] = g_next_seq;
    }
}

void start_server_if_needed() {
    if (g_server_started) {
        return;
    }
    g_server.begin();
    g_server.setNoDelay(true);
    g_server_started = true;
}

uint32_t get_oldest_seq_unsafe() {
    if (g_next_seq <= kRingCapacity) {
        return 0;
    }
    return g_next_seq - kRingCapacity;
}

void accept_new_clients() {
    WiFiClient new_client = g_server.accept();
    if (!new_client) {
        return;
    }

    for (size_t i = 0; i < kMaxClients; ++i) {
        if (!g_clients[i] || !g_clients[i].connected()) {
            if (g_clients[i]) {
                g_clients[i].stop();
            }
            g_clients[i] = new_client;
            if (lock_ring()) {
                g_client_next_seq[i] = get_oldest_seq_unsafe();
                unlock_ring();
            } else {
                g_client_next_seq[i] = g_next_seq;
            }
            return;
        }
    }

    // No free slots, reject the incoming client.
    new_client.stop();
}

void flush_clients() {
    for (size_t i = 0; i < kMaxClients; ++i) {
        if (!g_clients[i]) {
            continue;
        }
        if (!g_clients[i].connected()) {
            g_clients[i].stop();
            continue;
        }

        while (true) {
            char message[kMaxMessageLen];
            bool has_message = false;

            if (!lock_ring()) {
                break;
            }

            const uint32_t oldest_seq = get_oldest_seq_unsafe();
            if (g_client_next_seq[i] < oldest_seq) {
                g_client_next_seq[i] = oldest_seq;
            }
            if (g_client_next_seq[i] < g_next_seq) {
                const size_t idx = g_client_next_seq[i] % kRingCapacity;
                strncpy(message, g_message_ring[idx], sizeof(message));
                message[sizeof(message) - 1] = '\0';
                ++g_client_next_seq[i];
                has_message = true;
            }

            unlock_ring();

            if (!has_message) {
                break;
            }

            g_clients[i].print(message);
            g_clients[i].print('\n');
        }
    }
}
}  // namespace

void net_init(const AppConfig& config) {
    if (g_ring_mutex == nullptr) {
        g_ring_mutex = xSemaphoreCreateMutex();
    }

    g_net_enabled = config.wifi;
    strncpy(g_sta_ssid, config.ssid, sizeof(g_sta_ssid) - 1);
    g_sta_ssid[sizeof(g_sta_ssid) - 1] = '\0';
    strncpy(g_sta_password, config.password, sizeof(g_sta_password) - 1);
    g_sta_password[sizeof(g_sta_password) - 1] = '\0';

    if (!g_net_enabled) {
        stop_server_and_clients();
        WiFi.mode(WIFI_OFF);
        return;
    }

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

    stop_server_and_clients();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);
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
        } else if (millis() - g_connect_started_ms >= kClientConnectTimeoutMs) {
            net_start_ap();
        }
    }

    accept_new_clients();
    flush_clients();
}

void send_message(const char* message) {
    if (message == nullptr) {
        return;
    }
    if (!lock_ring()) {
        return;
    }

    const size_t idx = g_next_seq % kRingCapacity;
    strncpy(g_message_ring[idx], message, kMaxMessageLen - 1);
    g_message_ring[idx][kMaxMessageLen - 1] = '\0';
    ++g_next_seq;

    unlock_ring();
}

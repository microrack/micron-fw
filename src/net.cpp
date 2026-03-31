#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr const char* kApSsid = "PLTRNK";
constexpr uint16_t kServerPort = 2323;
constexpr size_t kMaxClients = 4;
constexpr size_t kRingCapacity = 32;
constexpr size_t kMaxMessageLen = 64;

WiFiServer g_server(kServerPort);
WiFiClient g_clients[kMaxClients];
uint32_t g_client_next_seq[kMaxClients] = {};

char g_message_ring[kRingCapacity][kMaxMessageLen] = {};
uint32_t g_next_seq = 0;
SemaphoreHandle_t g_ring_mutex = nullptr;

bool lock_ring() {
    return (g_ring_mutex != nullptr) && (xSemaphoreTake(g_ring_mutex, portMAX_DELAY) == pdTRUE);
}

void unlock_ring() {
    xSemaphoreGive(g_ring_mutex);
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

void net_init() {
    if (g_ring_mutex == nullptr) {
        g_ring_mutex = xSemaphoreCreateMutex();
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(kApSsid);
    g_server.begin();
    g_server.setNoDelay(true);
}

void handle_net() {
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

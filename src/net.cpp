#include "net.h"
#include "logger.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

namespace {
constexpr const char* kApSsid = "PLTRNK";
constexpr uint16_t kServerPort = 2323;
constexpr size_t kMaxClients = 4;
constexpr size_t kMaxMessageLen = 160;
constexpr uint32_t kClientConnectTimeoutMs = 10000;

WiFiServer g_server(kServerPort);
WiFiClient g_clients[kMaxClients];
bool g_net_enabled = false;
bool g_server_started = false;
NetState g_net_state = NetState::Ap;
char g_sta_ssid[33] = {};
char g_sta_password[65] = {};
uint32_t g_connect_started_ms = 0;

void stop_server_and_clients() {
    if (g_server_started) {
        g_server.stop();
        g_server_started = false;
    }

    for (size_t i = 0; i < kMaxClients; ++i) {
        if (g_clients[i]) {
            g_clients[i].stop();
        }
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
            return;
        }
    }

    // No free slots, reject the incoming client.
    new_client.stop();
}

bool has_connected_clients() {
    bool has_any = false;
    for (size_t i = 0; i < kMaxClients; ++i) {
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

void broadcast_message(const char* message) {
    for (size_t i = 0; i < kMaxClients; ++i) {
        if (g_clients[i] && g_clients[i].connected()) {
            g_clients[i].print(message);
            g_clients[i].print('\n');
        }
    }
}
}  // namespace

void net_init(const AppConfig& config) {
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
            logger_printf("WiFi connected, IP: %s", WiFi.localIP().toString().c_str());
        } else if (millis() - g_connect_started_ms >= kClientConnectTimeoutMs) {
            net_start_ap();
        }
    }

    accept_new_clients();
    if (!has_connected_clients()) {
        return;
    }

    char log_message[kMaxMessageLen];
    while (logger_get_next(log_message, sizeof(log_message))) {
        broadcast_message(log_message);
    }
}

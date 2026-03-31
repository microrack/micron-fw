#include "ota.h"

#include <ArduinoOTA.h>

#include "logger.h"

namespace {
bool g_ota_started = false;
}

void ota_init() {
    g_ota_started = false;
}

void ota_handle(NetState net_state, bool wifi_enabled) {
    if (!wifi_enabled) {
        return;
    }

    if (!g_ota_started && net_state != NetState::Connecting) {
        ArduinoOTA.setHostname("pltrnk-fw");
        ArduinoOTA.onStart([]() { logger_printf("OTA start"); });
        ArduinoOTA.onEnd([]() { logger_printf("OTA end"); });
        ArduinoOTA.onError([](ota_error_t error) { logger_printf("OTA error: %u", static_cast<unsigned>(error)); });
        ArduinoOTA.begin();
        g_ota_started = true;
        logger_printf("OTA ready");
    }

    if (g_ota_started) {
        ArduinoOTA.handle();
    }
}

#pragma once

struct AppConfig {
    bool usb;
    bool wifi;
    char ssid[33];
    char password[65];
};

AppConfig config_init();

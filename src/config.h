#pragma once

struct AppConfig {
    bool usb;
    bool wifi;
    char ssid[33];
    char password[65];
    /** LittleFS config.txt: `legacy` or `current` (see board_pins_init). */
    char hw_version[24];
};

AppConfig config_init();

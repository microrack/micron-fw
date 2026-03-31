#pragma once

struct AppConfig {
    bool usb;
    bool wifi;
};

AppConfig config_init();

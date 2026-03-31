#include "config.h"

#include <Arduino.h>
#include <LittleFS.h>

namespace {
constexpr const char* kConfigPath = "/config.txt";
constexpr const char* kDefaultConfigContent = "usb=0\nwifi=0\n";

AppConfig g_config = {
    .usb = false,
    .wifi = false,
};

bool parse_bool_value(const String& raw_value, bool* out_value) {
    String value = raw_value;
    value.trim();
    value.toLowerCase();

    if (value == "1" || value == "true" || value == "on") {
        *out_value = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "off") {
        *out_value = false;
        return true;
    }
    return false;
}

void parse_line(const String& raw_line) {
    String line = raw_line;
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) {
        return;
    }

    const int sep_pos = line.indexOf('=');
    if (sep_pos <= 0) {
        return;
    }

    String key = line.substring(0, sep_pos);
    String value = line.substring(sep_pos + 1);
    key.trim();
    key.toLowerCase();

    bool parsed_value = false;
    if (!parse_bool_value(value, &parsed_value)) {
        return;
    }

    if (key == "usb") {
        g_config.usb = parsed_value;
        return;
    }
    if (key == "wifi") {
        g_config.wifi = parsed_value;
    }
}

bool ensure_default_config_file() {
    if (LittleFS.exists(kConfigPath)) {
        return true;
    }

    File file = LittleFS.open(kConfigPath, "w");
    if (!file) {
        return false;
    }
    file.print(kDefaultConfigContent);
    file.close();
    return true;
}

void load_config_file() {
    File file = LittleFS.open(kConfigPath, "r");
    if (!file) {
        return;
    }

    while (file.available()) {
        const String line = file.readStringUntil('\n');
        parse_line(line);
    }
    file.close();
}
}  // namespace

AppConfig config_init() {
    g_config = {
        .usb = false,
        .wifi = false,
    };
    if (!LittleFS.begin(true)) {
        return g_config;
    }
    if (!ensure_default_config_file()) {
        return g_config;
    }
    load_config_file();
    return g_config;
}

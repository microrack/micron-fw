#include "config.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

namespace {
constexpr const char* CONFIG_PATH = "/config.txt";
constexpr const char* DEFAULT_CONFIG_CONTENT =
    "usb=0\nwifi=0\nssid=\npassword=\nhw_version=current\n";

AppConfig g_config = {
    .usb = false,
    .wifi = false,
    .ssid = "",
    .password = "",
    .hw_version = "",
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

void copy_string_value(const String& value, char* dst, size_t dst_size) {
    String trimmed = value;
    trimmed.trim();
    strncpy(dst, trimmed.c_str(), dst_size - 1);
    dst[dst_size - 1] = '\0';
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

    if (key == "usb") {
        bool parsed_value = false;
        if (!parse_bool_value(value, &parsed_value)) {
            return;
        }
        g_config.usb = parsed_value;
        return;
    }
    if (key == "wifi") {
        bool parsed_value = false;
        if (!parse_bool_value(value, &parsed_value)) {
            return;
        }
        g_config.wifi = parsed_value;
        return;
    }
    if (key == "ssid") {
        copy_string_value(value, g_config.ssid, sizeof(g_config.ssid));
        return;
    }
    if (key == "password") {
        copy_string_value(value, g_config.password, sizeof(g_config.password));
        return;
    }
    if (key == "hw_version" || key == "hw_verison") {
        copy_string_value(value, g_config.hw_version, sizeof(g_config.hw_version));
        return;
    }
}

bool ensure_default_config_file() {
    if (LittleFS.exists(CONFIG_PATH)) {
        return true;
    }

    File file = LittleFS.open(CONFIG_PATH, "w");
    if (!file) {
        return false;
    }
    file.print(DEFAULT_CONFIG_CONTENT);
    file.close();
    return true;
}

void load_config_file() {
    File file = LittleFS.open(CONFIG_PATH, "r");
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
        .ssid = "",
        .password = "",
        .hw_version = "",
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

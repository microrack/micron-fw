#include "usb_host.h"

extern "C" {
#include "esp_err.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "usb/usb_types_stack.h"
}

#include <stdarg.h>
#include <stdio.h>

struct HostState {
    usb_host_client_handle_t client = nullptr;
    volatile bool has_new_device = false;
    volatile uint8_t new_dev_addr = 0;
};

static HostState g_host;
static UsbHostConfig g_usb_host_config = {};
static SemaphoreHandle_t g_host_installed_sem = nullptr;

static void emit_stack_message(const char* fmt, ...) {
    if (g_usb_host_config.stack_message_cb == nullptr) {
        return;
    }

    char buffer[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    g_usb_host_config.stack_message_cb(buffer);
}

static void usb_daemon_task(void* arg) {
    (void)arg;

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        emit_stack_message("usb_host_install failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    emit_stack_message("USB Host installed");
    if (g_host_installed_sem != nullptr) {
        xSemaphoreGive(g_host_installed_sem);
    }

    while (true) {
        uint32_t event_flags = 0;
        err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err != ESP_OK) {
            emit_stack_message("usb_host_lib_handle_events failed: %s", esp_err_to_name(err));
        }
    }
}

static void client_event_cb(const usb_host_client_event_msg_t* event_msg, void* arg) {
    HostState* st = static_cast<HostState*>(arg);

    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            st->new_dev_addr = event_msg->new_dev.address;
            st->has_new_device = true;
            break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            emit_stack_message("USB device disconnected");
            break;

        default:
            break;
    }
}

static void usb_client_task(void* arg) {
    HostState* st = static_cast<HostState*>(arg);
    if (g_host_installed_sem != nullptr) {
        xSemaphoreTake(g_host_installed_sem, portMAX_DELAY);
    }

    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = st,
        },
    };

    esp_err_t err = usb_host_client_register(&client_config, &st->client);
    if (err != ESP_OK) {
        emit_stack_message("usb_host_client_register failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    emit_stack_message("USB Host client registered");

    while (true) {
        err = usb_host_client_handle_events(st->client, portMAX_DELAY);
        if (err != ESP_OK) {
            emit_stack_message("usb_host_client_handle_events failed: %s", esp_err_to_name(err));
            continue;
        }

        if (st->has_new_device) {
            st->has_new_device = false;

            usb_device_handle_t dev_hdl = nullptr;
            err = usb_host_device_open(st->client, st->new_dev_addr, &dev_hdl);
            if (err != ESP_OK) {
                emit_stack_message("usb_host_device_open failed: %s", esp_err_to_name(err));
                continue;
            }

            const usb_device_desc_t* desc = nullptr;
            err = usb_host_get_device_descriptor(dev_hdl, &desc);
            if (err != ESP_OK) {
                emit_stack_message("usb_host_get_device_descriptor failed: %s", esp_err_to_name(err));
                usb_host_device_close(st->client, dev_hdl);
                continue;
            }

            emit_stack_message(
                "USB device connected: addr=%u VID=%04X PID=%04X class=%02X subclass=%02X proto=%02X",
                st->new_dev_addr,
                desc->idVendor,
                desc->idProduct,
                desc->bDeviceClass,
                desc->bDeviceSubClass,
                desc->bDeviceProtocol
            );

            err = usb_host_device_close(st->client, dev_hdl);
            if (err != ESP_OK) {
                emit_stack_message("usb_host_device_close failed: %s", esp_err_to_name(err));
            }
        }
    }
}

void usb_host_init(const UsbHostConfig& config) {
    g_usb_host_config = config;
    if (g_host_installed_sem == nullptr) {
        g_host_installed_sem = xSemaphoreCreateBinary();
    }
    emit_stack_message("Starting USB Host example...");

    xTaskCreatePinnedToCore(usb_daemon_task, "usb_daemon", 4096, nullptr, 20, nullptr, 0);
    xTaskCreatePinnedToCore(usb_client_task, "usb_client", 4096, &g_host, 20, nullptr, 0);
}

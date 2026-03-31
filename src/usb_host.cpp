#include "usb_host.h"
#include "logger.h"
#include "midi.h"

extern "C" {
#include "esp_err.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "usb/usb_types_ch9.h"
#include "usb/usb_types_stack.h"
}

struct HostState {
    usb_host_client_handle_t client = nullptr;
    volatile bool has_new_device = false;
    volatile uint8_t new_dev_addr = 0;
    volatile bool has_gone_device = false;
    usb_device_handle_t gone_dev_hdl = nullptr;

    usb_device_handle_t dev_hdl = nullptr;
    usb_transfer_t* midi_in_xfer = nullptr;
    uint8_t midi_interface_number = 0;
    uint8_t midi_alternate_setting = 0;
    uint8_t midi_in_ep_addr = 0;
    uint8_t active_dev_addr = 0;
    bool midi_interface_claimed = false;
};

static HostState g_host;
static SemaphoreHandle_t g_host_installed_sem = nullptr;

struct MidiInEndpointInfo {
    uint8_t interface_number = 0;
    uint8_t alternate_setting = 0;
    uint8_t endpoint_addr = 0;
    uint16_t max_packet_size = 0;
    bool found = false;
};

static void log_usb_string_descriptor(const char* label, const usb_str_desc_t* str_desc) {
    if (str_desc == nullptr || str_desc->bLength <= 2) {
        logger_printf("%s: <none>", label);
        return;
    }

    const size_t max_chars = 63;
    char text[max_chars + 1];
    size_t out_idx = 0;
    const size_t char_count = (str_desc->bLength - 2) / 2;

    for (size_t i = 0; i < char_count && out_idx < max_chars; ++i) {
        const uint16_t ch = str_desc->wData[i];
        if (ch >= 0x20 && ch <= 0x7E) {
            text[out_idx++] = static_cast<char>(ch);
        } else {
            text[out_idx++] = '?';
        }
    }
    text[out_idx] = '\0';
    logger_printf("%s: %s", label, text);
}

static bool find_midi_in_endpoint(usb_device_handle_t dev_hdl, MidiInEndpointInfo* out_info) {
    if (out_info == nullptr) {
        return false;
    }
    *out_info = {};

    const usb_config_desc_t* config_desc = nullptr;
    esp_err_t err = usb_host_get_active_config_descriptor(dev_hdl, &config_desc);
    if (err != ESP_OK || config_desc == nullptr) {
        logger_printf("usb_host_get_active_config_descriptor failed: %s", esp_err_to_name(err));
        return false;
    }

    const uint8_t* ptr = config_desc->val;
    const uint8_t* end = config_desc->val + config_desc->wTotalLength;
    bool in_midi_interface = false;
    bool found_midi = false;

    while ((ptr + 2) <= end) {
        const uint8_t len = ptr[0];
        const uint8_t type = ptr[1];
        if (len == 0 || (ptr + len) > end) {
            break;
        }

        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE && len >= sizeof(usb_intf_desc_t)) {
            const usb_intf_desc_t* intf = reinterpret_cast<const usb_intf_desc_t*>(ptr);
            in_midi_interface = (intf->bInterfaceClass == 0x01) && (intf->bInterfaceSubClass == 0x03);
            if (in_midi_interface) {
                found_midi = true;
                out_info->interface_number = intf->bInterfaceNumber;
                out_info->alternate_setting = intf->bAlternateSetting;
                logger_printf(
                    "USB MIDI interface: num=%u alt=%u endpoints=%u",
                    intf->bInterfaceNumber,
                    intf->bAlternateSetting,
                    intf->bNumEndpoints
                );
            }
        } else if (in_midi_interface && type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= sizeof(usb_ep_desc_t)) {
            const usb_ep_desc_t* ep = reinterpret_cast<const usb_ep_desc_t*>(ptr);
            logger_printf(
                "USB MIDI endpoint: addr=0x%02X attr=0x%02X mps=%u interval=%u",
                ep->bEndpointAddress,
                ep->bmAttributes,
                ep->wMaxPacketSize,
                ep->bInterval
            );
            if ((ep->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) != 0 && !out_info->found) {
                out_info->endpoint_addr = ep->bEndpointAddress;
                out_info->max_packet_size = USB_EP_DESC_GET_MPS(ep);
                out_info->found = true;
            }
        }

        ptr += len;
    }

    if (!found_midi) {
        logger_printf("USB MIDI interface not found");
        return false;
    }
    return out_info->found;
}

static void cleanup_midi_stream(HostState* st, bool close_device) {
    if (st == nullptr) {
        return;
    }

    if (st->midi_in_xfer != nullptr) {
        usb_host_transfer_free(st->midi_in_xfer);
        st->midi_in_xfer = nullptr;
    }

    if (close_device && st->midi_interface_claimed && st->dev_hdl != nullptr && st->client != nullptr) {
        const esp_err_t rel_err = usb_host_interface_release(st->client, st->dev_hdl, st->midi_interface_number);
        if (rel_err != ESP_OK) {
            logger_printf("usb_host_interface_release failed: %s", esp_err_to_name(rel_err));
        }
    }
    st->midi_interface_claimed = false;

    if (close_device && st->dev_hdl != nullptr && st->client != nullptr) {
        const esp_err_t close_err = usb_host_device_close(st->client, st->dev_hdl);
        if (close_err != ESP_OK) {
            logger_printf("usb_host_device_close failed: %s", esp_err_to_name(close_err));
        }
    }

    st->dev_hdl = nullptr;
    st->midi_interface_number = 0;
    st->midi_alternate_setting = 0;
    st->midi_in_ep_addr = 0;
    st->active_dev_addr = 0;
}

static void midi_in_transfer_cb(usb_transfer_t* transfer) {
    if (transfer == nullptr) {
        return;
    }
    HostState* st = static_cast<HostState*>(transfer->context);
    if (st == nullptr) {
        return;
    }

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        for (int i = 0; (i + 3) < transfer->actual_num_bytes; i += 4) {
            midi_on_usb_packet(&transfer->data_buffer[i]);
        }
        if ((transfer->actual_num_bytes % 4) != 0) {
            logger_printf("USB MIDI short packet tail: %d bytes", transfer->actual_num_bytes % 4);
        }
    } else if (transfer->status == USB_TRANSFER_STATUS_NO_DEVICE) {
        logger_printf("USB MIDI IN transfer: device gone");
        return;
    } else {
        logger_printf("USB MIDI IN transfer status=%d", static_cast<int>(transfer->status));
    }

    transfer->num_bytes = static_cast<int>(transfer->data_buffer_size);
    const esp_err_t err = usb_host_transfer_submit(transfer);
    if (err != ESP_OK) {
        logger_printf("usb_host_transfer_submit(IN resubmit) failed: %s", esp_err_to_name(err));
    }
}

static bool setup_midi_stream(HostState* st) {
    MidiInEndpointInfo midi_info;
    if (!find_midi_in_endpoint(st->dev_hdl, &midi_info)) {
        logger_printf("USB MIDI IN endpoint not found");
        return false;
    }

    esp_err_t err = usb_host_interface_claim(st->client, st->dev_hdl, midi_info.interface_number, midi_info.alternate_setting);
    if (err != ESP_OK) {
        logger_printf("usb_host_interface_claim failed: %s", esp_err_to_name(err));
        return false;
    }
    st->midi_interface_claimed = true;
    st->midi_interface_number = midi_info.interface_number;
    st->midi_alternate_setting = midi_info.alternate_setting;
    st->midi_in_ep_addr = midi_info.endpoint_addr;

    const size_t transfer_size = (midi_info.max_packet_size >= 4) ? midi_info.max_packet_size : 64;
    err = usb_host_transfer_alloc(transfer_size, 0, &st->midi_in_xfer);
    if (err != ESP_OK || st->midi_in_xfer == nullptr) {
        logger_printf("usb_host_transfer_alloc failed: %s", esp_err_to_name(err));
        cleanup_midi_stream(st, false);
        return false;
    }

    st->midi_in_xfer->device_handle = st->dev_hdl;
    st->midi_in_xfer->bEndpointAddress = st->midi_in_ep_addr;
    st->midi_in_xfer->callback = midi_in_transfer_cb;
    st->midi_in_xfer->context = st;
    st->midi_in_xfer->num_bytes = static_cast<int>(st->midi_in_xfer->data_buffer_size);

    err = usb_host_transfer_submit(st->midi_in_xfer);
    if (err != ESP_OK) {
        logger_printf("usb_host_transfer_submit(IN) failed: %s", esp_err_to_name(err));
        cleanup_midi_stream(st, false);
        return false;
    }

    logger_printf(
        "USB MIDI IN streaming started: if=%u alt=%u ep=0x%02X mps=%u",
        st->midi_interface_number,
        st->midi_alternate_setting,
        st->midi_in_ep_addr,
        static_cast<unsigned>(midi_info.max_packet_size)
    );
    return true;
}

static void usb_daemon_task(void* arg) {
    (void)arg;

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK) {
        logger_printf("usb_host_install failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    logger_printf("USB Host installed");
    if (g_host_installed_sem != nullptr) {
        xSemaphoreGive(g_host_installed_sem);
    }

    while (true) {
        uint32_t event_flags = 0;
        err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err != ESP_OK) {
            logger_printf("usb_host_lib_handle_events failed: %s", esp_err_to_name(err));
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
            logger_printf("USB device disconnected");
            st->gone_dev_hdl = event_msg->dev_gone.dev_hdl;
            st->has_gone_device = true;
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
        logger_printf("usb_host_client_register failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    logger_printf("USB Host client registered");

    while (true) {
        err = usb_host_client_handle_events(st->client, portMAX_DELAY);
        if (err != ESP_OK) {
            logger_printf("usb_host_client_handle_events failed: %s", esp_err_to_name(err));
            continue;
        }

        if (st->has_new_device) {
            st->has_new_device = false;
            usb_device_handle_t new_dev_hdl = nullptr;
            err = usb_host_device_open(st->client, st->new_dev_addr, &new_dev_hdl);
            if (err != ESP_OK) {
                logger_printf("usb_host_device_open failed: %s", esp_err_to_name(err));
                continue;
            }

            const usb_device_desc_t* desc = nullptr;
            err = usb_host_get_device_descriptor(new_dev_hdl, &desc);
            if (err != ESP_OK) {
                logger_printf("usb_host_get_device_descriptor failed: %s", esp_err_to_name(err));
                usb_host_device_close(st->client, new_dev_hdl);
                continue;
            }

            logger_printf(
                "USB device connected: addr=%u VID=%04X PID=%04X class=%02X subclass=%02X proto=%02X",
                st->new_dev_addr,
                desc->idVendor,
                desc->idProduct,
                desc->bDeviceClass,
                desc->bDeviceSubClass,
                desc->bDeviceProtocol
            );
            usb_device_info_t dev_info;
            err = usb_host_device_info(new_dev_hdl, &dev_info);
            if (err == ESP_OK) {
                log_usb_string_descriptor("USB manufacturer", dev_info.str_desc_manufacturer);
                log_usb_string_descriptor("USB product", dev_info.str_desc_product);
                log_usb_string_descriptor("USB serial", dev_info.str_desc_serial_num);
            } else {
                logger_printf("usb_host_device_info failed: %s", esp_err_to_name(err));
            }

            MidiInEndpointInfo midi_info;
            const bool has_midi_in = find_midi_in_endpoint(new_dev_hdl, &midi_info);
            if (st->dev_hdl != nullptr) {
                if (has_midi_in) {
                    logger_printf(
                        "MIDI device addr=%u ignored: already streaming from addr=%u",
                        st->new_dev_addr,
                        st->active_dev_addr
                    );
                }
                usb_host_device_close(st->client, new_dev_hdl);
                continue;
            }

            if (!has_midi_in) {
                logger_printf("USB MIDI IN endpoint not found");
                usb_host_device_close(st->client, new_dev_hdl);
                continue;
            }

            st->dev_hdl = new_dev_hdl;
            st->active_dev_addr = st->new_dev_addr;
            if (!setup_midi_stream(st)) {
                cleanup_midi_stream(st, true);
            }
        }

        if (st->has_gone_device) {
            st->has_gone_device = false;
            if (st->dev_hdl == st->gone_dev_hdl) {
                cleanup_midi_stream(st, false);
            }
        }
    }
}

void usb_host_init(const UsbHostConfig& config) {
    (void)config;
    if (g_host_installed_sem == nullptr) {
        g_host_installed_sem = xSemaphoreCreateBinary();
    }
    logger_printf("Starting USB Host example...");

    xTaskCreatePinnedToCore(usb_daemon_task, "usb_daemon", 4096, nullptr, 20, nullptr, 0);
    xTaskCreatePinnedToCore(usb_client_task, "usb_client", 4096, &g_host, 20, nullptr, 0);
}

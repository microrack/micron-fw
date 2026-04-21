#include "gadget_handler.h"

#include "default_gadget_handler.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
}

namespace {
GadgetHandler* g_handlers[GADGET_HANDLER_MAX_COUNT] = {};
size_t g_handler_count = 0;
GadgetHandler* g_current_handler = nullptr;

portMUX_TYPE g_lifecycle_spin = portMUX_INITIALIZER_UNLOCKED;
bool g_pending_enter = false;
bool g_pending_exit = false;
}  // namespace

bool gadget_handler_register(GadgetHandler* handler) {
    if (handler == nullptr || g_handler_count >= GADGET_HANDLER_MAX_COUNT) {
        return false;
    }
    g_handlers[g_handler_count++] = handler;
    return true;
}

void gadget_handler_reset_registry() {
    GadgetHandler* to_exit = nullptr;
    portENTER_CRITICAL(&g_lifecycle_spin);
    for (size_t i = 0; i < g_handler_count; ++i) {
        g_handlers[i] = nullptr;
    }
    g_handler_count = 0;
    g_pending_enter = false;
    g_pending_exit = false;
    to_exit = g_current_handler;
    g_current_handler = nullptr;
    portEXIT_CRITICAL(&g_lifecycle_spin);

    if (to_exit != nullptr) {
        to_exit->exit();
    }
}

size_t gadget_handler_count() {
    return g_handler_count;
}

GadgetHandler* gadget_handler_at(size_t idx) {
    if (idx >= g_handler_count) {
        return nullptr;
    }
    return g_handlers[idx];
}

void gadget_handler_set_current(GadgetHandler* handler) {
    portENTER_CRITICAL(&g_lifecycle_spin);
    g_current_handler = handler;
    portEXIT_CRITICAL(&g_lifecycle_spin);
}

GadgetHandler* gadget_handler_current() {
    portENTER_CRITICAL(&g_lifecycle_spin);
    GadgetHandler* h = g_current_handler;
    portEXIT_CRITICAL(&g_lifecycle_spin);
    return h;
}

GadgetHandler& gadget_handler_get() {
    portENTER_CRITICAL(&g_lifecycle_spin);
    GadgetHandler* h = g_current_handler;
    portEXIT_CRITICAL(&g_lifecycle_spin);
    return (h != nullptr) ? *h : default_gadget_handler_get();
}

void gadget_handler_on_usb_gadget_ready(GadgetHandler* handler) {
    portENTER_CRITICAL(&g_lifecycle_spin);
    g_current_handler = handler;
    g_pending_enter = (handler != nullptr);
    g_pending_exit = false;
    portEXIT_CRITICAL(&g_lifecycle_spin);
}

void gadget_handler_on_usb_disconnect() {
    portENTER_CRITICAL(&g_lifecycle_spin);
    g_pending_exit = true;
    g_pending_enter = false;
    portEXIT_CRITICAL(&g_lifecycle_spin);
}

void gadget_handler_poll_lifecycle() {
    GadgetHandler* exit_target = nullptr;
    bool do_enter = false;
    GadgetHandler* enter_target = nullptr;

    portENTER_CRITICAL(&g_lifecycle_spin);
    if (g_pending_exit) {
        g_pending_exit = false;
        g_pending_enter = false;
        exit_target = g_current_handler;
        g_current_handler = nullptr;
    } else if (g_pending_enter) {
        g_pending_enter = false;
        do_enter = true;
        enter_target = g_current_handler;
    }
    portEXIT_CRITICAL(&g_lifecycle_spin);

    if (exit_target != nullptr) {
        exit_target->exit();
        return;
    }

    if (do_enter && enter_target != nullptr) {
        enter_target->enter();
    }
}

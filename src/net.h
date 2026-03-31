#pragma once

#include <stdint.h>

#include "config.h"

enum class NetState : uint8_t {
    Ap = 0,
    Client = 1,
    Connecting = 2,
};

void net_init(const AppConfig& config);
void net_start_ap();
void net_start_client();
NetState net_get_state();
void handle_net();

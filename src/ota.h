#pragma once

#include <stdbool.h>

#include "net.h"

void ota_init();
void ota_handle(NetState net_state, bool wifi_enabled);

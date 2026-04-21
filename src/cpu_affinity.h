#pragma once

#include "freertos/FreeRTOS.h"

// Core where application-created tasks are pinned (second core on ESP32-S3: index 1).
// Arduino loop() is expected to run on the same core.
static constexpr BaseType_t kAppTaskCore = 1;

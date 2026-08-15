#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t telemetry_start(const char *device_id, uint32_t interval_seconds);

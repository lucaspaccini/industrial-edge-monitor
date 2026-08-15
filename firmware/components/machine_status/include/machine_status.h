#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "device_config.h"

typedef enum
{
    MACHINE_STATUS_RUNNING = 0,
    MACHINE_STATUS_STOPPED,
    MACHINE_STATUS_UNKNOWN
} machine_status_t;

esp_err_t machine_status_init(const device_config_t *configuration);

bool machine_status_is_enabled(void);

esp_err_t machine_status_get(machine_status_t *status);

const char *machine_status_to_string(machine_status_t status);

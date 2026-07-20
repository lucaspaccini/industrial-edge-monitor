#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#define SYSTEM_TIME_TIMESTAMP_SIZE 21

esp_err_t system_time_init(void);

bool system_time_is_valid(void);

esp_err_t system_time_get_timestamp(
    char *buffer,
    size_t buffer_size
);

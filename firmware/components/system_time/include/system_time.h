#pragma once

#include <stddef.h>

#include "esp_err.h"


esp_err_t system_time_get_timestamp(
    char *buffer,
    size_t buffer_size
);
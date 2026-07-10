#pragma once

#include <stddef.h>

#include "telemetry_model.h"

int telemetry_to_json(
    const telemetry_t *telemetry,
    char *buffer,
    size_t buffer_size
);
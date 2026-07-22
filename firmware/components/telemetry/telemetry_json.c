#include "telemetry_json.h"

#include <stdio.h>

int telemetry_to_json(
    const telemetry_t *telemetry,
    char *buffer,
    size_t buffer_size
)
{
    if (telemetry == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "{"
        "\"device_id\":\"%s\","
        "\"timestamp\":\"%s\","
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"machine_status\":\"%s\""
        "}",
        telemetry->device_id,
        telemetry->timestamp,
        telemetry->temperature,
        telemetry->humidity,
        telemetry->machine_status
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return -1;
    }

    return written;
}

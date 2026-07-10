#include "system_time.h"

#include <stdio.h>

#include "esp_timer.h"


esp_err_t system_time_get_timestamp(
    char *buffer,
    size_t buffer_size
)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t uptime_ms = esp_timer_get_time() / 1000;

    int written = snprintf(
        buffer,
        buffer_size,
        "uptime-%lld-ms",
        uptime_ms
    );

    if (written < 0 || (size_t)written >= buffer_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}
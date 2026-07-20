#include "system_time.h"

#include <stdatomic.h>
#include <time.h>

#include "config.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "system_time";

static atomic_bool time_valid = ATOMIC_VAR_INIT(false);

static void time_sync_notification_cb(struct timeval *time_value)
{
    (void)time_value;
    atomic_store(&time_valid, true);
    ESP_LOGI(TAG, "System clock synchronized with SNTP server");
}

esp_err_t system_time_init(void)
{
    ESP_LOGI(
        TAG,
        "Starting SNTP synchronization with server: %s",
        CONFIG_SNTP_SERVER
    );

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_SERVER);
    config.sync_cb = time_sync_notification_cb;

    esp_err_t result = esp_netif_sntp_init(&config);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(result));
        return result;
    }

    result = esp_netif_sntp_sync_wait(
        pdMS_TO_TICKS(CONFIG_SNTP_SYNC_TIMEOUT_MS)
    );

    if (result == ESP_ERR_TIMEOUT) {
        ESP_LOGW(
            TAG,
            "Initial SNTP synchronization timed out after %d ms; continuing in background",
            CONFIG_SNTP_SYNC_TIMEOUT_MS
        );
    } else if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Initial SNTP synchronization incomplete: %s",
            esp_err_to_name(result)
        );
    }

    return result;
}

bool system_time_is_valid(void)
{
    return atomic_load(&time_valid);
}

esp_err_t system_time_get_timestamp(
    char *buffer,
    size_t buffer_size
)
{
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer_size < SYSTEM_TIME_TIMESTAMP_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!system_time_is_valid()) {
        return ESP_ERR_INVALID_STATE;
    }

    time_t now;
    struct tm utc_time;

    time(&now);

    if (gmtime_r(&now, &utc_time) == NULL) {
        return ESP_FAIL;
    }

    size_t written = strftime(
        buffer,
        buffer_size,
        "%Y-%m-%dT%H:%M:%SZ",
        &utc_time
    );

    if (written == 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

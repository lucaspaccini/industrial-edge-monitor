#include "telemetry.h"

#include "esp_log.h"

static const char *TAG = "telemetry";

void telemetry_start(void)
{
    ESP_LOGI(TAG, "Initializing Telemetry");
    ESP_LOGI(TAG, "Publish period: %d ms", CONFIG_TELEMETRY_PUBLISH_PERIOD_MS);
}
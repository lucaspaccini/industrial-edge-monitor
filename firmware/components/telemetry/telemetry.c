#include "telemetry.h"

#include "esp_log.h"
#include "mqtt_client_app.h"

static const char *TAG = "telemetry";

void telemetry_start(void)
{
    ESP_LOGI(TAG, "Initializing Telemetry");
    ESP_LOGI(TAG, "Publish period: %d ms", CONFIG_TELEMETRY_PUBLISH_PERIOD_MS);

    const char *payload =
    "{"
    "\"timestamp\":\"2026-07-06T20:00:00Z\","
    "\"temperature\":25.0,"
    "\"humidity\":50.0,"
    "\"machine_status\":\"RUNNING\""
    "}";

    mqtt_publish_telemetry(payload);
}
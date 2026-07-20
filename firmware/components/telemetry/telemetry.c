#include "telemetry.h"

#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client_app.h"
#include "system_time.h"
#include "telemetry_json.h"
#include "telemetry_model.h"

static const char *TAG = "telemetry";

static TaskHandle_t telemetry_task_handle = NULL;

static void telemetry_task(void *parameters)
{
    char payload[256];

    while (true) {
        if (!system_time_is_valid()) {
            ESP_LOGW(
                TAG,
                "System clock not synchronized, skipping telemetry publish"
            );
            vTaskDelay(pdMS_TO_TICKS(CONFIG_TELEMETRY_PUBLISH_PERIOD_MS));
            continue;
        }

        telemetry_t telemetry;

        telemetry_model_create(&telemetry);

        int payload_length = telemetry_to_json(
            &telemetry,
            payload,
            sizeof(payload)
        );

        if (payload_length < 0) {
            ESP_LOGE(TAG, "Failed to serialize telemetry");
        } else {
            ESP_LOGI(TAG, "Publishing telemetry: %s", payload);
            mqtt_publish_telemetry(payload);
        }

        vTaskDelay(
            pdMS_TO_TICKS(CONFIG_TELEMETRY_PUBLISH_PERIOD_MS)
        );
    }
}

void telemetry_start(void)
{
    if (telemetry_task_handle != NULL) {
        ESP_LOGW(TAG, "Telemetry task already running");
        return;
    }

    ESP_LOGI(
        TAG,
        "Starting telemetry task with period %d ms",
        CONFIG_TELEMETRY_PUBLISH_PERIOD_MS
    );

    BaseType_t result = xTaskCreate(
        telemetry_task,
        "telemetry_task",
        4096,
        NULL,
        5,
        &telemetry_task_handle
    );

    if (result != pdPASS) {
        telemetry_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create telemetry task");
    }
}

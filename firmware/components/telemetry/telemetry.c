#include "telemetry.h"

#include "config.h"
#include "device_health.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client_app.h"
#include "system_time.h"
#include "telemetry_json.h"
#include "telemetry_model.h"

static const char *TAG = "telemetry";

static TaskHandle_t telemetry_task_handle = NULL;

static const char *get_timestamp(char *buffer, size_t buffer_size)
{
    return system_time_get_timestamp(buffer, buffer_size) == ESP_OK
        ? buffer
        : NULL;
}

static void update_sensor_failure(esp_err_t model_result, const char *timestamp)
{
    device_health_error_t error = DEVICE_HEALTH_ERROR_READ_FAILED;

    if (model_result == TELEMETRY_MODEL_ERR_VALUE_NOT_FINITE) {
        error = DEVICE_HEALTH_ERROR_VALUE_NOT_FINITE;
    } else if (model_result == TELEMETRY_MODEL_ERR_TEMPERATURE_RANGE) {
        error = DEVICE_HEALTH_ERROR_TEMPERATURE_OUT_OF_RANGE;
    } else if (model_result == TELEMETRY_MODEL_ERR_HUMIDITY_RANGE) {
        error = DEVICE_HEALTH_ERROR_HUMIDITY_OUT_OF_RANGE;
    }

    device_health_update_component(
        DEVICE_HEALTH_COMPONENT_SENSOR,
        DEVICE_HEALTH_COMPONENT_FAULT,
        error,
        timestamp
    );
}

static void update_machine_status_health(const telemetry_t *telemetry, const char *timestamp)
{
    if (telemetry->machine_status_result == ESP_OK) {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
            DEVICE_HEALTH_COMPONENT_HEALTHY,
            DEVICE_HEALTH_ERROR_NONE,
            timestamp
        );
    } else if (telemetry->machine_status_result == ESP_ERR_NOT_SUPPORTED) {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
            DEVICE_HEALTH_COMPONENT_UNKNOWN,
            DEVICE_HEALTH_ERROR_NONE,
            timestamp
        );
    } else {
        device_health_increment_machine_status_errors();
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
            DEVICE_HEALTH_COMPONENT_FAULT,
            DEVICE_HEALTH_ERROR_MACHINE_STATUS_UNAVAILABLE,
            timestamp
        );
    }
}

static void telemetry_collect_and_publish(void)
{
    char payload[256];
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    const char *timestamp_value = get_timestamp(timestamp, sizeof(timestamp));

    if (!system_time_is_valid()) {
        device_health_increment_samples_rejected();
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
            DEVICE_HEALTH_COMPONENT_DEGRADED,
            DEVICE_HEALTH_ERROR_TIME_NOT_SYNCHRONIZED,
            NULL
        );
        ESP_LOGW(TAG, "Clock not synchronized; telemetry cycle skipped");
        return;
    }

    telemetry_t telemetry;
    esp_err_t model_result = telemetry_model_create(&telemetry);

    if (model_result != ESP_OK) {
        device_health_increment_samples_rejected();

        if (model_result == TELEMETRY_MODEL_ERR_SENSOR_READ) {
            update_sensor_failure(model_result, timestamp_value);
            ESP_LOGE(TAG, "Telemetry acquisition error; sample rejected");
        } else if (model_result == TELEMETRY_MODEL_ERR_VALUE_NOT_FINITE
            || model_result == TELEMETRY_MODEL_ERR_TEMPERATURE_RANGE
            || model_result == TELEMETRY_MODEL_ERR_HUMIDITY_RANGE) {
            update_sensor_failure(model_result, timestamp_value);
            ESP_LOGE(TAG, "Invalid sensor measurement; sample rejected");
        } else if (model_result == TELEMETRY_MODEL_ERR_TIMESTAMP) {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
                DEVICE_HEALTH_COMPONENT_DEGRADED,
                DEVICE_HEALTH_ERROR_TIME_NOT_SYNCHRONIZED,
                NULL
            );
            ESP_LOGE(TAG, "UTC timestamp generation error; sample rejected");
        } else {
            ESP_LOGE(
                TAG,
                "Telemetry model error: %s; sample rejected",
                esp_err_to_name(model_result)
            );
        }

        return;
    }

    device_health_increment_samples_ok();
    device_health_update_component(
        DEVICE_HEALTH_COMPONENT_SENSOR,
        DEVICE_HEALTH_COMPONENT_HEALTHY,
        DEVICE_HEALTH_ERROR_NONE,
        timestamp_value
    );
    device_health_update_component(
        DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
        DEVICE_HEALTH_COMPONENT_HEALTHY,
        DEVICE_HEALTH_ERROR_NONE,
        timestamp_value
    );
    update_machine_status_health(&telemetry, timestamp_value);

    int payload_length = telemetry_to_json(
        &telemetry,
        payload,
        sizeof(payload)
    );

    if (payload_length < 0) {
        device_health_increment_publish_failed();
        ESP_LOGE(TAG, "Telemetry serialization error; publication skipped");
        return;
    }

    ESP_LOGD(TAG, "Publishing telemetry: %s", payload);

    esp_err_t publish_result = mqtt_publish_telemetry(payload);

    if (publish_result == ESP_OK) {
        device_health_increment_publish_ok();
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MQTT,
            DEVICE_HEALTH_COMPONENT_HEALTHY,
            DEVICE_HEALTH_ERROR_NONE,
            timestamp_value
        );
    } else {
        device_health_increment_publish_failed();

        if (publish_result == ESP_ERR_INVALID_STATE) {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_MQTT,
                DEVICE_HEALTH_COMPONENT_DEGRADED,
                DEVICE_HEALTH_ERROR_MQTT_DISCONNECTED,
                timestamp_value
            );
            ESP_LOGW(TAG, "MQTT not connected; publication skipped");
        } else {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_MQTT,
                DEVICE_HEALTH_COMPONENT_FAULT,
                DEVICE_HEALTH_ERROR_PUBLISH_FAILED,
                timestamp_value
            );
            ESP_LOGE(
                TAG,
                "MQTT publication error: %s",
                esp_err_to_name(publish_result)
            );
        }
    }
}

static void telemetry_task(void *parameters)
{
    (void)parameters;

    while (true) {
        telemetry_collect_and_publish();

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

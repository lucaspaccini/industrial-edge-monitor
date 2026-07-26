#include "device_health_service.h"

#include <inttypes.h>
#include <stdio.h>

#include "config.h"
#include "device_health.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client_app.h"
#include "system_time.h"

static const char *TAG = "device_health_service";
static TaskHandle_t task_handle = NULL;

#define HEALTH_PAYLOAD_SIZE 1536
#define AVAILABILITY_PAYLOAD_SIZE 160

static void timestamp_or_null(
    const device_health_component_snapshot_t *component,
    char *buffer,
    size_t buffer_size
)
{
    if (component->has_updated_at) {
        snprintf(buffer, buffer_size, "\"%s\"", component->updated_at);
    } else {
        snprintf(buffer, buffer_size, "null");
    }
}

static int serialize_health(
    const device_health_snapshot_t *snapshot,
    char *buffer,
    size_t buffer_size
)
{
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    char timestamp_json[SYSTEM_TIME_TIMESTAMP_SIZE + 3];

    if (system_time_get_timestamp(timestamp, sizeof(timestamp)) == ESP_OK) {
        snprintf(timestamp_json, sizeof(timestamp_json), "\"%s\"", timestamp);
    } else {
        snprintf(timestamp_json, sizeof(timestamp_json), "null");
    }

    char updated_at[DEVICE_HEALTH_COMPONENT_COUNT][SYSTEM_TIME_TIMESTAMP_SIZE + 3];
    char wifi_rssi[16];

    for (size_t index = 0; index < DEVICE_HEALTH_COMPONENT_COUNT; ++index) {
        timestamp_or_null(&snapshot->components[index], updated_at[index], sizeof(updated_at[index]));
    }

    if (snapshot->metrics.has_wifi_rssi) {
        snprintf(wifi_rssi, sizeof(wifi_rssi), "%" PRId32, snapshot->metrics.wifi_rssi_dbm);
    } else {
        snprintf(wifi_rssi, sizeof(wifi_rssi), "null");
    }

    const device_health_component_snapshot_t *sensor =
        &snapshot->components[DEVICE_HEALTH_COMPONENT_SENSOR];
    const device_health_component_snapshot_t *machine =
        &snapshot->components[DEVICE_HEALTH_COMPONENT_MACHINE_STATUS];
    const device_health_component_snapshot_t *time =
        &snapshot->components[DEVICE_HEALTH_COMPONENT_SYSTEM_TIME];
    const device_health_component_snapshot_t *mqtt =
        &snapshot->components[DEVICE_HEALTH_COMPONENT_MQTT];

    return snprintf(
        buffer,
        buffer_size,
        "{\"schema_version\":1,\"device_id\":\"%s\",\"timestamp\":%s,"
        "\"status\":\"%s\",\"availability\":\"%s\",\"components\":{"
        "\"sensor\":{\"status\":\"%s\",\"error_code\":\"%s\",\"updated_at\":%s},"
        "\"machine_status\":{\"status\":\"%s\",\"error_code\":\"%s\",\"updated_at\":%s},"
        "\"system_time\":{\"status\":\"%s\",\"error_code\":\"%s\",\"updated_at\":%s},"
        "\"mqtt\":{\"status\":\"%s\",\"error_code\":\"%s\",\"updated_at\":%s}},"
        "\"counters\":{\"samples_ok\":%" PRIu64 ",\"samples_rejected\":%" PRIu64
        ",\"publish_ok\":%" PRIu64 ",\"publish_failed\":%" PRIu64
        ",\"machine_status_errors\":%" PRIu64 "},"
        "\"metrics\":{\"uptime_seconds\":%" PRIu64
        ",\"free_heap_bytes\":%" PRIu32 ",\"minimum_free_heap_bytes\":%" PRIu32
        ",\"wifi_rssi_dbm\":%s}}",
        CONFIG_DEVICE_ID,
        timestamp_json,
        device_health_overall_status_to_string(snapshot->status),
        device_availability_to_string(snapshot->availability),
        device_health_component_status_to_string(sensor->status),
        device_health_error_to_string(sensor->error),
        updated_at[DEVICE_HEALTH_COMPONENT_SENSOR],
        device_health_component_status_to_string(machine->status),
        device_health_error_to_string(machine->error),
        updated_at[DEVICE_HEALTH_COMPONENT_MACHINE_STATUS],
        device_health_component_status_to_string(time->status),
        device_health_error_to_string(time->error),
        updated_at[DEVICE_HEALTH_COMPONENT_SYSTEM_TIME],
        device_health_component_status_to_string(mqtt->status),
        device_health_error_to_string(mqtt->error),
        updated_at[DEVICE_HEALTH_COMPONENT_MQTT],
        snapshot->counters.samples_ok,
        snapshot->counters.samples_rejected,
        snapshot->counters.publish_ok,
        snapshot->counters.publish_failed,
        snapshot->counters.machine_status_errors,
        snapshot->metrics.uptime_seconds,
        snapshot->metrics.free_heap_bytes,
        snapshot->metrics.minimum_free_heap_bytes,
        wifi_rssi
    );
}

static void update_metrics(void)
{
    device_health_metrics_t metrics = {
        .uptime_seconds = (uint64_t)(esp_timer_get_time() / 1000000),
        .free_heap_bytes = esp_get_free_heap_size(),
        .minimum_free_heap_bytes = esp_get_minimum_free_heap_size(),
    };

    wifi_ap_record_t access_point;

    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        metrics.has_wifi_rssi = true;
        metrics.wifi_rssi_dbm = access_point.rssi;
    }

    device_health_set_metrics(&metrics);
}

static void current_timestamp(char *buffer, size_t buffer_size, const char **value)
{
    *value = system_time_get_timestamp(buffer, buffer_size) == ESP_OK ? buffer : NULL;
}

static void update_runtime_components(void)
{
    static bool mqtt_state_observed = false;
    static bool mqtt_was_connected = false;
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    const char *timestamp_value;
    current_timestamp(timestamp, sizeof(timestamp), &timestamp_value);

    if (system_time_is_valid()) {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
            DEVICE_HEALTH_COMPONENT_HEALTHY,
            DEVICE_HEALTH_ERROR_NONE,
            timestamp_value
        );
    } else {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
            DEVICE_HEALTH_COMPONENT_DEGRADED,
            DEVICE_HEALTH_ERROR_TIME_NOT_SYNCHRONIZED,
            NULL
        );
    }

    bool mqtt_connected = mqtt_is_connected();

    if (mqtt_connected) {
        device_health_set_availability(DEVICE_AVAILABILITY_ONLINE);
        if (!mqtt_state_observed || !mqtt_was_connected) {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_MQTT,
                DEVICE_HEALTH_COMPONENT_HEALTHY,
                DEVICE_HEALTH_ERROR_NONE,
                timestamp_value
            );
        }
    } else {
        device_health_set_availability(DEVICE_AVAILABILITY_OFFLINE);
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MQTT,
            DEVICE_HEALTH_COMPONENT_DEGRADED,
            DEVICE_HEALTH_ERROR_MQTT_DISCONNECTED,
            timestamp_value
        );
    }

    mqtt_state_observed = true;
    mqtt_was_connected = mqtt_connected;
}

static esp_err_t publish_online_availability(void)
{
    char payload[AVAILABILITY_PAYLOAD_SIZE];
    int written = snprintf(
        payload,
        sizeof(payload),
        "{\"schema_version\":1,\"device_id\":\"%s\",\"status\":\"online\"}",
        CONFIG_DEVICE_ID
    );

    if (written < 0 || (size_t)written >= sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return mqtt_publish_availability(payload);
}

static esp_err_t publish_health_snapshot(void)
{
    device_health_snapshot_t snapshot;
    esp_err_t result = device_health_get_snapshot(&snapshot);

    if (result != ESP_OK) {
        return result;
    }

    char payload[HEALTH_PAYLOAD_SIZE];
    int written = serialize_health(&snapshot, payload, sizeof(payload));

    if (written < 0 || (size_t)written >= sizeof(payload)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return mqtt_publish_health(payload);
}

static void record_health_publish_failure(void)
{
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    const char *timestamp_value;
    current_timestamp(timestamp, sizeof(timestamp), &timestamp_value);

    device_health_increment_publish_failed();
    device_health_update_component(
        DEVICE_HEALTH_COMPONENT_MQTT,
        DEVICE_HEALTH_COMPONENT_FAULT,
        DEVICE_HEALTH_ERROR_PUBLISH_FAILED,
        timestamp_value
    );
}

static void record_health_publish_success(void)
{
    if (!mqtt_is_connected()) {
        return;
    }

    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    const char *timestamp_value;
    current_timestamp(timestamp, sizeof(timestamp), &timestamp_value);

    device_health_update_component(
        DEVICE_HEALTH_COMPONENT_MQTT,
        DEVICE_HEALTH_COMPONENT_HEALTHY,
        DEVICE_HEALTH_ERROR_NONE,
        timestamp_value
    );
}

static void health_task(void *parameters)
{
    (void)parameters;

    TickType_t last_heartbeat = 0;
    uint32_t last_attempted_revision = 0;
    uint32_t last_connection_generation = 0;
    bool health_retry_pending = false;

    while (true) {
        update_runtime_components();
        update_metrics();

        device_health_snapshot_t snapshot;
        device_health_get_snapshot(&snapshot);

        TickType_t now = xTaskGetTickCount();
        uint32_t generation = mqtt_connection_generation();
        bool reconnected = generation != last_connection_generation;
        bool heartbeat_due = (now - last_heartbeat) >=
            pdMS_TO_TICKS(CONFIG_DEVICE_HEALTH_PUBLISH_PERIOD_MS);
        bool state_changed = snapshot.state_revision != last_attempted_revision;

        if (mqtt_is_connected()
            && (reconnected || heartbeat_due || state_changed || health_retry_pending)) {
            bool retry_attempt = health_retry_pending;
            health_retry_pending = false;

            if (reconnected) {
                esp_err_t availability_result = publish_online_availability();

                if (availability_result != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to publish online availability: %s", esp_err_to_name(availability_result));
                }
            }

            esp_err_t result = publish_health_snapshot();

            if (result != ESP_OK) {
                record_health_publish_failure();
                ESP_LOGE(TAG, "Failed to publish device health: %s", esp_err_to_name(result));

                if (!retry_attempt && mqtt_is_connected()) {
                    health_retry_pending = true;
                }

                device_health_snapshot_t failed_snapshot;

                if (device_health_get_snapshot(&failed_snapshot) == ESP_OK) {
                    last_attempted_revision = failed_snapshot.state_revision;
                } else {
                    last_attempted_revision = snapshot.state_revision;
                }
            } else {
                record_health_publish_success();
                last_attempted_revision = snapshot.state_revision;
            }

            last_heartbeat = now;
        }

        last_connection_generation = generation;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DEVICE_HEALTH_STATE_POLL_PERIOD_MS));
    }
}

esp_err_t device_health_service_start(void)
{
    if (task_handle != NULL) {
        return ESP_OK;
    }

    BaseType_t result = xTaskCreate(
        health_task,
        "device_health",
        6144,
        NULL,
        4,
        &task_handle
    );

    if (result != pdPASS) {
        task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

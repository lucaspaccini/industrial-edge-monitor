#include "device_health.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static device_health_snapshot_t state;
static StaticSemaphore_t mutex_buffer;
static SemaphoreHandle_t mutex = NULL;

static device_health_overall_status_t calculate_overall_status(void)
{
    for (size_t index = 0; index < DEVICE_HEALTH_COMPONENT_COUNT; ++index) {
        device_health_component_status_t status = state.components[index].status;

        if (status == DEVICE_HEALTH_COMPONENT_DEGRADED
            || status == DEVICE_HEALTH_COMPONENT_FAULT) {
            return DEVICE_HEALTH_OVERALL_DEGRADED;
        }
    }

    return DEVICE_HEALTH_OVERALL_HEALTHY;
}

static bool lock_state(void)
{
    return mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock_state(void)
{
    xSemaphoreGive(mutex);
}

esp_err_t device_health_init(void)
{
    if (mutex != NULL) {
        return ESP_OK;
    }

    mutex = xSemaphoreCreateMutexStatic(&mutex_buffer);

    if (mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&state, 0, sizeof(state));
    state.status = DEVICE_HEALTH_OVERALL_HEALTHY;
    state.availability = DEVICE_AVAILABILITY_OFFLINE;

    for (size_t index = 0; index < DEVICE_HEALTH_COMPONENT_COUNT; ++index) {
        state.components[index].status = DEVICE_HEALTH_COMPONENT_UNKNOWN;
        state.components[index].error = DEVICE_HEALTH_ERROR_NOT_INITIALIZED;
    }

    state.state_revision = 1;
    return ESP_OK;
}

esp_err_t device_health_update_component(
    device_health_component_t component,
    device_health_component_status_t status,
    device_health_error_t error,
    const char *updated_at
)
{
    if (component < 0 || component >= DEVICE_HEALTH_COMPONENT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lock_state()) {
        return ESP_ERR_INVALID_STATE;
    }

    device_health_component_snapshot_t *entry = &state.components[component];
    bool state_changed = entry->status != status || entry->error != error;

    entry->status = status;
    entry->error = error;
    entry->has_updated_at = updated_at != NULL;

    if (updated_at != NULL) {
        strlcpy(entry->updated_at, updated_at, sizeof(entry->updated_at));
    } else {
        entry->updated_at[0] = '\0';
    }

    device_health_overall_status_t overall = calculate_overall_status();

    if (state.status != overall) {
        state.status = overall;
        state_changed = true;
    }

    if (state_changed) {
        state.state_revision++;
    }

    unlock_state();
    return ESP_OK;
}

esp_err_t device_health_set_availability(device_availability_t availability)
{
    if (!lock_state()) {
        return ESP_ERR_INVALID_STATE;
    }

    state.availability = availability;

    unlock_state();
    return ESP_OK;
}

#define DEFINE_COUNTER_INCREMENT(function_name, field_name) \
    void function_name(void) \
    { \
        if (lock_state()) { \
            state.counters.field_name++; \
            unlock_state(); \
        } \
    }

DEFINE_COUNTER_INCREMENT(device_health_increment_samples_ok, samples_ok)
DEFINE_COUNTER_INCREMENT(device_health_increment_samples_rejected, samples_rejected)
DEFINE_COUNTER_INCREMENT(device_health_increment_publish_ok, publish_ok)
DEFINE_COUNTER_INCREMENT(device_health_increment_publish_failed, publish_failed)
DEFINE_COUNTER_INCREMENT(device_health_increment_machine_status_errors, machine_status_errors)

esp_err_t device_health_set_metrics(const device_health_metrics_t *metrics)
{
    if (metrics == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lock_state()) {
        return ESP_ERR_INVALID_STATE;
    }

    state.metrics = *metrics;
    unlock_state();
    return ESP_OK;
}

esp_err_t device_health_get_snapshot(device_health_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lock_state()) {
        return ESP_ERR_INVALID_STATE;
    }

    *snapshot = state;
    unlock_state();
    return ESP_OK;
}

const char *device_health_overall_status_to_string(device_health_overall_status_t status)
{
    return status == DEVICE_HEALTH_OVERALL_HEALTHY ? "healthy" : "degraded";
}

const char *device_health_component_status_to_string(device_health_component_status_t status)
{
    switch (status) {
        case DEVICE_HEALTH_COMPONENT_HEALTHY: return "healthy";
        case DEVICE_HEALTH_COMPONENT_DEGRADED: return "degraded";
        case DEVICE_HEALTH_COMPONENT_FAULT: return "fault";
        case DEVICE_HEALTH_COMPONENT_UNKNOWN:
        default: return "unknown";
    }
}

const char *device_health_error_to_string(device_health_error_t error)
{
    switch (error) {
        case DEVICE_HEALTH_ERROR_NONE: return "none";
        case DEVICE_HEALTH_ERROR_NOT_INITIALIZED: return "not_initialized";
        case DEVICE_HEALTH_ERROR_COMMUNICATION_FAILED: return "communication_failed";
        case DEVICE_HEALTH_ERROR_READ_FAILED: return "read_failed";
        case DEVICE_HEALTH_ERROR_VALUE_NOT_FINITE: return "value_not_finite";
        case DEVICE_HEALTH_ERROR_TEMPERATURE_OUT_OF_RANGE: return "temperature_out_of_range";
        case DEVICE_HEALTH_ERROR_HUMIDITY_OUT_OF_RANGE: return "humidity_out_of_range";
        case DEVICE_HEALTH_ERROR_TIME_NOT_SYNCHRONIZED: return "time_not_synchronized";
        case DEVICE_HEALTH_ERROR_MQTT_DISCONNECTED: return "mqtt_disconnected";
        case DEVICE_HEALTH_ERROR_PUBLISH_FAILED: return "publish_failed";
        case DEVICE_HEALTH_ERROR_MACHINE_STATUS_UNAVAILABLE: return "machine_status_unavailable";
        default: return "not_initialized";
    }
}

const char *device_availability_to_string(device_availability_t availability)
{
    return availability == DEVICE_AVAILABILITY_ONLINE ? "online" : "offline";
}

const char *device_health_component_to_string(device_health_component_t component)
{
    switch (component) {
        case DEVICE_HEALTH_COMPONENT_SENSOR: return "sensor";
        case DEVICE_HEALTH_COMPONENT_MACHINE_STATUS: return "machine_status";
        case DEVICE_HEALTH_COMPONENT_SYSTEM_TIME: return "system_time";
        case DEVICE_HEALTH_COMPONENT_MQTT: return "mqtt";
        default: return "unknown";
    }
}

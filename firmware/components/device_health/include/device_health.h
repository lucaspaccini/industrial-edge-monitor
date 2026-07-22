#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define DEVICE_HEALTH_TIMESTAMP_SIZE 21

typedef enum {
    DEVICE_HEALTH_OVERALL_HEALTHY = 0,
    DEVICE_HEALTH_OVERALL_DEGRADED,
} device_health_overall_status_t;

typedef enum {
    DEVICE_HEALTH_COMPONENT_HEALTHY = 0,
    DEVICE_HEALTH_COMPONENT_DEGRADED,
    DEVICE_HEALTH_COMPONENT_FAULT,
    DEVICE_HEALTH_COMPONENT_UNKNOWN,
} device_health_component_status_t;

typedef enum {
    DEVICE_AVAILABILITY_ONLINE = 0,
    DEVICE_AVAILABILITY_OFFLINE,
} device_availability_t;

typedef enum {
    DEVICE_HEALTH_ERROR_NONE = 0,
    DEVICE_HEALTH_ERROR_NOT_INITIALIZED,
    DEVICE_HEALTH_ERROR_COMMUNICATION_FAILED,
    DEVICE_HEALTH_ERROR_READ_FAILED,
    DEVICE_HEALTH_ERROR_VALUE_NOT_FINITE,
    DEVICE_HEALTH_ERROR_TEMPERATURE_OUT_OF_RANGE,
    DEVICE_HEALTH_ERROR_HUMIDITY_OUT_OF_RANGE,
    DEVICE_HEALTH_ERROR_TIME_NOT_SYNCHRONIZED,
    DEVICE_HEALTH_ERROR_MQTT_DISCONNECTED,
    DEVICE_HEALTH_ERROR_PUBLISH_FAILED,
    DEVICE_HEALTH_ERROR_MACHINE_STATUS_UNAVAILABLE,
} device_health_error_t;

typedef enum {
    DEVICE_HEALTH_COMPONENT_SENSOR = 0,
    DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
    DEVICE_HEALTH_COMPONENT_SYSTEM_TIME,
    DEVICE_HEALTH_COMPONENT_MQTT,
    DEVICE_HEALTH_COMPONENT_COUNT,
} device_health_component_t;

typedef struct {
    device_health_component_status_t status;
    device_health_error_t error;
    bool has_updated_at;
    char updated_at[DEVICE_HEALTH_TIMESTAMP_SIZE];
} device_health_component_snapshot_t;

typedef struct {
    uint64_t samples_ok;
    uint64_t samples_rejected;
    uint64_t publish_ok;
    uint64_t publish_failed;
    uint64_t machine_status_errors;
} device_health_counters_t;

typedef struct {
    uint64_t uptime_seconds;
    uint32_t free_heap_bytes;
    uint32_t minimum_free_heap_bytes;
    bool has_wifi_rssi;
    int32_t wifi_rssi_dbm;
} device_health_metrics_t;

typedef struct {
    device_health_overall_status_t status;
    device_availability_t availability;
    device_health_component_snapshot_t components[DEVICE_HEALTH_COMPONENT_COUNT];
    device_health_counters_t counters;
    device_health_metrics_t metrics;
    uint32_t state_revision;
} device_health_snapshot_t;

esp_err_t device_health_init(void);

esp_err_t device_health_update_component(
    device_health_component_t component,
    device_health_component_status_t status,
    device_health_error_t error,
    const char *updated_at
);

esp_err_t device_health_set_availability(device_availability_t availability);

void device_health_increment_samples_ok(void);
void device_health_increment_samples_rejected(void);
void device_health_increment_publish_ok(void);
void device_health_increment_publish_failed(void);
void device_health_increment_machine_status_errors(void);

esp_err_t device_health_set_metrics(const device_health_metrics_t *metrics);
esp_err_t device_health_get_snapshot(device_health_snapshot_t *snapshot);

const char *device_health_overall_status_to_string(device_health_overall_status_t status);
const char *device_health_component_status_to_string(device_health_component_status_t status);
const char *device_health_error_to_string(device_health_error_t error);
const char *device_availability_to_string(device_availability_t availability);
const char *device_health_component_to_string(device_health_component_t component);

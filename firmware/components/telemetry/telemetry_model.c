#include "telemetry_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "config.h"
#include "machine_status.h"
#include "sensor.h"
#include "system_time.h"


static const char *TAG = "telemetry_model";


static esp_err_t validate_measurement(const sensor_data_t *sensor_data)
{
    if (!isfinite(sensor_data->temperature) || !isfinite(sensor_data->humidity)) {
        return TELEMETRY_MODEL_ERR_VALUE_NOT_FINITE;
    }

    if (sensor_data->temperature < SENSOR_TEMPERATURE_MIN_C
        || sensor_data->temperature > SENSOR_TEMPERATURE_MAX_C) {
        return TELEMETRY_MODEL_ERR_TEMPERATURE_RANGE;
    }

    if (sensor_data->humidity < SENSOR_HUMIDITY_MIN_PERCENT
        || sensor_data->humidity > SENSOR_HUMIDITY_MAX_PERCENT) {
        return TELEMETRY_MODEL_ERR_HUMIDITY_RANGE;
    }

    return ESP_OK;
}

esp_err_t telemetry_model_create(telemetry_t *telemetry)
{
    if (telemetry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    telemetry_t candidate = {0};
    sensor_data_t sensor_data = {0};
    esp_err_t sensor_result = sensor_read(&sensor_data);

    if (sensor_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read sensor data: %s",
            esp_err_to_name(sensor_result)
        );
        return TELEMETRY_MODEL_ERR_SENSOR_READ;
    }

    esp_err_t validation_result = validate_measurement(&sensor_data);

    if (validation_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Invalid measurement: temperature=%.2f C, humidity=%.2f %%",
            sensor_data.temperature,
            sensor_data.humidity
        );
        return validation_result;
    }

    esp_err_t time_result = system_time_get_timestamp(
        candidate.timestamp,
        sizeof(candidate.timestamp)
    );

    if (time_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to generate timestamp: %s",
            esp_err_to_name(time_result)
        );
        return TELEMETRY_MODEL_ERR_TIMESTAMP;
    }

    machine_status_t status = MACHINE_STATUS_UNKNOWN;
    candidate.machine_status_result = machine_status_get(&status);

    const char *status_text = machine_status_to_string(status);

    if (status_text == NULL) {
        ESP_LOGE(TAG, "Machine status has no string representation");
        status_text = "unknown";
    }

    int status_length = snprintf(
        candidate.machine_status,
        sizeof(candidate.machine_status),
        "%s",
        status_text
    );

    if (status_length <= 0
        || (size_t)status_length >= sizeof(candidate.machine_status)) {
        ESP_LOGE(TAG, "Failed to format machine status");
        return ESP_ERR_INVALID_SIZE;
    }

    int device_id_length = snprintf(
        candidate.device_id,
        sizeof(candidate.device_id),
        "%s",
        CONFIG_DEVICE_ID
    );

    if (device_id_length <= 0
        || (size_t)device_id_length >= sizeof(candidate.device_id)) {
        return ESP_ERR_INVALID_SIZE;
    }

    candidate.temperature = sensor_data.temperature;
    candidate.humidity = sensor_data.humidity;
    *telemetry = candidate;

    return ESP_OK;
}

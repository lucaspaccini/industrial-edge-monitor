#include "telemetry_model.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "machine_status.h"
#include "sensor.h"
#include "system_time.h"


static const char *TAG = "telemetry_model";


void telemetry_model_create(telemetry_t *telemetry)
{
    if (telemetry == NULL) {
        return;
    }

    memset(telemetry, 0, sizeof(*telemetry));

    sensor_data_t sensor_data = {0};

    esp_err_t sensor_result = sensor_read(&sensor_data);

    if (sensor_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read sensor data: %s",
            esp_err_to_name(sensor_result)
        );
    } else {
        telemetry->temperature = sensor_data.temperature;
        telemetry->humidity = sensor_data.humidity;
    }

    esp_err_t time_result = system_time_get_timestamp(
        telemetry->timestamp,
        sizeof(telemetry->timestamp)
    );

    if (time_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to generate timestamp: %s",
            esp_err_to_name(time_result)
        );

        strncpy(
            telemetry->timestamp,
            "unknown",
            sizeof(telemetry->timestamp) - 1
        );
    }

    machine_status_t status = machine_status_get();

    strncpy(
        telemetry->machine_status,
        machine_status_to_string(status),
        sizeof(telemetry->machine_status) - 1
    );
}
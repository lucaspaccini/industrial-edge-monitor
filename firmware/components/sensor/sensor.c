#include "sensor.h"

#include <stddef.h>

#include "esp_log.h"

static const char *TAG = "sensor";

esp_err_t sensor_init(void)
{
    ESP_LOGI(TAG, "Sensor component initialized with simulated data");
    return ESP_OK;
}

esp_err_t sensor_read(sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data->temperature = 25.0f;
    data->humidity = 50.0f;

    return ESP_OK;
}
#include "sensor.h"

#include <stddef.h>
#include <stdbool.h>

#include "bme280.h"
#include "esp_log.h"

static const char *TAG = "sensor";
static bool initialized = false;

esp_err_t sensor_invalidate(void)
{
    initialized = false;
    return bme280_deinit();
}

esp_err_t sensor_init(void)
{
    esp_err_t result = bme280_init();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize BME280: %s",
            esp_err_to_name(result)
        );

        esp_err_t deinit_result = sensor_invalidate();
        if (deinit_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "BME280 cleanup deferred until next cycle: %s",
                esp_err_to_name(deinit_result)
            );
        }

        return result;
    }

    ESP_LOGI(TAG, "Sensor component initialized with BME280 provider");
    initialized = true;

    return ESP_OK;
}

esp_err_t sensor_read(sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!initialized) {
        esp_err_t init_result = sensor_init();

        if (init_result != ESP_OK) {
            return init_result;
        }
    }

    bme280_measurement_t measurement;

    esp_err_t result = bme280_read(&measurement);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read BME280 measurement: %s",
            esp_err_to_name(result)
        );

        esp_err_t deinit_result = sensor_invalidate();
        if (deinit_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "BME280 cleanup deferred until next cycle: %s",
                esp_err_to_name(deinit_result)
            );
        }

        return result;
    }

    data->temperature = measurement.temperature_c;
    data->humidity = measurement.humidity_percent;
    data->pressure = measurement.pressure_hpa;

    return ESP_OK;
}

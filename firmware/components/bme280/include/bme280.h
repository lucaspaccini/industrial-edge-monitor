#pragma once

#include "esp_err.h"

typedef struct {
    float temperature_c;
    float pressure_hpa;
    float humidity_percent;
} bme280_measurement_t;

/**
 * @brief Initialize the BME280 device.
 *
 * The I2C bus must already be initialized through app_i2c_bus_init().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if the I2C bus is not initialized
 *      - ESP_ERR_INVALID_RESPONSE if the detected device is not a BME280
 *      - Other ESP-IDF I2C errors
 */
esp_err_t bme280_init(void);

/**
 * @brief Read a compensated measurement from the BME280.
 *
 * @param[out] measurement Destination measurement structure.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if measurement is NULL
 *      - ESP_ERR_INVALID_STATE if the device is not initialized
 *      - Other ESP-IDF I2C errors
 */
esp_err_t bme280_read(bme280_measurement_t *measurement);
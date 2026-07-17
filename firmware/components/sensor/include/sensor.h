#pragma once

#include "esp_err.h"

typedef struct
{
    float temperature;  /**< Celsius */
    float humidity;     /**< Percent */
    float pressure;     /**< hPa */
} sensor_data_t;

/**
 * @brief Initialize the physical sensor provider.
 *
 * @return
 *      - ESP_OK on success
 *      - Error returned by the underlying sensor driver
 */
esp_err_t sensor_init(void);

/**
 * @brief Read the current environmental measurements.
 *
 * @param[out] data Destination structure.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if data is NULL
 *      - Error returned by the underlying sensor driver
 */
esp_err_t sensor_read(sensor_data_t *data);
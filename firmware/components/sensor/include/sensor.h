#pragma once

#include "esp_err.h"

#define SENSOR_TEMPERATURE_MIN_C    (-40.0f)
#define SENSOR_TEMPERATURE_MAX_C    85.0f
#define SENSOR_HUMIDITY_MIN_PERCENT 0.0f
#define SENSOR_HUMIDITY_MAX_PERCENT 100.0f

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
 * @brief Invalidate the current provider after a communication or data fault.
 *
 * The following sensor_read() performs at most one complete initialization
 * attempt before reading again.
 */
esp_err_t sensor_invalidate(void);

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

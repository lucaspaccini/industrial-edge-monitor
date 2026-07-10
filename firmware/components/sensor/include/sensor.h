#pragma once

#include "esp_err.h"

typedef struct
{
    float temperature;
    float humidity;
} sensor_data_t;

esp_err_t sensor_init(void);
esp_err_t sensor_read(sensor_data_t *data);
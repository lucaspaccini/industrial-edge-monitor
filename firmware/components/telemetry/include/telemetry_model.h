#pragma once

#include "esp_err.h"
#include "system_time.h"

#define TELEMETRY_MODEL_ERR_BASE                0x7100
#define TELEMETRY_MODEL_ERR_SENSOR_READ         (TELEMETRY_MODEL_ERR_BASE + 1)
#define TELEMETRY_MODEL_ERR_VALUE_NOT_FINITE    (TELEMETRY_MODEL_ERR_BASE + 2)
#define TELEMETRY_MODEL_ERR_TEMPERATURE_RANGE   (TELEMETRY_MODEL_ERR_BASE + 3)
#define TELEMETRY_MODEL_ERR_HUMIDITY_RANGE      (TELEMETRY_MODEL_ERR_BASE + 4)
#define TELEMETRY_MODEL_ERR_TIMESTAMP           (TELEMETRY_MODEL_ERR_BASE + 5)

#define TELEMETRY_DEVICE_ID_SIZE 64

typedef struct
{
    char device_id[TELEMETRY_DEVICE_ID_SIZE];
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    float temperature;
    float humidity;
    char machine_status[16];
    esp_err_t machine_status_result;
} telemetry_t;

/**
 * @brief Aggregate and validate a complete telemetry sample.
 *
 * The output is written only when every provider succeeds and every field is
 * valid. Callers must discard the sample for any result other than ESP_OK.
 */
esp_err_t telemetry_model_create(telemetry_t *telemetry);

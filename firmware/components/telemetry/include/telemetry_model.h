#pragma once

#include "system_time.h"

typedef struct
{
    char timestamp[SYSTEM_TIME_TIMESTAMP_SIZE];
    float temperature;
    float humidity;
    char machine_status[16];
} telemetry_t;

void telemetry_model_create(telemetry_t *telemetry);

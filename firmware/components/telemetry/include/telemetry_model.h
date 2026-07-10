#pragma once

typedef struct
{
    char timestamp[32];
    float temperature;
    float humidity;
    char machine_status[16];
} telemetry_t;

void telemetry_model_create(telemetry_t *telemetry);
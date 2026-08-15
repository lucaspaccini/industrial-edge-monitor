#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define MQTT_ERROR_TEXT_CAPACITY 96
#define MQTT_ERROR_STATE_INITIALIZER {.mutex = NULL, .text = {0}}

typedef struct {
    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;
    char text[MQTT_ERROR_TEXT_CAPACITY];
} mqtt_error_state_t;

bool mqtt_error_state_init(mqtt_error_state_t *state);
void mqtt_error_state_set(mqtt_error_state_t *state, const char *message);
bool mqtt_error_state_copy(mqtt_error_state_t *state, char *buffer, size_t buffer_size);

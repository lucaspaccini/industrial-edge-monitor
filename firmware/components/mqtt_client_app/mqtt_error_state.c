#include "mqtt_error_state.h"

#include <stdio.h>

static bool lock(mqtt_error_state_t *state)
{
    return state->mutex != NULL && xSemaphoreTake(state->mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock(mqtt_error_state_t *state)
{
    xSemaphoreGive(state->mutex);
}

bool mqtt_error_state_init(mqtt_error_state_t *state)
{
    if (state == NULL) return false;
    state->text[0] = '\0';
    state->mutex = xSemaphoreCreateMutexStatic(&state->mutex_storage);
    return state->mutex != NULL;
}

void mqtt_error_state_set(mqtt_error_state_t *state, const char *message)
{
    if (state == NULL) return;
    if (!lock(state)) return;
    snprintf(state->text, sizeof(state->text), "%s", message != NULL ? message : "");
    unlock(state);
}

bool mqtt_error_state_copy(mqtt_error_state_t *state, char *buffer, size_t buffer_size)
{
    if (state == NULL || buffer == NULL || buffer_size == 0) return false;
    if (!lock(state)) return false;
    int written = snprintf(buffer, buffer_size, "%s", state->text);
    unlock(state);
    return written >= 0 && (size_t)written < buffer_size;
}

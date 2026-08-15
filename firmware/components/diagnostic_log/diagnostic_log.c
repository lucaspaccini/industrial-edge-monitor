#include "diagnostic_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "diagnostic_ring.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t original_vprintf;
static bool diagnostic_sink_installed;

static void parse_prefix(diagnostic_log_record_t *record)
{
    record->level = '?';
    snprintf(record->component, sizeof(record->component), "esp-idf");
    const char *cursor = record->message;
    const char *level = strstr(cursor, ") ");
    if (level != NULL && level[2] != '\0') {
        const char *tag_start = level + 2;
        const char *colon = strchr(tag_start, ':');
        if (colon != NULL && colon > tag_start) {
            const char *candidate = level;
            while (candidate > cursor && candidate[-1] != ' ') {
                candidate--;
            }
            if (*candidate == 'E' || *candidate == 'W' || *candidate == 'I'
                || *candidate == 'D' || *candidate == 'V') {
                record->level = *candidate;
            }
            size_t length = (size_t)(colon - tag_start);
            if (length > DIAGNOSTIC_LOG_COMPONENT_MAX) {
                length = DIAGNOSTIC_LOG_COMPONENT_MAX;
            }
            memcpy(record->component, tag_start, length);
            record->component[length] = '\0';
        }
    }
}

static int diagnostic_vprintf(const char *format, va_list arguments)
{
    va_list serial_arguments;
    va_copy(serial_arguments, arguments);
    int result = original_vprintf != NULL
        ? original_vprintf(format, serial_arguments)
        : vprintf(format, serial_arguments);
    va_end(serial_arguments);

    diagnostic_log_record_t record = {0};
    va_list buffer_arguments;
    va_copy(buffer_arguments, arguments);
    vsnprintf(record.message, sizeof(record.message), format, buffer_arguments);
    va_end(buffer_arguments);
    record.relative_ms = (uint64_t)(esp_timer_get_time() / 1000);
    parse_prefix(&record);

    taskENTER_CRITICAL(&lock);
    diagnostic_ring_push(&record);
    taskEXIT_CRITICAL(&lock);
    return result;
}

esp_err_t diagnostic_log_init(void)
{
    taskENTER_CRITICAL(&lock);
    if (diagnostic_sink_installed) {
        taskEXIT_CRITICAL(&lock);
        return ESP_OK;
    }
    diagnostic_ring_reset();
    diagnostic_sink_installed = true;
    taskEXIT_CRITICAL(&lock);
    original_vprintf = esp_log_set_vprintf(diagnostic_vprintf);
    return ESP_OK;
}

void diagnostic_log_get_batch(diagnostic_log_batch_t *batch, uint64_t after_sequence)
{
    if (batch == NULL) {
        return;
    }
    taskENTER_CRITICAL(&lock);
    diagnostic_ring_batch(batch, after_sequence);
    taskEXIT_CRITICAL(&lock);
}

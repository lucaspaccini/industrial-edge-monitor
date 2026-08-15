#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define DIAGNOSTIC_LOG_CAPACITY 64
#define DIAGNOSTIC_LOG_BATCH_CAPACITY 8
#define DIAGNOSTIC_LOG_MESSAGE_MAX 159
#define DIAGNOSTIC_LOG_COMPONENT_MAX 23

typedef struct {
    uint64_t sequence;
    uint64_t relative_ms;
    char level;
    char component[DIAGNOSTIC_LOG_COMPONENT_MAX + 1];
    char message[DIAGNOSTIC_LOG_MESSAGE_MAX + 1];
} diagnostic_log_record_t;

typedef struct {
    diagnostic_log_record_t records[DIAGNOSTIC_LOG_BATCH_CAPACITY];
    size_t count;
    uint64_t cursor;
    uint64_t first_available_sequence;
    uint64_t next_sequence;
    uint64_t overwritten;
    uint64_t lost_before_cursor;
    bool has_more;
} diagnostic_log_batch_t;

esp_err_t diagnostic_log_init(void);
void diagnostic_log_get_batch(diagnostic_log_batch_t *batch, uint64_t after_sequence);

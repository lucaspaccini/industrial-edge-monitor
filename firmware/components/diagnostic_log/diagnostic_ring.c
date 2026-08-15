#include "diagnostic_ring.h"

#include <string.h>

static diagnostic_log_record_t records[DIAGNOSTIC_LOG_CAPACITY];
static size_t head;
static size_t count;
static uint64_t next_sequence = 1;
static uint64_t overwritten;

/* Keeps HTTP/SSE copy frames bounded well below the 8 KiB server task stack. */
_Static_assert(sizeof(diagnostic_log_batch_t) <= 2048, "diagnostic batch must remain <= 2 KiB");

void diagnostic_ring_reset(void)
{
    memset(records, 0, sizeof(records));
    head = 0;
    count = 0;
    next_sequence = 1;
    overwritten = 0;
}

void diagnostic_ring_push(diagnostic_log_record_t *record)
{
    if (record == NULL) {
        return;
    }
    record->sequence = next_sequence++;
    records[head] = *record;
    head = (head + 1) % DIAGNOSTIC_LOG_CAPACITY;
    if (count < DIAGNOSTIC_LOG_CAPACITY) {
        count++;
    } else {
        overwritten++;
    }
}

void diagnostic_ring_batch(diagnostic_log_batch_t *batch, uint64_t after_sequence)
{
    if (batch == NULL) {
        return;
    }
    memset(batch, 0, sizeof(*batch));
    batch->cursor = after_sequence;
    batch->next_sequence = next_sequence;
    batch->overwritten = overwritten;
    size_t start = (head + DIAGNOSTIC_LOG_CAPACITY - count) % DIAGNOSTIC_LOG_CAPACITY;
    if (count > 0) {
        batch->first_available_sequence = records[start].sequence;
        if (after_sequence != UINT64_MAX && after_sequence + 1 < batch->first_available_sequence) {
            batch->lost_before_cursor = batch->first_available_sequence - after_sequence - 1;
        }
    } else {
        batch->first_available_sequence = next_sequence;
    }
    for (size_t index = 0; index < count; index++) {
        const diagnostic_log_record_t *record = &records[(start + index) % DIAGNOSTIC_LOG_CAPACITY];
        if (record->sequence > after_sequence) {
            if (batch->count == DIAGNOSTIC_LOG_BATCH_CAPACITY) {
                batch->has_more = true;
                break;
            }
            batch->records[batch->count++] = *record;
            batch->cursor = record->sequence;
        }
    }
}

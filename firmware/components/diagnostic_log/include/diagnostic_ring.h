#pragma once

#include <stdint.h>

#include "diagnostic_log.h"

void diagnostic_ring_reset(void);
void diagnostic_ring_push(diagnostic_log_record_t *record);
void diagnostic_ring_batch(diagnostic_log_batch_t *batch, uint64_t after_sequence);

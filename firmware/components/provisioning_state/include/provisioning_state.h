#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef enum {
    PROVISIONING_BOOT = 0,
    PROVISIONING_LOAD_CONFIGURATION,
    PROVISIONING_UNPROVISIONED,
    PROVISIONING_MAINTENANCE_WINDOW,
    PROVISIONING_OPERATIONAL,
    PROVISIONING_ROLLBACK,
} provisioning_state_t;

typedef struct {
    StaticSemaphore_t mutex_storage;
    SemaphoreHandle_t mutex;
    provisioning_state_t state;
    bool configured;
    bool maintenance_enabled;
    bool authenticated_activity;
    uint64_t opened_ms;
    uint64_t last_authenticated_ms;
    uint32_t inactivity_seconds;
    uint32_t absolute_seconds;
} provisioning_state_machine_t;

typedef struct {
    provisioning_state_t state;
    bool configured;
    bool maintenance_enabled;
    bool authenticated_activity;
    uint64_t opened_ms;
    uint64_t last_authenticated_ms;
    uint32_t inactivity_seconds;
    uint32_t absolute_seconds;
} provisioning_state_snapshot_t;

void provisioning_state_init(provisioning_state_machine_t *machine);
void provisioning_state_configuration_loaded(
    provisioning_state_machine_t *machine,
    bool valid,
    bool maintenance_enabled,
    uint64_t now_ms,
    uint32_t inactivity_seconds,
    uint32_t absolute_seconds
);
void provisioning_state_authenticated_activity(provisioning_state_machine_t *machine, uint64_t now_ms);
bool provisioning_state_tick(provisioning_state_machine_t *machine, uint64_t now_ms);
bool provisioning_state_get_snapshot(
    provisioning_state_machine_t *machine,
    provisioning_state_snapshot_t *snapshot
);
const char *provisioning_state_name(provisioning_state_t state);

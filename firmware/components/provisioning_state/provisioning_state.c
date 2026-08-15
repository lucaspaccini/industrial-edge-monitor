#include "provisioning_state.h"

#include <stddef.h>
#include <string.h>

static bool state_lock(provisioning_state_machine_t *machine)
{
    return machine->mutex != NULL && xSemaphoreTake(machine->mutex, portMAX_DELAY) == pdTRUE;
}

static void state_unlock(provisioning_state_machine_t *machine)
{
    xSemaphoreGive(machine->mutex);
}

void provisioning_state_init(provisioning_state_machine_t *machine)
{
    if (machine == NULL) {
        return;
    }
    memset(machine, 0, sizeof(*machine));
    machine->mutex = xSemaphoreCreateMutexStatic(&machine->mutex_storage);
    machine->state = PROVISIONING_LOAD_CONFIGURATION;
}

void provisioning_state_configuration_loaded(
    provisioning_state_machine_t *machine,
    bool valid,
    bool maintenance_enabled,
    uint64_t now_ms,
    uint32_t inactivity_seconds,
    uint32_t absolute_seconds
)
{
    if (machine == NULL) {
        return;
    }
    if (!state_lock(machine)) {
        return;
    }
    machine->configured = valid;
    machine->maintenance_enabled = maintenance_enabled;
    machine->opened_ms = now_ms;
    machine->last_authenticated_ms = now_ms;
    machine->inactivity_seconds = inactivity_seconds;
    machine->absolute_seconds = absolute_seconds;
    if (!valid) {
        machine->state = PROVISIONING_UNPROVISIONED;
    } else if (maintenance_enabled) {
        machine->state = PROVISIONING_MAINTENANCE_WINDOW;
    } else {
        machine->state = PROVISIONING_OPERATIONAL;
    }
    state_unlock(machine);
}

void provisioning_state_authenticated_activity(provisioning_state_machine_t *machine, uint64_t now_ms)
{
    if (machine == NULL) {
        return;
    }
    if (!state_lock(machine)) {
        return;
    }
    if (machine->state == PROVISIONING_MAINTENANCE_WINDOW) {
        machine->authenticated_activity = true;
        machine->last_authenticated_ms = now_ms;
    }
    state_unlock(machine);
}

bool provisioning_state_tick(provisioning_state_machine_t *machine, uint64_t now_ms)
{
    if (machine == NULL) {
        return false;
    }
    if (!state_lock(machine)) {
        return false;
    }
    if (machine->state != PROVISIONING_MAINTENANCE_WINDOW) {
        state_unlock(machine);
        return false;
    }
    uint64_t inactive_ms = now_ms >= machine->last_authenticated_ms
        ? now_ms - machine->last_authenticated_ms : 0;
    uint64_t absolute_ms = now_ms >= machine->opened_ms ? now_ms - machine->opened_ms : 0;
    if (inactive_ms >= (uint64_t)machine->inactivity_seconds * 1000U
        || absolute_ms >= (uint64_t)machine->absolute_seconds * 1000U) {
        machine->state = PROVISIONING_OPERATIONAL;
        state_unlock(machine);
        return false;
    }
    state_unlock(machine);
    return true;
}

bool provisioning_state_get_snapshot(
    provisioning_state_machine_t *machine,
    provisioning_state_snapshot_t *snapshot
)
{
    if (machine == NULL || snapshot == NULL || !state_lock(machine)) return false;
    snapshot->state = machine->state;
    snapshot->configured = machine->configured;
    snapshot->maintenance_enabled = machine->maintenance_enabled;
    snapshot->authenticated_activity = machine->authenticated_activity;
    snapshot->opened_ms = machine->opened_ms;
    snapshot->last_authenticated_ms = machine->last_authenticated_ms;
    snapshot->inactivity_seconds = machine->inactivity_seconds;
    snapshot->absolute_seconds = machine->absolute_seconds;
    state_unlock(machine);
    return true;
}

const char *provisioning_state_name(provisioning_state_t state)
{
    switch (state) {
        case PROVISIONING_BOOT: return "boot";
        case PROVISIONING_LOAD_CONFIGURATION: return "load_configuration";
        case PROVISIONING_UNPROVISIONED: return "unprovisioned";
        case PROVISIONING_MAINTENANCE_WINDOW: return "maintenance_window";
        case PROVISIONING_OPERATIONAL: return "operational";
        case PROVISIONING_ROLLBACK: return "rollback";
        default: return "unknown";
    }
}

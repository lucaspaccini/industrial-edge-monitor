#pragma once

typedef enum
{
    MACHINE_STATUS_STOPPED = 0,
    MACHINE_STATUS_RUNNING,
    MACHINE_STATUS_FAULT
} machine_status_t;

machine_status_t machine_status_get(void);

const char *machine_status_to_string(machine_status_t status);
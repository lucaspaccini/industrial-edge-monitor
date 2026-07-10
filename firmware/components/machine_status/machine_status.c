#include "machine_status.h"


machine_status_t machine_status_get(void)
{
    /*
     * Simulated machine state.
     *
     * A future implementation may read this state from GPIO,
     * Modbus, Profinet or another industrial interface.
     */
    return MACHINE_STATUS_RUNNING;
}


const char *machine_status_to_string(machine_status_t status)
{
    switch (status) {
        case MACHINE_STATUS_STOPPED:
            return "STOPPED";

        case MACHINE_STATUS_RUNNING:
            return "RUNNING";

        case MACHINE_STATUS_FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}
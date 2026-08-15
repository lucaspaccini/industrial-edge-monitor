#pragma once

#include "device_config.h"
#include "esp_err.h"
#include "provisioning_state.h"

esp_err_t provisioning_service_start(
    const device_config_t *runtime_configuration,
    const char *setup_secret,
    provisioning_state_machine_t *state_machine
);
esp_err_t provisioning_service_stop(void);
bool provisioning_service_is_running(void);

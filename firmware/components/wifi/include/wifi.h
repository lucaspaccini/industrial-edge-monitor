#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "device_config.h"
#include "esp_err.h"

esp_err_t wifi_init_runtime(
    const device_config_t *configuration,
    bool enable_softap,
    const char *setup_secret
);
esp_err_t wifi_wait_for_station(uint32_t timeout_ms);
esp_err_t wifi_disable_softap(void);
bool wifi_station_is_connected(void);
bool wifi_softap_is_enabled(void);
esp_err_t wifi_get_station_ip(char *buffer, size_t buffer_size);
esp_err_t wifi_get_station_rssi(int *rssi);
esp_err_t wifi_get_softap_ipv4(uint32_t *address, uint32_t *netmask);
const char *wifi_get_softap_ssid(void);

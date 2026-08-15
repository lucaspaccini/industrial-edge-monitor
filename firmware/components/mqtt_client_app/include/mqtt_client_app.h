#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "device_config.h"

/**
 * @brief Initialize the authenticated MQTT-over-TLS transport.
 *
 * Initialization fails without an mqtts URI, provisioned broker CA, username or
 * password. No plaintext fallback is attempted.
 */
esp_err_t mqtt_init(const device_config_t *configuration);

/**
 * @brief Submit a telemetry payload to the MQTT client.
 *
 * ESP_OK means that ESP-MQTT accepted the publication request. It does not
 * represent broker or application-level acknowledgement.
 *
 * @return ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE or ESP_FAIL.
 */
esp_err_t mqtt_publish_telemetry(const char *payload);
esp_err_t mqtt_publish_health(const char *payload);
esp_err_t mqtt_publish_availability(const char *payload);

bool mqtt_is_connected(void);
uint32_t mqtt_connection_generation(void);
esp_err_t mqtt_wait_connected(uint32_t timeout_ms);
esp_err_t mqtt_copy_last_error(char *buffer, size_t buffer_size);

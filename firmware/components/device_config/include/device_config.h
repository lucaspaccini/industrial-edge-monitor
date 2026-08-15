#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define DEVICE_CONFIG_SCHEMA_VERSION 1U
#define DEVICE_CONFIG_DEVICE_ID_MAX 63
#define DEVICE_CONFIG_WIFI_SSID_MAX 32
#define DEVICE_CONFIG_WIFI_PASSWORD_MAX 63
#define DEVICE_CONFIG_MQTT_URI_MAX 255
#define DEVICE_CONFIG_MQTT_CA_MAX 4095
#define DEVICE_CONFIG_MQTT_USERNAME_MAX 63
#define DEVICE_CONFIG_MQTT_PASSWORD_MAX 127
#define DEVICE_CONFIG_MQTT_CLIENT_ID_MAX 127
#define DEVICE_CONFIG_ROLLBACK_REASON_MAX 95
#define DEVICE_CONFIG_SETUP_SECRET_LENGTH 32

typedef enum {
    DEVICE_CONFIG_MACHINE_DISABLED = 0,
    DEVICE_CONFIG_MACHINE_GPIO = 1,
} device_config_machine_provider_t;

typedef enum {
    DEVICE_CONFIG_PULL_NONE = 0,
    DEVICE_CONFIG_PULL_UP = 1,
    DEVICE_CONFIG_PULL_DOWN = 2,
} device_config_pull_t;

typedef enum {
    DEVICE_CONFIG_STATE_NONE = 0,
    DEVICE_CONFIG_STATE_ACTIVE = 1,
    DEVICE_CONFIG_STATE_PENDING = 2,
    DEVICE_CONFIG_STATE_ROLLBACK = 3,
} device_config_state_t;

typedef struct {
    uint32_t schema_version;
    uint32_t revision;
    char device_id[DEVICE_CONFIG_DEVICE_ID_MAX + 1];
    char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX + 1];
    char wifi_password[DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1];
    char mqtt_broker_uri[DEVICE_CONFIG_MQTT_URI_MAX + 1];
    char mqtt_ca_certificate[DEVICE_CONFIG_MQTT_CA_MAX + 1];
    char mqtt_username[DEVICE_CONFIG_MQTT_USERNAME_MAX + 1];
    char mqtt_password[DEVICE_CONFIG_MQTT_PASSWORD_MAX + 1];
    char mqtt_client_id[DEVICE_CONFIG_MQTT_CLIENT_ID_MAX + 1];
    uint32_t telemetry_interval_seconds;
    device_config_machine_provider_t machine_status_provider;
    int32_t machine_status_gpio;
    bool machine_status_active_high;
    device_config_pull_t machine_status_pull;
    bool maintenance_on_boot;
    uint32_t maintenance_window_seconds;
    uint32_t maintenance_max_session_seconds;
} device_config_t;

typedef struct {
    uint32_t schema_version;
    uint32_t revision;
    char device_id[DEVICE_CONFIG_DEVICE_ID_MAX + 1];
    char wifi_ssid[DEVICE_CONFIG_WIFI_SSID_MAX + 1];
    bool wifi_password_configured;
    char mqtt_broker_uri[DEVICE_CONFIG_MQTT_URI_MAX + 1];
    char mqtt_username[DEVICE_CONFIG_MQTT_USERNAME_MAX + 1];
    bool mqtt_password_configured;
    char mqtt_client_id[DEVICE_CONFIG_MQTT_CLIENT_ID_MAX + 1];
    bool mqtt_ca_certificate_configured;
    uint32_t telemetry_interval_seconds;
    device_config_machine_provider_t machine_status_provider;
    int32_t machine_status_gpio;
    bool machine_status_active_high;
    device_config_pull_t machine_status_pull;
    bool maintenance_on_boot;
    uint32_t maintenance_window_seconds;
    uint32_t maintenance_max_session_seconds;
} device_config_redacted_t;

typedef struct {
    device_config_state_t state;
    uint32_t candidate_revision;
    uint32_t boot_attempts;
    uint32_t boot_count;
    char last_rollback_reason[DEVICE_CONFIG_ROLLBACK_REASON_MAX + 1];
} device_config_metadata_t;

esp_err_t device_config_storage_init(void);
esp_err_t device_config_storage_recover(void);
esp_err_t device_config_load_active(device_config_t *configuration);
esp_err_t device_config_load_for_boot(
    device_config_t *configuration,
    bool *using_candidate,
    device_config_metadata_t *metadata
);
esp_err_t device_config_validate(const device_config_t *configuration, char *error, size_t error_size);
esp_err_t device_config_stage_candidate(const device_config_t *configuration);
esp_err_t device_config_activate_candidate(const device_config_t *verified_candidate);
esp_err_t device_config_rollback_candidate(const char *reason);
esp_err_t device_config_cancel_candidate(void);
bool device_config_candidate_validation_in_progress(void);
esp_err_t device_config_factory_reset(void);
esp_err_t device_config_get_redacted_snapshot(
    const device_config_t *configuration,
    device_config_redacted_t *snapshot
);
esp_err_t device_config_get_metadata(device_config_metadata_t *metadata);
esp_err_t device_config_get_or_create_setup_secret(
    char secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1],
    bool *generated
);

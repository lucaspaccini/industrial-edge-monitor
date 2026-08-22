#include "device_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bootloader_random.h"
#include "device_config_storage.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mbedtls/x509_crt.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "device_config";
static const char *NAMESPACE = "iem_config";
static const char *KEY_ACTIVE = "active";
static const char *KEY_CANDIDATE = "candidate";
static const char *KEY_METADATA = "metadata";
static const char *KEY_SETUP = "setup_secret";
static StaticSemaphore_t configuration_mutex_storage;
static SemaphoreHandle_t configuration_mutex;
static bool candidate_validation_in_progress;
static uint32_t candidate_validation_revision;

static esp_err_t configuration_lock(void)
{
    if (configuration_mutex == NULL) return ESP_ERR_INVALID_STATE;
    return xSemaphoreTake(configuration_mutex, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

static void configuration_unlock(void)
{
    xSemaphoreGive(configuration_mutex);
}

static bool valid_device_id(const char *value)
{
    size_t length = strnlen(value, DEVICE_CONFIG_DEVICE_ID_MAX + 1);
    if (length == 0 || length > DEVICE_CONFIG_DEVICE_ID_MAX) {
        return false;
    }
    unsigned char first = (unsigned char)value[0];
    bool first_is_ascii_alphanumeric = (first >= 'A' && first <= 'Z')
        || (first >= 'a' && first <= 'z') || (first >= '0' && first <= '9');
    if (!first_is_ascii_alphanumeric) {
        return false;
    }
    for (size_t index = 0; index < length; index++) {
        unsigned char character = (unsigned char)value[index];
        bool ascii_alphanumeric = (character >= 'A' && character <= 'Z')
            || (character >= 'a' && character <= 'z')
            || (character >= '0' && character <= '9');
        if (!ascii_alphanumeric && character != '.' && character != '_'
            && character != '-') {
            return false;
        }
    }
    const char *reserved[] = {
        "collector", "healthcheck", "simulator", "legacy-test", "legacy-device"
    };
    for (size_t index = 0; index < sizeof(reserved) / sizeof(reserved[0]); index++) {
        if (strcmp(value, reserved[index]) == 0) {
            return false;
        }
    }
    return true;
}

static esp_err_t validation_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
    return ESP_ERR_INVALID_ARG;
}

static bool terminated(const char *value, size_t capacity)
{
    return memchr(value, '\0', capacity) != NULL;
}

static bool gpio_allowed(int32_t gpio)
{
    if (gpio < 0 || gpio > 39) {
        return false;
    }
    /* Flash pins, input-only pins with pull requests, boot straps and project I2C pins. */
    const int reserved[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 15, 21, 22};
    for (size_t index = 0; index < sizeof(reserved) / sizeof(reserved[0]); index++) {
        if (gpio == reserved[index]) {
            return false;
        }
    }
    return true;
}

static bool valid_mqtts_uri(const char *value)
{
    if (strncmp(value, "mqtts://", 8) != 0 || value[8] == '\0'
        || strchr(value + 8, '@') != NULL || strpbrk(value, " \t\r\n") != NULL) {
        return false;
    }
    const char *authority = value + 8;
    const char *port_separator;
    if (*authority == '[') {
        const char *closing = strchr(authority, ']');
        if (closing == NULL || closing == authority + 1 || closing[1] != ':') {
            return false;
        }
        port_separator = closing + 1;
    } else {
        port_separator = strrchr(authority, ':');
        if (port_separator == NULL || port_separator == authority) {
            return false;
        }
    }
    const char *port_text = port_separator + 1;
    if (*port_text == '\0') {
        return false;
    }
    uint32_t port = 0;
    for (const char *cursor = port_text; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        if (port > 6553U) {
            return false;
        }
        port = port * 10U + (uint32_t)(*cursor - '0');
        if (port > 65535U) {
            return false;
        }
    }
    return port > 0;
}

esp_err_t device_config_validate(const device_config_t *configuration, char *error, size_t error_size)
{
    if (configuration == NULL) {
        return validation_error(error, error_size, "configuration is required");
    }
    if (configuration->schema_version != DEVICE_CONFIG_SCHEMA_VERSION) {
        return validation_error(error, error_size, "unsupported schema_version");
    }
    if (configuration->revision == 0) {
        return validation_error(error, error_size, "revision must be greater than zero");
    }
    if (!terminated(configuration->device_id, sizeof(configuration->device_id))
        || !valid_device_id(configuration->device_id)) {
        return validation_error(error, error_size, "device_id is invalid or reserved");
    }
    if (!terminated(configuration->wifi_ssid, sizeof(configuration->wifi_ssid))
        || configuration->wifi_ssid[0] == '\0') {
        return validation_error(error, error_size, "wifi_ssid is required");
    }
    size_t wifi_password_length = strnlen(configuration->wifi_password, sizeof(configuration->wifi_password));
    if (wifi_password_length < 8 || wifi_password_length > DEVICE_CONFIG_WIFI_PASSWORD_MAX) {
        return validation_error(error, error_size, "wifi_password must be 8-63 characters");
    }
    if (!terminated(configuration->mqtt_broker_uri, sizeof(configuration->mqtt_broker_uri))
        || !valid_mqtts_uri(configuration->mqtt_broker_uri)) {
        return validation_error(error, error_size, "mqtt_broker_uri must be mqtts://host:port without credentials or path");
    }
    if (!terminated(configuration->mqtt_username, sizeof(configuration->mqtt_username))
        || strcmp(configuration->mqtt_username, configuration->device_id) != 0) {
        return validation_error(error, error_size, "mqtt_username must equal device_id for broker ACL isolation");
    }
    if (!terminated(configuration->mqtt_password, sizeof(configuration->mqtt_password))
        || configuration->mqtt_password[0] == '\0') {
        return validation_error(error, error_size, "mqtt_password is required");
    }
    if (!terminated(configuration->mqtt_client_id, sizeof(configuration->mqtt_client_id))
        || configuration->mqtt_client_id[0] == '\0'
        || strcmp(configuration->mqtt_client_id, configuration->mqtt_username) == 0
        || strchr(configuration->mqtt_client_id, '/') != NULL
        || strchr(configuration->mqtt_client_id, '+') != NULL
        || strchr(configuration->mqtt_client_id, '#') != NULL) {
        return validation_error(error, error_size, "mqtt_client_id is invalid or not distinct");
    }
    if (!terminated(configuration->mqtt_ca_certificate, sizeof(configuration->mqtt_ca_certificate))
        || strstr(configuration->mqtt_ca_certificate, "-----BEGIN CERTIFICATE-----") == NULL) {
        return validation_error(error, error_size, "mqtt_ca_certificate must contain a PEM certificate");
    }
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);
    int certificate_result = mbedtls_x509_crt_parse(
        &certificate,
        (const unsigned char *)configuration->mqtt_ca_certificate,
        strlen(configuration->mqtt_ca_certificate) + 1
    );
    mbedtls_x509_crt_free(&certificate);
    if (certificate_result != 0) {
        return validation_error(error, error_size, "mqtt_ca_certificate is not valid PEM");
    }
    if (configuration->telemetry_interval_seconds < 1
        || configuration->telemetry_interval_seconds > 3600) {
        return validation_error(error, error_size, "telemetry_interval_seconds must be 1-3600");
    }
    if (configuration->machine_status_provider != DEVICE_CONFIG_MACHINE_DISABLED
        && configuration->machine_status_provider != DEVICE_CONFIG_MACHINE_GPIO) {
        return validation_error(error, error_size, "machine_status_provider is unsupported");
    }
    if (configuration->machine_status_provider == DEVICE_CONFIG_MACHINE_GPIO) {
        if (!gpio_allowed(configuration->machine_status_gpio)) {
            return validation_error(error, error_size, "machine_status_gpio is reserved or unsupported");
        }
        if (configuration->machine_status_gpio >= 34
            && configuration->machine_status_pull != DEVICE_CONFIG_PULL_NONE) {
            return validation_error(error, error_size, "GPIO 34-39 do not support internal pulls");
        }
    }
    if (configuration->machine_status_pull < DEVICE_CONFIG_PULL_NONE
        || configuration->machine_status_pull > DEVICE_CONFIG_PULL_DOWN) {
        return validation_error(error, error_size, "machine_status_pull is invalid");
    }
    if (configuration->maintenance_window_seconds < 60
        || configuration->maintenance_window_seconds > 1800) {
        return validation_error(error, error_size, "maintenance_window_seconds must be 60-1800");
    }
    if (configuration->maintenance_max_session_seconds < configuration->maintenance_window_seconds
        || configuration->maintenance_max_session_seconds > 3600) {
        return validation_error(error, error_size, "maintenance_max_session_seconds must cover the window and be <=3600");
    }
    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    return ESP_OK;
}

static esp_err_t open_namespace(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    return nvs_open(NAMESPACE, mode, handle);
}

static void secure_clear(void *value, size_t size)
{
    volatile uint8_t *bytes = value;
    while (bytes != NULL && size-- > 0) {
        *bytes++ = 0;
    }
}

static esp_err_t read_encoded_blob(const char *key, size_t maximum_size, uint8_t **encoded, size_t *size)
{
    if (encoded == NULL || size == NULL) return ESP_ERR_INVALID_ARG;
    *encoded = NULL;
    *size = 0;
    nvs_handle_t handle;
    esp_err_t result = open_namespace(NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    result = nvs_get_blob(handle, key, NULL, size);
    if (result != ESP_OK) {
        nvs_close(handle);
        return result;
    }
    if (*size < DEVICE_CONFIG_STORAGE_HEADER_SIZE || *size > maximum_size) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }
    *encoded = calloc(1, *size);
    if (*encoded == NULL) {
        nvs_close(handle);
        return ESP_ERR_NO_MEM;
    }
    result = nvs_get_blob(handle, key, *encoded, size);
    nvs_close(handle);
    if (result != ESP_OK) {
        secure_clear(*encoded, *size);
        free(*encoded);
        *encoded = NULL;
    }
    return result;
}

static esp_err_t write_encoded_blob(const char *key, const uint8_t *encoded, size_t size)
{
    nvs_handle_t handle;
    esp_err_t result = open_namespace(NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_set_blob(handle, key, encoded, size);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

static esp_err_t read_configuration(const char *key, device_config_t *configuration)
{
    uint8_t *encoded = NULL;
    size_t size = 0;
    esp_err_t result = read_encoded_blob(key, 8192, &encoded, &size);
    if (result == ESP_OK) {
        result = device_config_storage_decode_configuration(encoded, size, configuration);
    }
    if (encoded != NULL) {
        secure_clear(encoded, size);
        free(encoded);
    }
    return result == ESP_OK ? device_config_validate(configuration, NULL, 0) : result;
}

static esp_err_t write_configuration(const char *key, const device_config_t *configuration)
{
    size_t size = device_config_storage_configuration_size();
    uint8_t *encoded = calloc(1, size);
    if (encoded == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = device_config_storage_encode_configuration(configuration, encoded, size);
    if (result == ESP_OK) result = write_encoded_blob(key, encoded, size);
    secure_clear(encoded, size);
    free(encoded);
    return result;
}

static esp_err_t read_metadata(device_config_metadata_t *metadata)
{
    uint8_t *encoded = NULL;
    size_t size = 0;
    esp_err_t result = read_encoded_blob(KEY_METADATA, 1024, &encoded, &size);
    if (result == ESP_OK) result = device_config_storage_decode_metadata(encoded, size, metadata);
    if (encoded != NULL) {
        secure_clear(encoded, size);
        free(encoded);
    }
    return result;
}

static esp_err_t write_metadata(const device_config_metadata_t *metadata)
{
    size_t size = device_config_storage_metadata_size();
    uint8_t *encoded = calloc(1, size);
    if (encoded == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = device_config_storage_encode_metadata(metadata, encoded, size);
    if (result == ESP_OK) result = write_encoded_blob(KEY_METADATA, encoded, size);
    secure_clear(encoded, size);
    free(encoded);
    return result;
}

static esp_err_t erase_key(const char *key)
{
    nvs_handle_t handle;
    esp_err_t result = open_namespace(NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_erase_key(handle, key);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        result = ESP_OK;
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

esp_err_t device_config_storage_init(void)
{
    if (configuration_mutex == NULL) {
        configuration_mutex = xSemaphoreCreateMutexStatic(&configuration_mutex_storage);
        if (configuration_mutex == NULL) return ESP_ERR_NO_MEM;
    }
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "NVS is unusable; refusing automatic erase and entering serial recovery");
    }
    return result;
}

esp_err_t device_config_storage_recover(void)
{
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    esp_err_t result = nvs_flash_erase();
    if (result == ESP_OK) result = nvs_flash_init();
    if (result == ESP_OK) {
        candidate_validation_in_progress = false;
        candidate_validation_revision = 0;
    }
    configuration_unlock();
    return result;
}

esp_err_t device_config_load_active(device_config_t *configuration)
{
    if (configuration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    esp_err_t result = read_configuration(KEY_ACTIVE, configuration);
    configuration_unlock();
    return result;
}

static esp_err_t get_metadata_locked(device_config_metadata_t *metadata)
{
    esp_err_t result = read_metadata(metadata);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        memset(metadata, 0, sizeof(*metadata));
        return ESP_OK;
    }
    return result;
}

esp_err_t device_config_get_metadata(device_config_metadata_t *metadata)
{
    if (metadata == NULL) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    esp_err_t result = get_metadata_locked(metadata);
    configuration_unlock();
    return result;
}

esp_err_t device_config_stage_candidate(const device_config_t *configuration)
{
    char error[128];
    ESP_RETURN_ON_ERROR(device_config_validate(configuration, error, sizeof(error)), TAG, "%s", error);
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    if (candidate_validation_in_progress) {
        configuration_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = write_configuration(KEY_CANDIDATE, configuration);
    if (result != ESP_OK) {
        configuration_unlock();
        return result;
    }
    device_config_metadata_t metadata;
    result = get_metadata_locked(&metadata);
    if (result != ESP_OK) {
        configuration_unlock();
        return result;
    }
    metadata.state = DEVICE_CONFIG_STATE_PENDING;
    metadata.candidate_revision = configuration->revision;
    metadata.boot_attempts = 0;
    result = write_metadata(&metadata);
    configuration_unlock();
    return result;
}

static esp_err_t rollback_candidate_locked(const char *reason)
{
    device_config_metadata_t metadata;
    esp_err_t result = get_metadata_locked(&metadata);
    if (result != ESP_OK) return result;
    metadata.state = DEVICE_CONFIG_STATE_ROLLBACK;
    metadata.candidate_revision = 0;
    metadata.boot_attempts = 0;
    snprintf(metadata.last_rollback_reason, sizeof(metadata.last_rollback_reason), "%s", reason != NULL ? reason : "unspecified");
    result = write_metadata(&metadata);
    if (result == ESP_OK) result = erase_key(KEY_CANDIDATE);
    if (result == ESP_OK) {
        candidate_validation_in_progress = false;
        candidate_validation_revision = 0;
    }
    return result;
}

esp_err_t device_config_load_for_boot(
    device_config_t *configuration,
    bool *using_candidate,
    device_config_metadata_t *metadata
)
{
    if (configuration == NULL || using_candidate == NULL || metadata == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    candidate_validation_in_progress = false;
    candidate_validation_revision = 0;
    esp_err_t result = get_metadata_locked(metadata);
    if (result != ESP_OK) goto done;
    metadata->boot_count++;
    result = write_metadata(metadata);
    if (result != ESP_OK) goto done;
    if (metadata->state == DEVICE_CONFIG_STATE_PENDING) {
        if (metadata->boot_attempts >= CONFIG_PROVISIONING_CANDIDATE_MAX_ATTEMPTS) {
            result = rollback_candidate_locked("candidate boot attempt limit reached");
            if (result != ESP_OK) goto done;
            result = get_metadata_locked(metadata);
            if (result != ESP_OK) goto done;
        } else {
            esp_err_t candidate_result = read_configuration(KEY_CANDIDATE, configuration);
            if (candidate_result == ESP_OK
                && device_config_validate(configuration, NULL, 0) == ESP_OK
                && configuration->revision == metadata->candidate_revision) {
                metadata->boot_attempts++;
                result = write_metadata(metadata);
                if (result == ESP_OK) {
                    candidate_validation_in_progress = true;
                    candidate_validation_revision = configuration->revision;
                    *using_candidate = true;
                }
                goto done;
            }
            result = rollback_candidate_locked("candidate missing, corrupt, invalid or revision-mismatched");
            if (result != ESP_OK) goto done;
            result = get_metadata_locked(metadata);
            if (result != ESP_OK) goto done;
        }
    }
    *using_candidate = false;
    result = read_configuration(KEY_ACTIVE, configuration);
done:
    configuration_unlock();
    return result;
}

static bool configurations_match(const device_config_t *left, const device_config_t *right)
{
    return left->schema_version == right->schema_version
        && left->revision == right->revision
        && strcmp(left->device_id, right->device_id) == 0
        && strcmp(left->wifi_ssid, right->wifi_ssid) == 0
        && strcmp(left->wifi_password, right->wifi_password) == 0
        && strcmp(left->mqtt_broker_uri, right->mqtt_broker_uri) == 0
        && strcmp(left->mqtt_username, right->mqtt_username) == 0
        && strcmp(left->mqtt_password, right->mqtt_password) == 0
        && strcmp(left->mqtt_client_id, right->mqtt_client_id) == 0
        && strcmp(left->mqtt_ca_certificate, right->mqtt_ca_certificate) == 0
        && left->telemetry_interval_seconds == right->telemetry_interval_seconds
        && left->machine_status_provider == right->machine_status_provider
        && left->machine_status_gpio == right->machine_status_gpio
        && left->machine_status_active_high == right->machine_status_active_high
        && left->machine_status_pull == right->machine_status_pull
        && left->maintenance_on_boot == right->maintenance_on_boot
        && left->maintenance_window_seconds == right->maintenance_window_seconds
        && left->maintenance_max_session_seconds == right->maintenance_max_session_seconds;
}

esp_err_t device_config_activate_candidate(const device_config_t *verified_candidate)
{
    if (verified_candidate == NULL) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    if (!candidate_validation_in_progress
        || candidate_validation_revision != verified_candidate->revision) {
        configuration_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    device_config_t *candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL) {
        configuration_unlock();
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = read_configuration(KEY_CANDIDATE, candidate);
    if (result == ESP_OK) result = device_config_validate(candidate, NULL, 0);
    device_config_metadata_t metadata;
    if (result == ESP_OK) result = get_metadata_locked(&metadata);
    if (result == ESP_OK && (metadata.state != DEVICE_CONFIG_STATE_PENDING
        || metadata.candidate_revision != verified_candidate->revision
        || !configurations_match(candidate, verified_candidate))) {
        result = ESP_ERR_INVALID_STATE;
    }
    if (result == ESP_OK) result = write_configuration(KEY_ACTIVE, candidate);
    secure_clear(candidate, sizeof(*candidate));
    free(candidate);
    if (result == ESP_OK) {
        metadata.state = DEVICE_CONFIG_STATE_ACTIVE;
        metadata.candidate_revision = 0;
        metadata.boot_attempts = 0;
        result = write_metadata(&metadata);
    }
    if (result == ESP_OK) result = erase_key(KEY_CANDIDATE);
    if (result == ESP_OK) {
        candidate_validation_in_progress = false;
        candidate_validation_revision = 0;
    }
    configuration_unlock();
    return result;
}

esp_err_t device_config_rollback_candidate(const char *reason)
{
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    esp_err_t result = rollback_candidate_locked(reason);
    configuration_unlock();
    return result;
}

esp_err_t device_config_cancel_candidate(void)
{
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    if (candidate_validation_in_progress) {
        configuration_unlock();
        return ESP_ERR_INVALID_STATE;
    }
    device_config_metadata_t metadata;
    esp_err_t result = get_metadata_locked(&metadata);
    if (result != ESP_OK) {
        configuration_unlock();
        return result;
    }
    metadata.state = DEVICE_CONFIG_STATE_ACTIVE;
    metadata.candidate_revision = 0;
    metadata.boot_attempts = 0;
    result = write_metadata(&metadata);
    if (result == ESP_OK) result = erase_key(KEY_CANDIDATE);
    configuration_unlock();
    return result;
}

bool device_config_candidate_validation_in_progress(void)
{
    if (configuration_lock() != ESP_OK) return false;
    bool in_progress = candidate_validation_in_progress;
    configuration_unlock();
    return in_progress;
}

esp_err_t device_config_factory_reset(void)
{
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    nvs_handle_t handle;
    esp_err_t result = open_namespace(NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        configuration_unlock();
        return result;
    }
    result = nvs_erase_all(handle);
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result == ESP_OK) {
        candidate_validation_in_progress = false;
        candidate_validation_revision = 0;
    }
    configuration_unlock();
    return result;
}

esp_err_t device_config_get_redacted_snapshot(
    const device_config_t *configuration,
    device_config_redacted_t *snapshot
)
{
    if (configuration == NULL || snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->schema_version = configuration->schema_version;
    snapshot->revision = configuration->revision;
    snprintf(snapshot->device_id, sizeof(snapshot->device_id), "%s", configuration->device_id);
    snprintf(snapshot->wifi_ssid, sizeof(snapshot->wifi_ssid), "%s", configuration->wifi_ssid);
    snapshot->wifi_password_configured = configuration->wifi_password[0] != '\0';
    snprintf(snapshot->mqtt_broker_uri, sizeof(snapshot->mqtt_broker_uri), "%s", configuration->mqtt_broker_uri);
    snprintf(snapshot->mqtt_username, sizeof(snapshot->mqtt_username), "%s", configuration->mqtt_username);
    snapshot->mqtt_password_configured = configuration->mqtt_password[0] != '\0';
    snprintf(snapshot->mqtt_client_id, sizeof(snapshot->mqtt_client_id), "%s", configuration->mqtt_client_id);
    snapshot->mqtt_ca_certificate_configured = configuration->mqtt_ca_certificate[0] != '\0';
    snapshot->telemetry_interval_seconds = configuration->telemetry_interval_seconds;
    snapshot->machine_status_provider = configuration->machine_status_provider;
    snapshot->machine_status_gpio = configuration->machine_status_gpio;
    snapshot->machine_status_active_high = configuration->machine_status_active_high;
    snapshot->machine_status_pull = configuration->machine_status_pull;
    snapshot->maintenance_on_boot = configuration->maintenance_on_boot;
    snapshot->maintenance_window_seconds = configuration->maintenance_window_seconds;
    snapshot->maintenance_max_session_seconds = configuration->maintenance_max_session_seconds;
    return ESP_OK;
}

esp_err_t device_config_get_or_create_setup_secret(
    char secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1],
    bool *generated
)
{
    if (secret == NULL || generated == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(secret, 0, DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1);
    *generated = false;
    ESP_RETURN_ON_ERROR(configuration_lock(), TAG, "configuration lock failed");
    nvs_handle_t handle;
    esp_err_t result = open_namespace(NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        configuration_unlock();
        return result;
    }
    size_t length = 0;
    result = nvs_get_str(handle, KEY_SETUP, NULL, &length);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        uint8_t random_bytes[DEVICE_CONFIG_SETUP_SECRET_LENGTH / 2];
        char generated_secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1] = {0};
        bootloader_random_enable();
        esp_fill_random(random_bytes, sizeof(random_bytes));
        bootloader_random_disable();
        for (size_t index = 0; index < sizeof(random_bytes); index++) {
            snprintf(generated_secret + index * 2, 3, "%02x", random_bytes[index]);
        }
        secure_clear(random_bytes, sizeof(random_bytes));
        result = nvs_set_str(handle, KEY_SETUP, generated_secret);
        if (result == ESP_OK) {
            result = nvs_commit(handle);
        }
        if (result == ESP_OK) {
            memcpy(secret, generated_secret, sizeof(generated_secret));
            *generated = true;
        }
        secure_clear(generated_secret, sizeof(generated_secret));
    } else if (result == ESP_OK) {
        if (length != DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1) {
            result = ESP_ERR_INVALID_SIZE;
        } else {
            char stored_secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1] = {0};
            result = nvs_get_str(handle, KEY_SETUP, stored_secret, &length);
            if (result == ESP_OK) {
                for (size_t index = 0; index < DEVICE_CONFIG_SETUP_SECRET_LENGTH; index++) {
                    unsigned char character = (unsigned char)stored_secret[index];
                    bool hexadecimal = (character >= '0' && character <= '9')
                        || (character >= 'A' && character <= 'F')
                        || (character >= 'a' && character <= 'f');
                    if (!hexadecimal) {
                        result = ESP_ERR_INVALID_ARG;
                        break;
                    }
                }
            }
            if (result == ESP_OK && stored_secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH] == '\0') {
                memcpy(secret, stored_secret, sizeof(stored_secret));
            } else if (result == ESP_OK) {
                result = ESP_ERR_INVALID_SIZE;
            }
            secure_clear(stored_secret, sizeof(stored_secret));
        }
    }
    nvs_close(handle);
    if (result != ESP_OK) memset(secret, 0, DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1);
    configuration_unlock();
    return result;
}

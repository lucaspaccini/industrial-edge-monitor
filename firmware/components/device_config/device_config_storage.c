#include "device_config_storage.h"

#include <stdbool.h>
#include <string.h>

#define CONFIGURATION_PAYLOAD_SIZE ( \
    4U + 4U + \
    (DEVICE_CONFIG_DEVICE_ID_MAX + 1U) + \
    (DEVICE_CONFIG_WIFI_SSID_MAX + 1U) + \
    (DEVICE_CONFIG_WIFI_PASSWORD_MAX + 1U) + \
    (DEVICE_CONFIG_MQTT_URI_MAX + 1U) + \
    (DEVICE_CONFIG_MQTT_CA_MAX + 1U) + \
    (DEVICE_CONFIG_MQTT_USERNAME_MAX + 1U) + \
    (DEVICE_CONFIG_MQTT_PASSWORD_MAX + 1U) + \
    (DEVICE_CONFIG_MQTT_CLIENT_ID_MAX + 1U) + \
    4U + 1U + 4U + 1U + 1U + 1U + 4U + 4U)

#define METADATA_PAYLOAD_SIZE (1U + 4U + 4U + 4U + (DEVICE_CONFIG_ROLLBACK_REASON_MAX + 1U))

static const char CONFIGURATION_MAGIC[4] = {'I', 'E', 'M', 'C'};
static const char METADATA_MAGIC[4] = {'I', 'E', 'M', 'M'};

typedef struct {
    uint8_t *data;
    size_t size;
    size_t offset;
} writer_t;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} reader_t;

static bool write_bytes(writer_t *writer, const void *value, size_t size)
{
    if (writer == NULL || value == NULL || writer->offset > writer->size
        || size > writer->size - writer->offset) return false;
    memcpy(writer->data + writer->offset, value, size);
    writer->offset += size;
    return true;
}

static bool write_u8(writer_t *writer, uint8_t value)
{
    return write_bytes(writer, &value, sizeof(value));
}

static bool write_u16(writer_t *writer, uint16_t value)
{
    uint8_t encoded[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    return write_bytes(writer, encoded, sizeof(encoded));
}

static bool write_u32(writer_t *writer, uint32_t value)
{
    uint8_t encoded[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24),
    };
    return write_bytes(writer, encoded, sizeof(encoded));
}

static bool read_bytes(reader_t *reader, void *value, size_t size)
{
    if (reader == NULL || value == NULL || reader->offset > reader->size
        || size > reader->size - reader->offset) return false;
    memcpy(value, reader->data + reader->offset, size);
    reader->offset += size;
    return true;
}

static bool read_u8(reader_t *reader, uint8_t *value)
{
    return read_bytes(reader, value, sizeof(*value));
}

static bool read_u32(reader_t *reader, uint32_t *value)
{
    uint8_t encoded[4];
    if (!read_bytes(reader, encoded, sizeof(encoded))) return false;
    *value = (uint32_t)encoded[0]
        | ((uint32_t)encoded[1] << 8)
        | ((uint32_t)encoded[2] << 16)
        | ((uint32_t)encoded[3] << 24);
    return true;
}

static uint16_t decode_u16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t decode_u32(const uint8_t *value)
{
    return (uint32_t)value[0]
        | ((uint32_t)value[1] << 8)
        | ((uint32_t)value[2] << 16)
        | ((uint32_t)value[3] << 24);
}

static bool write_header(writer_t *writer, const char magic[4], uint32_t payload_size)
{
    return write_bytes(writer, magic, 4)
        && write_u16(writer, DEVICE_CONFIG_STORAGE_FORMAT_VERSION)
        && write_u16(writer, DEVICE_CONFIG_STORAGE_HEADER_SIZE)
        && write_u32(writer, payload_size);
}

size_t device_config_storage_configuration_size(void)
{
    return DEVICE_CONFIG_STORAGE_HEADER_SIZE + CONFIGURATION_PAYLOAD_SIZE;
}

size_t device_config_storage_metadata_size(void)
{
    return DEVICE_CONFIG_STORAGE_HEADER_SIZE + METADATA_PAYLOAD_SIZE;
}

esp_err_t device_config_storage_peek_header(
    const uint8_t *encoded,
    size_t encoded_size,
    const char expected_magic[4],
    device_config_storage_header_t *header
)
{
    if (encoded == NULL || expected_magic == NULL || header == NULL) return ESP_ERR_INVALID_ARG;
    if (encoded_size < DEVICE_CONFIG_STORAGE_HEADER_SIZE) return ESP_ERR_INVALID_SIZE;
    if (memcmp(encoded, expected_magic, 4) != 0) return ESP_ERR_INVALID_ARG;
    header->format_version = decode_u16(encoded + 4);
    header->header_size = decode_u16(encoded + 6);
    header->payload_size = decode_u32(encoded + 8);
    if (header->format_version != DEVICE_CONFIG_STORAGE_FORMAT_VERSION) return ESP_ERR_NOT_SUPPORTED;
    if (header->header_size != DEVICE_CONFIG_STORAGE_HEADER_SIZE
        || header->payload_size != encoded_size - header->header_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t device_config_storage_encode_configuration(
    const device_config_t *configuration,
    uint8_t *encoded,
    size_t encoded_size
)
{
    if (configuration == NULL || encoded == NULL) return ESP_ERR_INVALID_ARG;
    if (encoded_size != device_config_storage_configuration_size()) return ESP_ERR_INVALID_SIZE;
    writer_t writer = {.data = encoded, .size = encoded_size};
    bool ok = write_header(&writer, CONFIGURATION_MAGIC, CONFIGURATION_PAYLOAD_SIZE)
        && write_u32(&writer, configuration->schema_version)
        && write_u32(&writer, configuration->revision)
        && write_bytes(&writer, configuration->device_id, sizeof(configuration->device_id))
        && write_bytes(&writer, configuration->wifi_ssid, sizeof(configuration->wifi_ssid))
        && write_bytes(&writer, configuration->wifi_password, sizeof(configuration->wifi_password))
        && write_bytes(&writer, configuration->mqtt_broker_uri, sizeof(configuration->mqtt_broker_uri))
        && write_bytes(&writer, configuration->mqtt_ca_certificate, sizeof(configuration->mqtt_ca_certificate))
        && write_bytes(&writer, configuration->mqtt_username, sizeof(configuration->mqtt_username))
        && write_bytes(&writer, configuration->mqtt_password, sizeof(configuration->mqtt_password))
        && write_bytes(&writer, configuration->mqtt_client_id, sizeof(configuration->mqtt_client_id))
        && write_u32(&writer, configuration->telemetry_interval_seconds)
        && write_u8(&writer, (uint8_t)configuration->machine_status_provider)
        && write_u32(&writer, (uint32_t)configuration->machine_status_gpio)
        && write_u8(&writer, configuration->machine_status_active_high ? 1U : 0U)
        && write_u8(&writer, (uint8_t)configuration->machine_status_pull)
        && write_u8(&writer, configuration->maintenance_on_boot ? 1U : 0U)
        && write_u32(&writer, configuration->maintenance_window_seconds)
        && write_u32(&writer, configuration->maintenance_max_session_seconds);
    return ok && writer.offset == encoded_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t device_config_storage_decode_configuration(
    const uint8_t *encoded,
    size_t encoded_size,
    device_config_t *configuration
)
{
    if (configuration == NULL) return ESP_ERR_INVALID_ARG;
    device_config_storage_header_t header;
    esp_err_t result = device_config_storage_peek_header(encoded, encoded_size, CONFIGURATION_MAGIC, &header);
    if (result != ESP_OK) return result;
    if (header.payload_size != CONFIGURATION_PAYLOAD_SIZE) return ESP_ERR_INVALID_SIZE;
    memset(configuration, 0, sizeof(*configuration));
    reader_t reader = {
        .data = encoded,
        .size = encoded_size,
        .offset = DEVICE_CONFIG_STORAGE_HEADER_SIZE,
    };
    uint8_t provider;
    uint8_t active_high;
    uint8_t pull;
    uint8_t maintenance;
    uint32_t gpio;
    bool ok = read_u32(&reader, &configuration->schema_version)
        && read_u32(&reader, &configuration->revision)
        && read_bytes(&reader, configuration->device_id, sizeof(configuration->device_id))
        && read_bytes(&reader, configuration->wifi_ssid, sizeof(configuration->wifi_ssid))
        && read_bytes(&reader, configuration->wifi_password, sizeof(configuration->wifi_password))
        && read_bytes(&reader, configuration->mqtt_broker_uri, sizeof(configuration->mqtt_broker_uri))
        && read_bytes(&reader, configuration->mqtt_ca_certificate, sizeof(configuration->mqtt_ca_certificate))
        && read_bytes(&reader, configuration->mqtt_username, sizeof(configuration->mqtt_username))
        && read_bytes(&reader, configuration->mqtt_password, sizeof(configuration->mqtt_password))
        && read_bytes(&reader, configuration->mqtt_client_id, sizeof(configuration->mqtt_client_id))
        && read_u32(&reader, &configuration->telemetry_interval_seconds)
        && read_u8(&reader, &provider)
        && read_u32(&reader, &gpio)
        && read_u8(&reader, &active_high)
        && read_u8(&reader, &pull)
        && read_u8(&reader, &maintenance)
        && read_u32(&reader, &configuration->maintenance_window_seconds)
        && read_u32(&reader, &configuration->maintenance_max_session_seconds);
    if (!ok || reader.offset != encoded_size || active_high > 1U || maintenance > 1U) {
        memset(configuration, 0, sizeof(*configuration));
        return ESP_ERR_INVALID_SIZE;
    }
    configuration->machine_status_provider = (device_config_machine_provider_t)provider;
    configuration->machine_status_gpio = (int32_t)gpio;
    configuration->machine_status_active_high = active_high == 1U;
    configuration->machine_status_pull = (device_config_pull_t)pull;
    configuration->maintenance_on_boot = maintenance == 1U;
    return ESP_OK;
}

esp_err_t device_config_storage_encode_metadata(
    const device_config_metadata_t *metadata,
    uint8_t *encoded,
    size_t encoded_size
)
{
    if (metadata == NULL || encoded == NULL) return ESP_ERR_INVALID_ARG;
    if (encoded_size != device_config_storage_metadata_size()) return ESP_ERR_INVALID_SIZE;
    writer_t writer = {.data = encoded, .size = encoded_size};
    bool ok = write_header(&writer, METADATA_MAGIC, METADATA_PAYLOAD_SIZE)
        && write_u8(&writer, (uint8_t)metadata->state)
        && write_u32(&writer, metadata->candidate_revision)
        && write_u32(&writer, metadata->boot_attempts)
        && write_u32(&writer, metadata->boot_count)
        && write_bytes(&writer, metadata->last_rollback_reason, sizeof(metadata->last_rollback_reason));
    return ok && writer.offset == encoded_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

esp_err_t device_config_storage_decode_metadata(
    const uint8_t *encoded,
    size_t encoded_size,
    device_config_metadata_t *metadata
)
{
    if (metadata == NULL) return ESP_ERR_INVALID_ARG;
    device_config_storage_header_t header;
    esp_err_t result = device_config_storage_peek_header(encoded, encoded_size, METADATA_MAGIC, &header);
    if (result != ESP_OK) return result;
    if (header.payload_size != METADATA_PAYLOAD_SIZE) return ESP_ERR_INVALID_SIZE;
    memset(metadata, 0, sizeof(*metadata));
    reader_t reader = {
        .data = encoded,
        .size = encoded_size,
        .offset = DEVICE_CONFIG_STORAGE_HEADER_SIZE,
    };
    uint8_t state;
    bool ok = read_u8(&reader, &state)
        && read_u32(&reader, &metadata->candidate_revision)
        && read_u32(&reader, &metadata->boot_attempts)
        && read_u32(&reader, &metadata->boot_count)
        && read_bytes(&reader, metadata->last_rollback_reason, sizeof(metadata->last_rollback_reason));
    if (!ok || reader.offset != encoded_size || state > DEVICE_CONFIG_STATE_ROLLBACK
        || metadata->last_rollback_reason[DEVICE_CONFIG_ROLLBACK_REASON_MAX] != '\0') {
        memset(metadata, 0, sizeof(*metadata));
        return ESP_ERR_INVALID_SIZE;
    }
    metadata->state = (device_config_state_t)state;
    return ESP_OK;
}

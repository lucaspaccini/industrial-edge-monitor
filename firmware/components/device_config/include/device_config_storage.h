#pragma once

#include <stddef.h>
#include <stdint.h>

#include "device_config.h"
#include "esp_err.h"

#define DEVICE_CONFIG_STORAGE_FORMAT_VERSION 1U
#define DEVICE_CONFIG_STORAGE_HEADER_SIZE 12U

typedef struct {
    uint16_t format_version;
    uint16_t header_size;
    uint32_t payload_size;
} device_config_storage_header_t;

size_t device_config_storage_configuration_size(void);
size_t device_config_storage_metadata_size(void);

esp_err_t device_config_storage_peek_header(
    const uint8_t *encoded,
    size_t encoded_size,
    const char expected_magic[4],
    device_config_storage_header_t *header
);
esp_err_t device_config_storage_encode_configuration(
    const device_config_t *configuration,
    uint8_t *encoded,
    size_t encoded_size
);
esp_err_t device_config_storage_decode_configuration(
    const uint8_t *encoded,
    size_t encoded_size,
    device_config_t *configuration
);
esp_err_t device_config_storage_encode_metadata(
    const device_config_metadata_t *metadata,
    uint8_t *encoded,
    size_t encoded_size
);
esp_err_t device_config_storage_decode_metadata(
    const uint8_t *encoded,
    size_t encoded_size,
    device_config_metadata_t *metadata
);

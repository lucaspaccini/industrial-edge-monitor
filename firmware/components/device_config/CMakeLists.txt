idf_component_register(
    SRCS "device_config.c" "device_config_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash mbedtls esp_system config bootloader_support freertos
)

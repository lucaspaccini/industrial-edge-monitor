idf_component_register(
    SRCS "machine_status.c"
    INCLUDE_DIRS "include"
    REQUIRES config device_config esp_driver_gpio
)

idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES config wifi mqtt_client_app telemetry sensor i2c_bus bme280 system_time
        machine_status device_health device_health_service device_config diagnostic_log
        provisioning_service provisioning_state serial_recovery
)

idf_component_register(
    SRCS "mqtt_client_app.c" "mqtt_error_state.c"
    INCLUDE_DIRS "include"
    REQUIRES config device_config mqtt esp-tls freertos
)

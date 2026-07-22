idf_component_register(
    SRCS "device_health_service.c"
    INCLUDE_DIRS "include"
    REQUIRES config device_health mqtt_client_app system_time esp_wifi esp_timer esp_system
)

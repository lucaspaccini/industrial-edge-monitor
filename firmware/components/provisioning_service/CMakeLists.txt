idf_component_register(
    SRCS "provisioning_service.c" "provisioning_security.c" "provisioning_softap_endpoint.c"
    INCLUDE_DIRS "include"
    REQUIRES config device_config provisioning_state diagnostic_log wifi mqtt_client_app
        device_health machine_status system_time esp_http_server esp_app_format esp_system
        esp_timer espressif__cjson
)

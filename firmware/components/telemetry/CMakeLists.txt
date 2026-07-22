idf_component_register(
    SRCS
        "telemetry.c"
        "telemetry_model.c"
        "telemetry_json.c"
    INCLUDE_DIRS "include"
    REQUIRES
        config
        mqtt_client_app
        sensor
        machine_status
        system_time
        device_health
)

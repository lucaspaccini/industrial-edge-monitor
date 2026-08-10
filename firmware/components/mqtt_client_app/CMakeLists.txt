idf_component_register(
    SRCS "mqtt_client_app.c"
    INCLUDE_DIRS "include"
    REQUIRES config mqtt esp-tls
)

set(mqtt_ca_cert "${PROJECT_DIR}/${CONFIG_MQTT_BROKER_CA_CERT_PATH}")

if(EXISTS "${mqtt_ca_cert}")
    target_add_binary_data(
        ${COMPONENT_LIB}
        "${mqtt_ca_cert}"
        TEXT
        RENAME_TO mqtt_broker_ca
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MQTT_BROKER_CA_EMBEDDED=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MQTT_BROKER_CA_EMBEDDED=0)
endif()

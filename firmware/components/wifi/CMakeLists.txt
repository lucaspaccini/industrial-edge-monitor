idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES config device_config esp_wifi esp_event esp_netif
)

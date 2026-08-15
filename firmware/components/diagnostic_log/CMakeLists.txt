idf_component_register(
    SRCS "diagnostic_log.c" "diagnostic_ring.c"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer
)

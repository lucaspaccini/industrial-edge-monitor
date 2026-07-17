idf_component_register(
    SRCS "bme280.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES config i2c_bus esp_driver_i2c
)
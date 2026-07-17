#include "i2c_bus.h"

#include "config.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t i2c_bus_handle = NULL;

esp_err_t app_i2c_bus_init(void)
{
    if (i2c_bus_handle != NULL) {
        ESP_LOGW(TAG, "I2C bus already initialized");
        return ESP_OK;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t result = i2c_new_master_bus(
        &bus_config,
        &i2c_bus_handle
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize I2C bus: %s",
            esp_err_to_name(result)
        );
        return result;
    }

    ESP_LOGI(
        TAG,
        "I2C bus initialized: SDA=%d, SCL=%d, frequency=%d Hz",
        CONFIG_I2C_SDA_GPIO,
        CONFIG_I2C_SCL_GPIO,
        CONFIG_I2C_FREQUENCY_HZ
    );

    return ESP_OK;
}

i2c_master_bus_handle_t app_i2c_bus_get_handle(void)
{
    return i2c_bus_handle;
}
#include "bme280.h"
#include "bme280_registers.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

static const char *TAG = "bme280";

#define BME280_I2C_TIMEOUT_MS             1000
#define BME280_RESET_DELAY_MS             3
#define BME280_STARTUP_POLL_DELAY_MS      2
#define BME280_STARTUP_MAX_ATTEMPTS       10

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;

    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
} bme280_calibration_t;

typedef struct {
    int32_t temperature;
    int32_t pressure;
    int32_t humidity;
} bme280_raw_data_t;

static i2c_master_dev_handle_t bme280_device = NULL;
static bool bme280_ready = false;
static bme280_calibration_t calibration;
static int32_t temperature_fine = 0;

static uint16_t bme280_uint16_from_le(const uint8_t *data)
{
    return (uint16_t)data[0]
        | ((uint16_t)data[1] << 8);
}

static int16_t bme280_int16_from_le(const uint8_t *data)
{
    return (int16_t)bme280_uint16_from_le(data);
}

static esp_err_t bme280_read_registers(
    uint8_t register_address,
    uint8_t *buffer,
    size_t length
)
{
    if (bme280_device == NULL) {
        ESP_LOGE(TAG, "BME280 device is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (buffer == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = i2c_master_transmit_receive(
        bme280_device,
        &register_address,
        sizeof(register_address),
        buffer,
        length,
        BME280_I2C_TIMEOUT_MS
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to read register 0x%02X: %s",
            register_address,
            esp_err_to_name(result)
        );
    }

    return result;
}

static esp_err_t bme280_read_register(
    uint8_t register_address,
    uint8_t *value
)
{
    return bme280_read_registers(
        register_address,
        value,
        sizeof(*value)
    );
}

static esp_err_t bme280_write_register(
    uint8_t register_address,
    uint8_t value
)
{
    if (bme280_device == NULL) {
        ESP_LOGE(TAG, "BME280 device is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t buffer[] = {
        register_address,
        value
    };

    esp_err_t result = i2c_master_transmit(
        bme280_device,
        buffer,
        sizeof(buffer),
        BME280_I2C_TIMEOUT_MS
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to write register 0x%02X: %s",
            register_address,
            esp_err_to_name(result)
        );
    }

    return result;
}

static esp_err_t bme280_remove_device(void)
{
    if (bme280_device == NULL) {
        return ESP_OK;
    }

    esp_err_t result = i2c_master_bus_rm_device(bme280_device);

    if (result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Failed to remove BME280 device: %s",
            esp_err_to_name(result)
        );
        return result;
    }

    bme280_device = NULL;
    return ESP_OK;
}

static esp_err_t bme280_wait_for_calibration_copy(void)
{
    for (uint32_t attempt = 0;
         attempt < BME280_STARTUP_MAX_ATTEMPTS;
         ++attempt) {

        uint8_t status = 0;

        esp_err_t result = bme280_read_register(
            BME280_REG_STATUS,
            &status
        );

        if (result != ESP_OK) {
            return result;
        }

        if ((status & BME280_STATUS_IM_UPDATE_MASK) == 0) {
            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(BME280_STARTUP_POLL_DELAY_MS));
    }

    ESP_LOGE(TAG, "Timeout waiting for calibration data copy");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t bme280_read_calibration(void)
{
    uint8_t block_1[BME280_CALIBRATION_BLOCK_1_SIZE];
    uint8_t block_2[BME280_CALIBRATION_BLOCK_2_SIZE];

    esp_err_t result = bme280_read_registers(
        BME280_REG_CALIB_T1,
        block_1,
        sizeof(block_1)
    );

    if (result != ESP_OK) {
        return result;
    }

    result = bme280_read_registers(
        BME280_REG_CALIB_H2,
        block_2,
        sizeof(block_2)
    );

    if (result != ESP_OK) {
        return result;
    }

    calibration.dig_t1 = bme280_uint16_from_le(&block_1[0]);
    calibration.dig_t2 = bme280_int16_from_le(&block_1[2]);
    calibration.dig_t3 = bme280_int16_from_le(&block_1[4]);

    calibration.dig_p1 = bme280_uint16_from_le(&block_1[6]);
    calibration.dig_p2 = bme280_int16_from_le(&block_1[8]);
    calibration.dig_p3 = bme280_int16_from_le(&block_1[10]);
    calibration.dig_p4 = bme280_int16_from_le(&block_1[12]);
    calibration.dig_p5 = bme280_int16_from_le(&block_1[14]);
    calibration.dig_p6 = bme280_int16_from_le(&block_1[16]);
    calibration.dig_p7 = bme280_int16_from_le(&block_1[18]);
    calibration.dig_p8 = bme280_int16_from_le(&block_1[20]);
    calibration.dig_p9 = bme280_int16_from_le(&block_1[22]);

    calibration.dig_h1 = block_1[25];
    calibration.dig_h2 = bme280_int16_from_le(&block_2[0]);
    calibration.dig_h3 = block_2[2];

    /*
     * H4 and H5 are 12-bit signed values sharing register E5.
     * Multiplication is used instead of left-shifting negative values.
     */
    calibration.dig_h4 =
        (int16_t)(
            ((int16_t)(int8_t)block_2[3] * 16)
            | (block_2[4] & 0x0F)
        );

    calibration.dig_h5 =
        (int16_t)(
            ((int16_t)(int8_t)block_2[5] * 16)
            | (block_2[4] >> 4)
        );

    calibration.dig_h6 = (int8_t)block_2[6];

    if (calibration.dig_p1 == 0) {
        ESP_LOGE(TAG, "Invalid pressure calibration coefficient dig_p1");
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "BME280 calibration data loaded");

    return ESP_OK;
}

static esp_err_t bme280_configure(void)
{
    /*
     * ctrl_hum must be written before ctrl_meas because the humidity
     * configuration becomes effective when ctrl_meas is written.
     */
    esp_err_t result = bme280_write_register(
        BME280_REG_CTRL_HUM,
        BME280_CTRL_HUM_VALUE
    );

    if (result != ESP_OK) {
        return result;
    }

    result = bme280_write_register(
        BME280_REG_CONFIG,
        BME280_CONFIG_VALUE
    );

    if (result != ESP_OK) {
        return result;
    }

    return bme280_write_register(
        BME280_REG_CTRL_MEAS,
        BME280_CTRL_MEAS_VALUE
    );
}

static esp_err_t bme280_read_raw(bme280_raw_data_t *raw_data)
{
    if (raw_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[BME280_MEASUREMENT_DATA_SIZE];

    esp_err_t result = bme280_read_registers(
        BME280_REG_PRESS_MSB,
        buffer,
        sizeof(buffer)
    );

    if (result != ESP_OK) {
        return result;
    }

    raw_data->pressure =
        ((int32_t)buffer[0] << 12)
        | ((int32_t)buffer[1] << 4)
        | ((int32_t)buffer[2] >> 4);

    raw_data->temperature =
        ((int32_t)buffer[3] << 12)
        | ((int32_t)buffer[4] << 4)
        | ((int32_t)buffer[5] >> 4);

    raw_data->humidity =
        ((int32_t)buffer[6] << 8)
        | (int32_t)buffer[7];

    return ESP_OK;
}

static float bme280_compensate_temperature(int32_t raw_temperature)
{
    int32_t variable_1;
    int32_t variable_2;

    variable_1 =
        ((((raw_temperature >> 3)
        - ((int32_t)calibration.dig_t1 << 1)))
        * (int32_t)calibration.dig_t2)
        >> 11;

    variable_2 =
        (((((raw_temperature >> 4)
        - (int32_t)calibration.dig_t1)
        * ((raw_temperature >> 4)
        - (int32_t)calibration.dig_t1))
        >> 12)
        * (int32_t)calibration.dig_t3)
        >> 14;

    temperature_fine = variable_1 + variable_2;

    const int32_t temperature_x100 =
        (temperature_fine * 5 + 128) >> 8;

    return (float)temperature_x100 / 100.0f;
}

static float bme280_compensate_pressure(int32_t raw_pressure)
{
    int64_t variable_1;
    int64_t variable_2;
    int64_t pressure;

    variable_1 = (int64_t)temperature_fine - 128000;
    variable_2 =
        variable_1
        * variable_1
        * (int64_t)calibration.dig_p6;

    variable_2 +=
        (variable_1 * (int64_t)calibration.dig_p5) << 17;

    variable_2 +=
        ((int64_t)calibration.dig_p4) << 35;

    variable_1 =
        ((variable_1
        * variable_1
        * (int64_t)calibration.dig_p3) >> 8)
        + ((variable_1
        * (int64_t)calibration.dig_p2) << 12);

    variable_1 =
        (((((int64_t)1) << 47) + variable_1)
        * (int64_t)calibration.dig_p1)
        >> 33;

    if (variable_1 == 0) {
        ESP_LOGE(TAG, "Pressure compensation division by zero");
        return 0.0f;
    }

    pressure = 1048576 - raw_pressure;

    pressure =
        (((pressure << 31) - variable_2) * 3125)
        / variable_1;

    variable_1 =
        ((int64_t)calibration.dig_p9
        * (pressure >> 13)
        * (pressure >> 13))
        >> 25;

    variable_2 =
        ((int64_t)calibration.dig_p8 * pressure)
        >> 19;

    pressure =
        ((pressure + variable_1 + variable_2) >> 8)
        + ((int64_t)calibration.dig_p7 << 4);

    const float pressure_pa = (float)pressure / 256.0f;

    return pressure_pa / 100.0f;
}

static float bme280_compensate_humidity(int32_t raw_humidity)
{
    int32_t humidity;

    humidity = temperature_fine - 76800;

    humidity =
        (((((raw_humidity << 14)
        - ((int32_t)calibration.dig_h4 << 20)
        - ((int32_t)calibration.dig_h5 * humidity))
        + 16384) >> 15)
        *
        (((((((humidity * (int32_t)calibration.dig_h6) >> 10)
        * (((humidity * (int32_t)calibration.dig_h3) >> 11)
        + 32768)) >> 10)
        + 2097152)
        * (int32_t)calibration.dig_h2
        + 8192) >> 14));

    humidity -=
        (((((humidity >> 15)
        * (humidity >> 15)) >> 7)
        * (int32_t)calibration.dig_h1)
        >> 4);

    if (humidity < 0) {
        humidity = 0;
    }

    if (humidity > 419430400) {
        humidity = 419430400;
    }

    const uint32_t humidity_x1024 =
        (uint32_t)(humidity >> 12);

    return (float)humidity_x1024 / 1024.0f;
}

esp_err_t bme280_init(void)
{
    if (bme280_device != NULL && bme280_ready) {
        ESP_LOGW(TAG, "BME280 already initialized");
        return ESP_OK;
    }

    if (bme280_device != NULL) {
        esp_err_t remove_result = bme280_remove_device();
        if (remove_result != ESP_OK) {
            return remove_result;
        }
    }

    i2c_master_bus_handle_t bus_handle = app_i2c_bus_get_handle();

    if (bus_handle == NULL) {
        ESP_LOGE(TAG, "I2C bus is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_BME280_I2C_ADDRESS,
        .scl_speed_hz = CONFIG_I2C_FREQUENCY_HZ,
    };

    esp_err_t result = i2c_master_bus_add_device(
        bus_handle,
        &device_config,
        &bme280_device
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to add BME280 device: %s",
            esp_err_to_name(result)
        );
        return result;
    }

    uint8_t chip_id = 0;

    result = bme280_read_register(
        BME280_REG_CHIP_ID,
        &chip_id
    );

    if (result != ESP_OK) {
        bme280_remove_device();
        return result;
    }

    if (chip_id != BME280_CHIP_ID) {
        ESP_LOGE(
            TAG,
            "Unexpected chip ID: 0x%02X (expected 0x%02X)",
            chip_id,
            BME280_CHIP_ID
        );

        bme280_remove_device();
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(
        TAG,
        "BME280 detected at address 0x%02X, chip ID: 0x%02X",
        CONFIG_BME280_I2C_ADDRESS,
        chip_id
    );

    result = bme280_write_register(
        BME280_REG_RESET,
        BME280_RESET_COMMAND
    );

    if (result != ESP_OK) {
        bme280_remove_device();
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(BME280_RESET_DELAY_MS));

    result = bme280_wait_for_calibration_copy();

    if (result != ESP_OK) {
        bme280_remove_device();
        return result;
    }

    result = bme280_read_calibration();

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load calibration data");
        bme280_remove_device();
        return result;
    }

    result = bme280_configure();

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure BME280");
        bme280_remove_device();
        return result;
    }

    ESP_LOGI(TAG, "BME280 initialized successfully");
    bme280_ready = true;

    return ESP_OK;
}

esp_err_t bme280_deinit(void)
{
    bme280_ready = false;
    return bme280_remove_device();
}

esp_err_t bme280_read(bme280_measurement_t *measurement)
{
    if (measurement == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (bme280_device == NULL || !bme280_ready) {
        ESP_LOGE(TAG, "BME280 is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    bme280_raw_data_t raw_data;

    esp_err_t result = bme280_read_raw(&raw_data);

    if (result != ESP_OK) {
        return result;
    }

    /*
     * Temperature must be compensated first because pressure and humidity
     * compensation depend on temperature_fine.
     */
    measurement->temperature_c =
        bme280_compensate_temperature(raw_data.temperature);

    measurement->pressure_hpa =
        bme280_compensate_pressure(raw_data.pressure);

    measurement->humidity_percent =
        bme280_compensate_humidity(raw_data.humidity);

    return ESP_OK;
}

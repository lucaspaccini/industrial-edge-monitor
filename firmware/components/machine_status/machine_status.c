#include "machine_status.h"

#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "machine_status";
static bool initialized;
static device_config_machine_provider_t provider;
static int32_t input_gpio;
static bool active_high;
static device_config_pull_t pull;

bool machine_status_is_enabled(void)
{
    return provider == DEVICE_CONFIG_MACHINE_GPIO;
}

esp_err_t machine_status_init(const device_config_t *configuration)
{
    if (configuration == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    provider = configuration->machine_status_provider;
    input_gpio = configuration->machine_status_gpio;
    active_high = configuration->machine_status_active_high;
    pull = configuration->machine_status_pull;
    initialized = false;
    if (!machine_status_is_enabled()) {
        ESP_LOGI(TAG, "Machine status provider disabled");
        return ESP_OK;
    }
    gpio_pull_mode_t pull_mode = GPIO_FLOATING;
    if (pull == DEVICE_CONFIG_PULL_UP) {
        pull_mode = GPIO_PULLUP_ONLY;
    } else if (pull == DEVICE_CONFIG_PULL_DOWN) {
        pull_mode = GPIO_PULLDOWN_ONLY;
    }
    const gpio_config_t gpio_configuration = {
        .pin_bit_mask = 1ULL << input_gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_mode == GPIO_PULLUP_ONLY,
        .pull_down_en = pull_mode == GPIO_PULLDOWN_ONLY,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&gpio_configuration);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(result));
        return result;
    }
    initialized = true;
    ESP_LOGI(TAG, "GPIO provider initialized on GPIO %" PRId32, input_gpio);
    return ESP_OK;
}

esp_err_t machine_status_get(machine_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *status = MACHINE_STATUS_UNKNOWN;
    if (!machine_status_is_enabled()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    bool active = gpio_get_level(input_gpio) != 0;
    if (!active_high) {
        active = !active;
    }
    *status = active ? MACHINE_STATUS_RUNNING : MACHINE_STATUS_STOPPED;
    return ESP_OK;
}

const char *machine_status_to_string(machine_status_t status)
{
    switch (status) {
        case MACHINE_STATUS_RUNNING: return "running";
        case MACHINE_STATUS_STOPPED: return "stopped";
        case MACHINE_STATUS_UNKNOWN:
        default: return "unknown";
    }
}

#include "machine_status.h"

#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "machine_status";
static bool initialized = false;

bool machine_status_is_enabled(void)
{
#if CONFIG_MACHINE_STATUS_PROVIDER_GPIO
    return true;
#else
    return false;
#endif
}

esp_err_t machine_status_init(void)
{
    if (!machine_status_is_enabled()) {
        ESP_LOGI(TAG, "Machine status provider disabled");
        return ESP_OK;
    }

#if CONFIG_MACHINE_STATUS_PROVIDER_GPIO
    gpio_pull_mode_t pull_mode = GPIO_FLOATING;

#if CONFIG_MACHINE_STATUS_PULL_UP
    pull_mode = GPIO_PULLUP_ONLY;
#elif CONFIG_MACHINE_STATUS_PULL_DOWN
    pull_mode = GPIO_PULLDOWN_ONLY;
#endif

    const gpio_config_t machine_gpio_config = {
        .pin_bit_mask = 1ULL << CONFIG_MACHINE_STATUS_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull_mode == GPIO_PULLUP_ONLY,
        .pull_down_en = pull_mode == GPIO_PULLDOWN_ONLY,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t result = gpio_config(&machine_gpio_config);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(result));
        return result;
    }

    initialized = true;
    ESP_LOGI(TAG, "GPIO provider initialized on GPIO %d", CONFIG_MACHINE_STATUS_GPIO);
#endif

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

#if CONFIG_MACHINE_STATUS_PROVIDER_GPIO
    int level = gpio_get_level(CONFIG_MACHINE_STATUS_GPIO);
    bool active = level != 0;

#if CONFIG_MACHINE_STATUS_ACTIVE_LOW
    active = !active;
#endif

    *status = active ? MACHINE_STATUS_RUNNING : MACHINE_STATUS_STOPPED;
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

const char *machine_status_to_string(machine_status_t status)
{
    switch (status) {
        case MACHINE_STATUS_RUNNING:
            return "running";

        case MACHINE_STATUS_STOPPED:
            return "stopped";

        case MACHINE_STATUS_UNKNOWN:
        default:
            return "unknown";
    }
}

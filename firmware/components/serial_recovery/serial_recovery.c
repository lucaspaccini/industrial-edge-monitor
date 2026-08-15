#include "serial_recovery.h"

#include <stdio.h>
#include <string.h>

#include "device_config.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "serial_recovery";
static TaskHandle_t recovery_task_handle;

static void recovery_task(void *parameters)
{
    (void)parameters;
    char line[96];
    while (true) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "factory-reset ERASE-DEVICE-CONFIGURATION") != 0) {
            ESP_LOGW(TAG, "Rejected serial recovery command");
            continue;
        }
        ESP_LOGW(TAG, "Confirmed local recovery; erasing and reinitializing the complete NVS partition");
        esp_err_t result = device_config_storage_recover();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Factory reset failed: %s", esp_err_to_name(result));
            continue;
        }
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
    }
}

esp_err_t serial_recovery_start(void)
{
    if (recovery_task_handle != NULL) {
        return ESP_OK;
    }
    BaseType_t result = xTaskCreate(
        recovery_task,
        "serial_recovery",
        3072,
        NULL,
        3,
        &recovery_task_handle
    );
    if (result != pdPASS) {
        recovery_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Local recovery command is available on the serial console");
    return ESP_OK;
}

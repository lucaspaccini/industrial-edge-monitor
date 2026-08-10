#include "esp_log.h"

#include "config.h"
#include "device_health.h"
#include "device_health_service.h"
#include "wifi.h"
#include "mqtt_client_app.h"
#include "machine_status.h"
#include "telemetry.h"
#include "sensor.h"
#include "system_time.h"
#include "i2c_bus.h"
#include "bme280.h"


static const char *TAG = "industrial_edge_monitor";

void app_main(void)
{
    ESP_LOGI(TAG, "Industrial Edge Monitor firmware started");

    ESP_ERROR_CHECK(device_health_init());

    ESP_ERROR_CHECK(app_i2c_bus_init());   
    esp_err_t sensor_result = sensor_init();

    if (sensor_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Sensor initialization failed: %s; retrying from telemetry task",
            esp_err_to_name(sensor_result)
        );
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_SENSOR,
            DEVICE_HEALTH_COMPONENT_FAULT,
            DEVICE_HEALTH_ERROR_NOT_INITIALIZED,
            NULL
        );
    } else {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_SENSOR,
            DEVICE_HEALTH_COMPONENT_HEALTHY,
            DEVICE_HEALTH_ERROR_NONE,
            NULL
        );
    }
    esp_err_t machine_status_result = machine_status_init();

    if (machine_status_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Machine status initialization failed: %s; using unknown status",
            esp_err_to_name(machine_status_result)
        );

        if (machine_status_result == ESP_ERR_NOT_SUPPORTED) {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
                DEVICE_HEALTH_COMPONENT_UNKNOWN,
                DEVICE_HEALTH_ERROR_NONE,
                NULL
            );
        } else {
            device_health_update_component(
                DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
                DEVICE_HEALTH_COMPONENT_FAULT,
                DEVICE_HEALTH_ERROR_MACHINE_STATUS_UNAVAILABLE,
                NULL
            );
        }
    } else {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
            machine_status_is_enabled()
                ? DEVICE_HEALTH_COMPONENT_HEALTHY
                : DEVICE_HEALTH_COMPONENT_UNKNOWN,
            DEVICE_HEALTH_ERROR_NONE,
            NULL
        );
    }
    
    wifi_init();

    esp_err_t time_result = system_time_init();

    if (time_result != ESP_OK && time_result != ESP_ERR_TIMEOUT) {
        ESP_LOGE(
            TAG,
            "System time initialization failed: %s; telemetry will remain paused",
            esp_err_to_name(time_result)
        );
    }

    esp_err_t mqtt_result = mqtt_init();

    if (mqtt_result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Secure MQTT initialization failed: %s; transport-dependent tasks not started",
            esp_err_to_name(mqtt_result)
        );
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MQTT,
            DEVICE_HEALTH_COMPONENT_FAULT,
            DEVICE_HEALTH_ERROR_NOT_INITIALIZED,
            NULL
        );
        return;
    }

    telemetry_start();
    ESP_ERROR_CHECK(device_health_service_start());
}

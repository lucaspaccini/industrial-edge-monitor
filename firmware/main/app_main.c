#include "esp_log.h"

#include "config.h"
#include "wifi.h"
#include "mqtt_client_app.h"
#include "telemetry.h"
#include "sensor.h"
#include "system_time.h"
#include "i2c_bus.h"
#include "bme280.h"


static const char *TAG = "industrial_edge_monitor";

void app_main(void)
{
    ESP_LOGI(TAG, "Industrial Edge Monitor firmware started");

    ESP_ERROR_CHECK(app_i2c_bus_init());   
    ESP_ERROR_CHECK(sensor_init());
    
    wifi_init();

    esp_err_t time_result = system_time_init();

    if (time_result != ESP_OK && time_result != ESP_ERR_TIMEOUT) {
        ESP_LOGE(
            TAG,
            "System time initialization failed: %s; telemetry will remain paused",
            esp_err_to_name(time_result)
        );
    }

    mqtt_init();
}

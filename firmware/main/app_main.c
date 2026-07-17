#include "esp_log.h"

#include "config.h"
#include "wifi.h"
#include "mqtt_client_app.h"
#include "telemetry.h"
#include "sensor.h"
#include "i2c_bus.h"
#include "bme280.h"


static const char *TAG = "industrial_edge_monitor";

void app_main(void)
{
    ESP_LOGI(TAG, "Industrial Edge Monitor firmware started");

    ESP_ERROR_CHECK(app_i2c_bus_init());   
    ESP_ERROR_CHECK(sensor_init());
    
    wifi_init();
    mqtt_init();
}
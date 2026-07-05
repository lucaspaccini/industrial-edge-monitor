#include "esp_log.h"

#include "config.h"
#include "wifi.h"
#include "mqtt_client.h"
#include "telemetry.h"

static const char *TAG = "industrial_edge_monitor";

void app_main(void)
{
    ESP_LOGI(TAG, "Industrial Edge Monitor firmware started");

    wifi_init();
    mqtt_init();
    telemetry_start();
}
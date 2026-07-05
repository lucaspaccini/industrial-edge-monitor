#include "mqtt_client.h"

#include "esp_log.h"

static const char *TAG = "mqtt";

void mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT");
    ESP_LOGI(TAG, "Host: %s", CONFIG_MQTT_HOST);
    ESP_LOGI(TAG, "Port: %d", CONFIG_MQTT_PORT);
    ESP_LOGI(TAG, "Topic: %s", CONFIG_MQTT_TOPIC);
    ESP_LOGI(TAG, "Client ID: %s", CONFIG_MQTT_CLIENT_ID);
    ESP_LOGI(TAG, "QoS: %d", CONFIG_MQTT_QOS);
}
#include "mqtt_client_app.h"

#include <stdbool.h>

#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "telemetry.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");

            telemetry_start();

            
            break;

        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Telemetry published, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;

        default:
            break;
    }
}

void mqtt_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT");
    ESP_LOGI(TAG, "Host: %s", CONFIG_MQTT_HOST);
    ESP_LOGI(TAG, "Port: %d", CONFIG_MQTT_PORT);
    ESP_LOGI(TAG, "Topic: %s", CONFIG_MQTT_TOPIC);
    ESP_LOGI(TAG, "Client ID: %s", CONFIG_MQTT_CLIENT_ID);
    ESP_LOGI(TAG, "QoS: %d", CONFIG_MQTT_QOS);

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.hostname = CONFIG_MQTT_HOST,
        .broker.address.port = CONFIG_MQTT_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);

    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    esp_mqtt_client_start(mqtt_client);
}

void mqtt_publish_telemetry(const char *payload)
{
    if (!mqtt_connected) {
        ESP_LOGW(TAG, "MQTT client not connected, skipping publish");
        return;
    }

    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        CONFIG_MQTT_TOPIC,
        payload,
        0,
        CONFIG_MQTT_QOS,
        0
    );

    ESP_LOGI(TAG, "Publish requested, msg_id=%d", msg_id);
}
#include "mqtt_client_app.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static atomic_bool mqtt_connected = ATOMIC_VAR_INIT(false);
static atomic_uint connection_generation = ATOMIC_VAR_INIT(0);

#define MQTT_TOPIC_SIZE 128
#define MQTT_AVAILABILITY_PAYLOAD_SIZE 160

static char telemetry_topic[MQTT_TOPIC_SIZE];
static char health_topic[MQTT_TOPIC_SIZE];
static char availability_topic[MQTT_TOPIC_SIZE];
static char offline_payload[MQTT_AVAILABILITY_PAYLOAD_SIZE];

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
            atomic_store(&mqtt_connected, true);
            atomic_fetch_add(&connection_generation, 1);
            ESP_LOGI(TAG, "Connected to MQTT broker");
            break;

        case MQTT_EVENT_DISCONNECTED:
            atomic_store(&mqtt_connected, false);
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT publish acknowledged, msg_id=%d", event->msg_id);
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
    snprintf(
        telemetry_topic,
        sizeof(telemetry_topic),
        "%s/%s/telemetry",
        CONFIG_MQTT_TOPIC_PREFIX,
        CONFIG_DEVICE_ID
    );
    snprintf(
        health_topic,
        sizeof(health_topic),
        "%s/%s/health",
        CONFIG_MQTT_TOPIC_PREFIX,
        CONFIG_DEVICE_ID
    );
    snprintf(
        availability_topic,
        sizeof(availability_topic),
        "%s/%s/availability",
        CONFIG_MQTT_TOPIC_PREFIX,
        CONFIG_DEVICE_ID
    );
    snprintf(
        offline_payload,
        sizeof(offline_payload),
        "{\"schema_version\":1,\"device_id\":\"%s\",\"status\":\"offline\"}",
        CONFIG_DEVICE_ID
    );

    ESP_LOGI(TAG, "Telemetry topic: %s", telemetry_topic);
    ESP_LOGI(TAG, "Client ID: %s", CONFIG_MQTT_CLIENT_ID);
    ESP_LOGI(TAG, "QoS: %d", CONFIG_MQTT_QOS);

    esp_mqtt_client_config_t mqtt_config = {
        .broker.address.hostname = CONFIG_MQTT_HOST,
        .broker.address.port = CONFIG_MQTT_PORT,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
        .session.last_will.topic = availability_topic,
        .session.last_will.msg = offline_payload,
        .session.last_will.qos = CONFIG_MQTT_QOS,
        .session.last_will.retain = 1,
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

static esp_err_t mqtt_publish(
    const char *topic,
    const char *payload,
    bool retain
)
{
    if (topic == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (mqtt_client == NULL || !atomic_load(&mqtt_connected)) {
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        topic,
        payload,
        0,
        CONFIG_MQTT_QOS,
        retain ? 1 : 0
    );

    if (msg_id < 0) {
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "MQTT publish accepted, msg_id=%d", msg_id);
    return ESP_OK;
}

esp_err_t mqtt_publish_telemetry(const char *payload)
{
    return mqtt_publish(telemetry_topic, payload, false);
}

esp_err_t mqtt_publish_health(const char *payload)
{
    return mqtt_publish(health_topic, payload, true);
}

esp_err_t mqtt_publish_availability(const char *payload)
{
    return mqtt_publish(availability_topic, payload, true);
}

bool mqtt_is_connected(void)
{
    return atomic_load(&mqtt_connected);
}

uint32_t mqtt_connection_generation(void)
{
    return atomic_load(&connection_generation);
}

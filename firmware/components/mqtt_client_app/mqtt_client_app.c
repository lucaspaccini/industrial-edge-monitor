#include "mqtt_client_app.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_tls_errors.h"
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

#if MQTT_BROKER_CA_EMBEDDED
extern const uint8_t mqtt_broker_ca_start[] asm("_binary_mqtt_broker_ca_start");
#endif

static bool mqtt_configuration_is_valid(void)
{
    const char secure_scheme[] = "mqtts://";

    if (strncmp(CONFIG_MQTT_BROKER_URI, secure_scheme, sizeof(secure_scheme) - 1) != 0
        || strstr(CONFIG_MQTT_BROKER_URI, "<broker-host>") != NULL
        || strchr(CONFIG_MQTT_BROKER_URI, '@') != NULL) {
        ESP_LOGE(TAG, "MQTT broker URI must use mqtts:// without embedded credentials");
        return false;
    }

    if (CONFIG_MQTT_USERNAME[0] == '\0' || CONFIG_MQTT_PASSWORD[0] == '\0') {
        ESP_LOGE(TAG, "MQTT username and password are not configured");
        return false;
    }

#if !MQTT_BROKER_CA_EMBEDDED
    ESP_LOGE(
        TAG,
        "Broker CA certificate is not embedded; expected configured local certificate path"
    );
    return false;
#endif

    return true;
}

static void log_mqtt_error(const esp_mqtt_error_codes_t *error)
{
    if (error == NULL) {
        ESP_LOGE(TAG, "MQTT error without diagnostic context");
        return;
    }

    if (error->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        if (error->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME) {
            ESP_LOGE(TAG, "MQTT authentication failed: username or password rejected");
        } else if (error->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) {
            ESP_LOGE(TAG, "MQTT authorization failed: broker rejected the client");
        } else {
            ESP_LOGE(
                TAG,
                "MQTT broker refused the connection, reason=%d",
                error->connect_return_code
            );
        }
        return;
    }

    if (error->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        if (error->esp_tls_cert_verify_flags != 0) {
            ESP_LOGE(TAG, "MQTT TLS certificate or hostname verification failed");
        } else if (error->esp_tls_last_esp_err == ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME) {
            ESP_LOGE(TAG, "MQTT broker DNS resolution failed");
        } else if (error->esp_tls_last_esp_err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "MQTT TLS/transport connection failed: %s",
                esp_err_to_name(error->esp_tls_last_esp_err)
            );
        } else {
            ESP_LOGE(TAG, "MQTT transport connection failed");
        }
        return;
    }

    ESP_LOGE(TAG, "MQTT client error type=%d", error->error_type);
}

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
            log_mqtt_error(event->error_handle);
            break;

        default:
            break;
    }
}

esp_err_t mqtt_init(void)
{
    if (!mqtt_configuration_is_valid()) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Initializing authenticated MQTT over TLS");
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
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
#if MQTT_BROKER_CA_EMBEDDED
        .broker.verification.certificate = (const char *)mqtt_broker_ca_start,
#endif
        .broker.verification.skip_cert_common_name_check = false,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.client_id = CONFIG_MQTT_CLIENT_ID,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
        .session.last_will.topic = availability_topic,
        .session.last_will.msg = offline_payload,
        .session.last_will.qos = CONFIG_MQTT_QOS,
        .session.last_will.retain = 1,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);

    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return ESP_FAIL;
    }

    esp_err_t result = esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL
    );

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(result));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return result;
    }

    result = esp_mqtt_client_start(mqtt_client);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(result));
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return result;
    }

    return ESP_OK;
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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "bme280.h"
#include "device_config.h"
#include "device_health.h"
#include "device_health_service.h"
#include "diagnostic_log.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "machine_status.h"
#include "mqtt_client_app.h"
#include "nvs.h"
#include "provisioning_service.h"
#include "provisioning_state.h"
#include "sensor.h"
#include "serial_recovery.h"
#include "system_time.h"
#include "telemetry.h"
#include "wifi.h"

static const char *TAG = "industrial_edge_monitor";
static device_config_t runtime_configuration;
static provisioning_state_machine_t provisioning_machine;
static char setup_secret[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1];

static void log_stack_margin(const char *context)
{
    ESP_LOGI(TAG, "%s stack high-water mark=%u bytes", context,
        (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

static uint64_t monotonic_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void restart_after_candidate_failure(const char *reason)
{
    ESP_LOGE(TAG, "Candidate configuration rejected: %s", reason);
    esp_err_t result = device_config_rollback_candidate(reason);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist rollback decision: %s", esp_err_to_name(result));
    }
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_restart();
}

static void maintenance_task(void *parameters)
{
    (void)parameters;
    while (provisioning_state_tick(&provisioning_machine, monotonic_ms())) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Maintenance window closed; disabling provisioning interface");
    esp_err_t stop_result = provisioning_service_stop();
    if (stop_result == ESP_ERR_TIMEOUT) {
        /* Remove network reachability first; retry cleanup without blocking other tasks. */
        ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_disable_softap());
        do {
            ESP_LOGW(TAG, "Provisioning worker still closing; retrying cleanup");
            vTaskDelay(pdMS_TO_TICKS(1000));
            stop_result = provisioning_service_stop();
        } while (stop_result == ESP_ERR_TIMEOUT);
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_disable_softap());
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(stop_result);
    log_stack_margin("maintenance task");
    vTaskDelete(NULL);
}

static void start_operational_hardware(void)
{
    ESP_ERROR_CHECK(app_i2c_bus_init());
    esp_err_t sensor_result = sensor_init();
    if (sensor_result != ESP_OK) {
        ESP_LOGE(TAG, "Sensor initialization failed: %s; telemetry will retry", esp_err_to_name(sensor_result));
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

    esp_err_t machine_result = machine_status_init(&runtime_configuration);
    if (machine_result == ESP_OK) {
        device_health_update_component(
            DEVICE_HEALTH_COMPONENT_MACHINE_STATUS,
            machine_status_is_enabled() ? DEVICE_HEALTH_COMPONENT_HEALTHY : DEVICE_HEALTH_COMPONENT_UNKNOWN,
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
}

void app_main(void)
{
    ESP_LOGI(TAG, "Industrial Edge Monitor firmware started");
    ESP_LOGI(TAG, "Bounded runtime structures: device_config=%u bytes, diagnostic_batch=%u bytes",
        (unsigned)sizeof(device_config_t), (unsigned)sizeof(diagnostic_log_batch_t));
    esp_err_t storage_result = device_config_storage_init();
    if (storage_result != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed closed: %s; only confirmed serial recovery is available",
            esp_err_to_name(storage_result));
        ESP_ERROR_CHECK(diagnostic_log_init());
        ESP_ERROR_CHECK(serial_recovery_start());
        log_stack_margin("recovery-only startup");
        return;
    }

    ESP_ERROR_CHECK(serial_recovery_start());
    bool setup_secret_generated = false;
    esp_err_t setup_secret_result = device_config_get_or_create_setup_secret(
        setup_secret,
        &setup_secret_generated
    );
    if (setup_secret_result != ESP_OK) {
        ESP_LOGE(TAG, "Setup secret is unreadable: %s; serial recovery remains available and no automatic erase or regeneration was attempted",
            esp_err_to_name(setup_secret_result));
        log_stack_margin("setup-secret recovery-only startup");
        return;
    }
    if (setup_secret_generated) {
        /* Deliberate one-time bootstrap output, outside every firmware log sink. */
        printf("IEM first-boot setup code: %s\n", setup_secret);
        printf("Record it now; it will not be displayed again.\n");
        fflush(stdout);
    }
    ESP_ERROR_CHECK(diagnostic_log_init());
    ESP_ERROR_CHECK(device_health_init());
    provisioning_state_init(&provisioning_machine);

    bool using_candidate = false;
    device_config_metadata_t metadata;
    esp_err_t configuration_result = device_config_load_for_boot(
        &runtime_configuration,
        &using_candidate,
        &metadata
    );
    bool configured = configuration_result == ESP_OK;
    if (!configured && configuration_result != ESP_ERR_NVS_NOT_FOUND
        && configuration_result != ESP_ERR_INVALID_ARG && configuration_result != ESP_ERR_INVALID_SIZE) {
        ESP_LOGW(TAG, "Stored configuration unavailable: %s", esp_err_to_name(configuration_result));
    }

    bool expose_provisioning = !configured || using_candidate || runtime_configuration.maintenance_on_boot;
    provisioning_state_configuration_loaded(
        &provisioning_machine,
        configured,
        expose_provisioning,
        monotonic_ms(),
        configured ? runtime_configuration.maintenance_window_seconds : 0,
        configured ? runtime_configuration.maintenance_max_session_seconds : 0
    );
    ESP_ERROR_CHECK(wifi_init_runtime(configured ? &runtime_configuration : NULL, expose_provisioning, setup_secret));
    if (expose_provisioning) {
        ESP_ERROR_CHECK(provisioning_service_start(
            configured ? &runtime_configuration : NULL,
            setup_secret,
            &provisioning_machine
        ));
        if (configured) {
            BaseType_t task_result = xTaskCreate(
                maintenance_task,
                "maintenance_window",
                3072,
                NULL,
                3,
                NULL
            );
            ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
        }
    }

    if (!configured) {
        ESP_LOGW(TAG, "No valid configuration; provisioning-only mode active at http://192.168.4.1");
        log_stack_margin("unprovisioned startup");
        return;
    }

    if (wifi_wait_for_station(30000) != ESP_OK) {
        if (using_candidate) {
            restart_after_candidate_failure("station connection timed out");
        }
        ESP_LOGW(TAG, "Station connection pending; SNTP and MQTT will continue their background retries");
    }

    esp_err_t time_result = system_time_init();
    if (using_candidate && (time_result != ESP_OK || !system_time_is_valid())) {
        restart_after_candidate_failure("SNTP validation timed out");
    }
    if (time_result != ESP_OK && time_result != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "SNTP initialization incomplete: %s", esp_err_to_name(time_result));
    }

    esp_err_t mqtt_result = mqtt_init(&runtime_configuration);
    if (mqtt_result == ESP_OK && using_candidate) {
        mqtt_result = mqtt_wait_connected(30000);
    }
    if (using_candidate && mqtt_result != ESP_OK) {
        restart_after_candidate_failure("authenticated MQTT connection failed");
    }
    if (mqtt_result != ESP_OK) {
        ESP_LOGE(TAG, "Secure MQTT initialization failed: %s", esp_err_to_name(mqtt_result));
        log_stack_margin("degraded operational startup");
        return;
    }

    if (using_candidate) {
        ESP_ERROR_CHECK(device_config_activate_candidate(&runtime_configuration));
        ESP_LOGI(TAG, "Candidate configuration passed Wi-Fi, SNTP and MQTT checks and is now active");
        if (!runtime_configuration.maintenance_on_boot) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(provisioning_service_stop());
            ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_disable_softap());
            expose_provisioning = false;
        }
    }

    start_operational_hardware();
    ESP_ERROR_CHECK(telemetry_start(
        runtime_configuration.device_id,
        runtime_configuration.telemetry_interval_seconds
    ));
    ESP_ERROR_CHECK(device_health_service_start(runtime_configuration.device_id));

    log_stack_margin("operational startup");
}

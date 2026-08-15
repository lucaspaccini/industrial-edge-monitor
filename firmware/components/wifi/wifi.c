#include "wifi.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t wifi_event_group;
static esp_netif_t *station_netif;
static esp_netif_t *access_point_netif;
static atomic_bool station_connected = ATOMIC_VAR_INIT(false);
static atomic_bool softap_enabled = ATOMIC_VAR_INIT(false);
static int retry_count;
static char softap_ssid[33];

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&station_connected, false);
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        if (retry_count < CONFIG_WIFI_MAXIMUM_RETRY) {
            retry_count++;
            ESP_LOGW(TAG, "Station disconnected; retrying (%d/%d)", retry_count, CONFIG_WIFI_MAXIMUM_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            retry_count = 0;
            ESP_LOGW(TAG, "Station initial retry window exhausted; continuing background reconnects");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Station obtained IP " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        atomic_store(&station_connected, true);
        xEventGroupClearBits(wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        atomic_store(&softap_enabled, true);
        ESP_LOGI(TAG, "Provisioning SoftAP started at http://192.168.4.1");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STOP) {
        atomic_store(&softap_enabled, false);
        ESP_LOGI(TAG, "Provisioning SoftAP stopped; station lifecycle is unchanged");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        ESP_LOGI(TAG, "A local provisioning client joined the SoftAP");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        ESP_LOGI(TAG, "A local provisioning client left the SoftAP");
    }
}

static esp_err_t configure_softap(const char *setup_secret)
{
    if (setup_secret == NULL || strlen(setup_secret) < 8) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mac[6];
    ESP_RETURN_ON_ERROR(esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP), TAG, "SoftAP MAC read failed");
    snprintf(softap_ssid, sizeof(softap_ssid), "IEM-Setup-%02X%02X%02X", mac[3], mac[4], mac[5]);
    wifi_config_t access_point = {0};
    size_t softap_ssid_length = strnlen(softap_ssid, sizeof(access_point.ap.ssid));
    size_t setup_secret_length = strnlen(setup_secret, sizeof(access_point.ap.password));
    memcpy(access_point.ap.ssid, softap_ssid, softap_ssid_length);
    memcpy(access_point.ap.password, setup_secret, setup_secret_length);
    access_point.ap.ssid_len = softap_ssid_length;
    access_point.ap.channel = 1;
    access_point.ap.max_connection = 2;
    access_point.ap.authmode = WIFI_AUTH_WPA2_PSK;
    access_point.ap.pmf_cfg.required = true;
    return esp_wifi_set_config(WIFI_IF_AP, &access_point);
}

esp_err_t wifi_init_runtime(
    const device_config_t *configuration,
    bool enable_softap,
    const char *setup_secret
)
{
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "network interface initialization failed");
    esp_err_t loop_result = esp_event_loop_create_default();
    if (loop_result != ESP_OK && loop_result != ESP_ERR_INVALID_STATE) {
        return loop_result;
    }
    if (configuration != NULL) {
        station_netif = esp_netif_create_default_wifi_sta();
    }
    if (enable_softap) {
        access_point_netif = esp_netif_create_default_wifi_ap();
    }
    wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&initialization), TAG, "Wi-Fi driver initialization failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Wi-Fi RAM storage selection failed");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL),
        TAG,
        "Wi-Fi event registration failed"
    );
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL),
        TAG,
        "IP event registration failed"
    );
    wifi_mode_t mode = configuration == NULL
        ? WIFI_MODE_AP
        : (enable_softap ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(mode), TAG, "Wi-Fi mode selection failed");
    if (configuration != NULL) {
        wifi_config_t station = {0};
        size_t ssid_length = strnlen(configuration->wifi_ssid, sizeof(station.sta.ssid));
        size_t password_length = strnlen(configuration->wifi_password, sizeof(station.sta.password));
        memcpy(station.sta.ssid, configuration->wifi_ssid, ssid_length);
        memcpy(station.sta.password, configuration->wifi_password, password_length);
        station.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        station.sta.pmf_cfg.capable = true;
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &station), TAG, "station configuration failed");
    }
    if (enable_softap) {
        ESP_RETURN_ON_ERROR(configure_softap(setup_secret), TAG, "SoftAP configuration failed");
    }
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi driver start failed");
    ESP_LOGI(TAG, "Wi-Fi driver started in %s mode", mode == WIFI_MODE_AP ? "AP" : mode == WIFI_MODE_APSTA ? "APSTA" : "STA");
    return ESP_OK;
}

esp_err_t wifi_wait_for_station(uint32_t timeout_ms)
{
    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms)
    );
    return (bits & WIFI_CONNECTED_BIT) != 0 ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t wifi_disable_softap(void)
{
    if (!atomic_load(&softap_enabled)) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Closing provisioning SoftAP without stopping Wi-Fi station");
    return esp_wifi_set_mode(WIFI_MODE_STA);
}

bool wifi_station_is_connected(void)
{
    return atomic_load(&station_connected);
}

bool wifi_softap_is_enabled(void)
{
    return atomic_load(&softap_enabled);
}

esp_err_t wifi_get_station_ip(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size < 16 || station_netif == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_netif_ip_info_t information;
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(station_netif, &information), TAG, "station IP lookup failed");
    snprintf(buffer, buffer_size, IPSTR, IP2STR(&information.ip));
    return information.ip.addr != 0 ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t wifi_get_station_rssi(int *rssi)
{
    if (rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!atomic_load(&station_connected)) {
        return ESP_ERR_INVALID_STATE;
    }
    wifi_ap_record_t record;
    esp_err_t error = esp_wifi_sta_get_ap_info(&record);
    if (error != ESP_OK) {
        return error;
    }
    *rssi = record.rssi;
    return ESP_OK;
}

esp_err_t wifi_get_softap_ipv4(uint32_t *address, uint32_t *netmask)
{
    if (address == NULL || netmask == NULL || access_point_netif == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_netif_ip_info_t information = {0};
    ESP_RETURN_ON_ERROR(
        esp_netif_get_ip_info(access_point_netif, &information),
        TAG,
        "SoftAP IP lookup failed"
    );
    if (information.ip.addr == 0 || information.netmask.addr == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    *address = information.ip.addr;
    *netmask = information.netmask.addr;
    return ESP_OK;
}

const char *wifi_get_softap_ssid(void)
{
    return softap_ssid;
}

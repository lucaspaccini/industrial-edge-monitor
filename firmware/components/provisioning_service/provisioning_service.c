#include "provisioning_service.h"
#include "provisioning_security.h"
#include "provisioning_softap_endpoint.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "config.h"
#include "device_health.h"
#include "diagnostic_log.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "machine_status.h"
#include "mqtt_client_app.h"
#include "system_time.h"
#include "wifi.h"

static const char *TAG = "provisioning_http";
static httpd_handle_t server;
static const device_config_t *current_configuration;
static const char *setup_secret_value;
static provisioning_state_machine_t *machine;
static char session_token[33];
static char csrf_token[33];
static int64_t session_expires_ms;
static uint64_t session_generation;
static uint32_t login_failures;
static int64_t login_locked_until_ms;
static portMUX_TYPE service_state_lock = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool service_stop_in_progress = ATOMIC_VAR_INIT(false);
static StaticSemaphore_t stream_mutex_storage;
static SemaphoreHandle_t stream_mutex;
static provisioning_stream_lifecycle_t stream_lifecycle;

typedef struct {
    httpd_req_t *request;
    uint64_t session_generation;
    uint64_t worker_generation;
    provisioning_async_lifecycle_t lifecycle;
} diagnostic_stream_context_t;

static const char PAGE[] =
    "<!doctype html><html lang=en><meta charset=utf-8><meta name=viewport content='width=device-width'>"
    "<title>Industrial Edge Monitor provisioning</title><style>body{font:16px system-ui;max-width:900px;margin:auto;padding:1rem}"
    "input,textarea,button{font:inherit;margin:.25rem;padding:.5rem}textarea{width:100%;height:18rem}pre{background:#111;color:#eee;padding:1rem;overflow:auto}</style>"
    "<h1>Device provisioning</h1><p>Connect only through the WPA2 SoftAP. HTTP does not provide application-layer encryption.</p>"
    "<section><h2>Login</h2><input id=s type=password autocomplete=current-password><button onclick=login()>Login</button><button onclick=logout()>Logout</button></section>"
    "<section><h2>Status</h2><button onclick=get('/api/status')>Refresh</button><pre id=o></pre></section>"
    "<section><h2>Configuration</h2><p>Load an MQTT package, then add Wi-Fi and review the non-sensitive defaults in the JSON. Existing passwords are never returned.</p>"
    "<label>MQTT package <input id=p type=file accept='application/json,.json' onchange=loadPackage(this)></label>"
    "<label>Wi-Fi password <input id=wp type=password autocomplete=new-password></label><label>New MQTT password <input id=mp type=password autocomplete=new-password></label>"
    "<textarea id=c aria-label='Complete candidate configuration'></textarea><button onclick=save()>Save candidate</button><button onclick=post('/api/config/apply')>Apply and reboot</button>"
    "<button onclick=del('/api/config/candidate')>Cancel</button><pre id=ce aria-live=polite></pre></section>"
    "<section><h2>Diagnostics</h2><label>Level <select id=lf><option value=''>all</option><option>E</option><option>W</option><option>I</option><option>D</option></select></label>"
    "<label>Component <input id=cf></label><button onclick=loadLogs()>Load logs</button><button onclick=live()>Start live</button><button onclick=closeLive()>Stop live</button> <span id=ls>offline</span><pre id=l></pre></section>"
    "<section><h2>Recovery</h2><button onclick=reboot()>Reboot</button><button onclick=factoryReset()>Factory reset</button><p>Factory reset erases active and candidate configuration and generates a new setup secret.</p></section>"
    "<script>let csrf='',streamGeneration='',liveSource=null,logRecords=[],logCursor=0,logLost=0,logOverwritten=0;function clearConfigurationInputs(){c.value='';p.value='';wp.value='';mp.value=''}async function login(){closeLive();clearConfigurationInputs();csrf='';streamGeneration='';try{let r=await fetch('/api/session',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({secret:s.value})});let j=await r.json();if(r.ok){csrf=j.csrf_token||'';streamGeneration=typeof j.stream_generation==='string'?j.stream_generation:'';o.textContent='Login successful';await loadCurrent()}else{o.textContent=JSON.stringify({ok:false,error:j.error||'login failed'},null,2)}}finally{s.value=''}}"
    "async function req(u,m='GET',b,out=o){let r=await fetch(u,{method:m,headers:{'Content-Type':'application/json','X-CSRF-Token':csrf},body:b,credentials:'same-origin'});out.textContent=await r.text();return r.ok}"
    "function get(u){req(u)}function post(u){req(u,'POST','{}',ce)}function del(u){req(u,'DELETE',undefined,ce)}async function save(){try{let q=JSON.parse(c.value);if(wp.value)q.wifi_password=wp.value;if(mp.value){if(q.mqtt)q.mqtt.password=mp.value;else q.mqtt_password=mp.value}await req('/api/config/candidate','PUT',JSON.stringify(q),ce)}catch(e){ce.textContent='Invalid configuration JSON: '+e.message}finally{wp.value='';mp.value=''}}"
    "async function loadCurrent(){try{let r=await fetch('/api/config',{credentials:'same-origin'});let q=await r.json();if(r.ok&&q.state!=='unprovisioned'){delete q.wifi_password_configured;delete q.mqtt_password_configured;delete q.mqtt_ca_certificate_configured;c.value=JSON.stringify(q,null,2)}else{clearConfigurationInputs();ce.textContent=JSON.stringify(q,null,2)}}catch(e){clearConfigurationInputs();ce.textContent='Configuration unavailable'}}"
    "async function loadPackage(i){let f=i.files[0];clearConfigurationInputs();try{if(!f)return;let q=JSON.parse(await f.text());mp.value=q.mqtt.password||'';delete q.mqtt.password;c.value=JSON.stringify({schema_version:q.schema_version,device_id:q.device_id,wifi_ssid:'',mqtt:q.mqtt,telemetry_interval_seconds:5,machine_status:{provider:'gpio',gpio:27,active_level:'high',pull:'none'},maintenance:{on_boot:true,window_seconds:300,max_session_seconds:900}},null,2);ce.textContent='Package loaded. Password is held only in the separate masked field. Enter Wi-Fi and review every field.'}catch(e){clearConfigurationInputs();ce.textContent='Invalid provisioning package: '+e.message}finally{i.value=''}}"
    "function reboot(){if(confirm('Reboot the device now?'))post('/api/reboot')}async function factoryReset(){let x=prompt('Type ERASE-DEVICE-CONFIGURATION to erase all device configuration');if(x!=='ERASE-DEVICE-CONFIGURATION'){ce.textContent='Factory reset cancelled.';return}let r=await fetch('/api/factory-reset',{method:'POST',headers:{'Content-Type':'application/json','X-CSRF-Token':csrf,'X-Confirm-Factory-Reset':x},body:'{}',credentials:'same-origin'});ce.textContent=await r.text();if(r.ok)clearConfigurationInputs()}"
    "function show(){let a=logRecords.filter(x=>(!lf.value||x.level===lf.value)&&(!cf.value||x.component.includes(cf.value)));l.textContent=a.map(x=>x.sequence+' '+x.relative_ms+' '+x.level+' '+x.component+': '+x.message).join('\\n')+'\\n[overwritten: '+logOverwritten+', lost before cursor: '+logLost+']'}"
    "function acceptBatch(j){logRecords.push(...(j.records||[]));if(logRecords.length>256)logRecords=logRecords.slice(-256);logCursor=j.cursor||logCursor;logLost+=j.lost_before_cursor||0;logOverwritten=j.overwritten||0;show()}"
    "async function loadLogs(){logRecords=[];logCursor=0;logLost=0;let more=true;while(more){let r=await fetch('/api/logs?after='+logCursor,{credentials:'same-origin'});if(!r.ok){l.textContent=await r.text();return}let j=await r.json();acceptBatch(j);more=!!j.has_more}}"
    "function live(){closeLive();if(!streamGeneration){ls.textContent='login required';return}liveSource=new EventSource('/api/logs/stream?after='+logCursor+'&generation='+encodeURIComponent(streamGeneration));ls.textContent='connecting';liveSource.onopen=()=>ls.textContent='live';liveSource.onmessage=x=>acceptBatch(JSON.parse(x.data));liveSource.onerror=()=>ls.textContent='connection lost / retrying'}function closeLive(){if(liveSource){liveSource.close();liveSource=null}ls.textContent='offline'}async function logout(){closeLive();clearConfigurationInputs();await req('/api/session','DELETE',undefined,o);csrf='';streamGeneration='';s.value='';logRecords=[];logCursor=0;show()}</script></html>";

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void random_hex(char output[33])
{
    uint8_t random[16];
    esp_fill_random(random, sizeof(random));
    for (size_t index = 0; index < sizeof(random); index++) {
        snprintf(output + index * 2, 3, "%02x", random[index]);
    }
    volatile uint8_t *cursor = random;
    for (size_t index = 0; index < sizeof(random); index++) cursor[index] = 0;
}

static void secure_clear(void *value, size_t size)
{
    volatile uint8_t *bytes = value;
    while (bytes != NULL && size-- > 0) *bytes++ = 0;
}

static bool stream_lock(void)
{
    return stream_mutex != NULL
        && xSemaphoreTake(stream_mutex, portMAX_DELAY) == pdTRUE;
}

static void stream_unlock(void)
{
    xSemaphoreGive(stream_mutex);
}

static bool service_is_stopping(void)
{
    if (!stream_lock()) return true;
    bool stopping = stream_lifecycle.service_stopping;
    stream_unlock();
    return stopping;
}

static void session_snapshot(
    char token[33],
    char csrf[33],
    int64_t *expires_ms,
    uint64_t *generation
)
{
    taskENTER_CRITICAL(&service_state_lock);
    memcpy(token, session_token, sizeof(session_token));
    memcpy(csrf, csrf_token, sizeof(csrf_token));
    *expires_ms = session_expires_ms;
    *generation = session_generation;
    taskEXIT_CRITICAL(&service_state_lock);
}

static void session_clear(void)
{
    taskENTER_CRITICAL(&service_state_lock);
    memset(session_token, 0, sizeof(session_token));
    memset(csrf_token, 0, sizeof(csrf_token));
    session_expires_ms = 0;
    session_generation++;
    if (session_generation == 0) session_generation = 1;
    taskEXIT_CRITICAL(&service_state_lock);
}

static bool stream_session_is_valid(uint64_t stream_generation, uint64_t worker_generation)
{
    int64_t expires_ms;
    uint64_t current_generation;
    taskENTER_CRITICAL(&service_state_lock);
    expires_ms = session_expires_ms;
    current_generation = session_generation;
    taskEXIT_CRITICAL(&service_state_lock);
    if (!stream_lock()) return false;
    bool worker_current = provisioning_stream_worker_current(
        &stream_lifecycle,
        worker_generation
    );
    stream_unlock();
    return worker_current && provisioning_security_stream_session_valid(
        stream_generation,
        current_generation,
        expires_ms,
        now_ms(),
        false,
        false
    );
}

static uint64_t request_stream_shutdown(bool stopping_service)
{
    if (!stream_lock()) return 0;
    int socket_fd = -1;
    uint64_t worker_generation = provisioning_stream_request_shutdown(
        &stream_lifecycle,
        stopping_service,
        &socket_fd
    );
    httpd_handle_t handle = server;
    if (handle != NULL && socket_fd >= 0) {
        httpd_sess_trigger_close(handle, socket_fd);
    }
    stream_unlock();
    return worker_generation;
}

static bool request_arrived_on_softap(httpd_req_t *request)
{
    uint32_t softap_ipv4 = 0;
    uint32_t softap_netmask = 0;
    bool netif_ready = wifi_get_softap_ipv4(&softap_ipv4, &softap_netmask) == ESP_OK;
    int family = AF_UNSPEC;
    int socket_fd = request != NULL ? httpd_req_to_sockfd(request) : -1;
    provisioning_endpoint_class_t classification = provisioning_softap_authorize_socket(
        socket_fd,
        netif_ready,
        softap_ipv4,
        softap_netmask,
        &family
    );
    bool authorized = provisioning_softap_endpoint_is_authorized(classification);
    if (!authorized) {
        ESP_LOGW(TAG, "Provisioning request rejected: socket_family=%d local_endpoint=%s reason=softap-only-policy",
            family, provisioning_softap_endpoint_class_name(classification));
    }
    return authorized;
}

static esp_err_t json_error(httpd_req_t *request, const char *status, const char *message)
{
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    cJSON *object = cJSON_CreateObject();
    cJSON_AddFalseToObject(object, "ok");
    cJSON_AddStringToObject(object, "error", message);
    char *body = cJSON_PrintUnformatted(object);
    esp_err_t result = httpd_resp_sendstr(request, body != NULL ? body : "{\"ok\":false}");
    cJSON_free(body);
    cJSON_Delete(object);
    return result;
}

static esp_err_t send_json(httpd_req_t *request, cJSON *object)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    char *body = cJSON_PrintUnformatted(object);
    if (body == NULL) {
        return json_error(request, "500 Internal Server Error", "response allocation failed");
    }
    esp_err_t result = httpd_resp_sendstr(request, body);
    secure_clear(body, strlen(body));
    cJSON_free(body);
    return result;
}

static esp_err_t send_json_text(httpd_req_t *request, const char *body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, body);
}

static esp_err_t receive_json(httpd_req_t *request, cJSON **output)
{
    if (output == NULL) return ESP_ERR_INVALID_ARG;
    *output = NULL;
    char content_type[64];
    bool has_content_type = httpd_req_get_hdr_value_str(
        request, "Content-Type", content_type, sizeof(content_type)
    ) == ESP_OK;
    if (!has_content_type || !provisioning_security_json_body_allowed(content_type, 1, 1)) {
        return json_error(request, "415 Unsupported Media Type", "Content-Type must be application/json");
    }
    if (!provisioning_security_json_body_allowed(
            content_type,
            request->content_len > 0 ? (size_t)request->content_len : 0,
            CONFIG_PROVISIONING_MAX_BODY_BYTES
        )) {
        return json_error(request, "413 Content Too Large", "request body size is invalid");
    }
    char *body = calloc(1, request->content_len + 1);
    if (body == NULL) {
        return json_error(request, "503 Service Unavailable", "request buffer unavailable");
    }
    size_t received = 0;
    while (received < request->content_len) {
        int result = httpd_req_recv(request, body + received, request->content_len - received);
        if (result <= 0) {
            secure_clear(body, request->content_len + 1);
            free(body);
            return ESP_FAIL;
        }
        received += (size_t)result;
    }
    *output = cJSON_ParseWithLength(body, received);
    secure_clear(body, request->content_len + 1);
    free(body);
    if (*output == NULL || !cJSON_IsObject(*output)) {
        cJSON_Delete(*output);
        *output = NULL;
        return json_error(request, "400 Bad Request", "request body is not a JSON object");
    }
    return ESP_OK;
}

static void clear_json_string(cJSON *object, const char *name)
{
    cJSON *value = cJSON_IsObject(object)
        ? cJSON_GetObjectItemCaseSensitive(object, name)
        : NULL;
    if (cJSON_IsString(value) && value->valuestring != NULL) {
        secure_clear(value->valuestring, strlen(value->valuestring));
    }
}

static void clear_candidate_json_secrets(cJSON *candidate)
{
    if (!cJSON_IsObject(candidate)) return;
    clear_json_string(candidate, "wifi_password");
    clear_json_string(candidate, "mqtt_password");
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(candidate, "mqtt");
    clear_json_string(mqtt, "password");
}

static bool cookie_token(httpd_req_t *request, char output[33])
{
    char cookie[160] = {0};
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookie, sizeof(cookie)) != ESP_OK) {
        secure_clear(cookie, sizeof(cookie));
        return false;
    }
    const char *start = strstr(cookie, "IEM_SESSION=");
    if (start == NULL) {
        secure_clear(cookie, sizeof(cookie));
        return false;
    }
    start += strlen("IEM_SESSION=");
    size_t length = strcspn(start, "; ");
    if (length != 32) {
        secure_clear(cookie, sizeof(cookie));
        return false;
    }
    memcpy(output, start, length);
    output[length] = '\0';
    secure_clear(cookie, sizeof(cookie));
    return true;
}

static bool authenticated(httpd_req_t *request, bool modifying, uint64_t *authorized_generation)
{
    if (service_is_stopping() || !request_arrived_on_softap(request)) {
        return false;
    }
    char cookie[33];
    if (!cookie_token(request, cookie)) {
        return false;
    }
    char csrf[40] = {0};
    if (modifying) {
        if (httpd_req_get_hdr_value_str(request, "X-CSRF-Token", csrf, sizeof(csrf)) != ESP_OK) {
            secure_clear(cookie, sizeof(cookie));
            secure_clear(csrf, sizeof(csrf));
            return false;
        }
    }
    char expected_token[33];
    char expected_csrf[33];
    int64_t expires_ms;
    uint64_t generation;
    session_snapshot(expected_token, expected_csrf, &expires_ms, &generation);
    bool authorized = provisioning_security_session_authorized(
            expected_token, cookie, expected_csrf, modifying ? csrf : NULL,
            expires_ms, now_ms(), modifying
        );
    secure_clear(cookie, sizeof(cookie));
    secure_clear(csrf, sizeof(csrf));
    secure_clear(expected_token, sizeof(expected_token));
    secure_clear(expected_csrf, sizeof(expected_csrf));
    if (!authorized) return false;
    if (authorized_generation != NULL) *authorized_generation = generation;
    if (machine != NULL) {
        provisioning_state_authenticated_activity(machine, (uint64_t)now_ms());
    }
    return true;
}

static esp_err_t require_auth(httpd_req_t *request, bool modifying)
{
    return authenticated(request, modifying, NULL)
        ? ESP_OK
        : json_error(request, "401 Unauthorized", "authentication or CSRF validation failed");
}

static esp_err_t root_handler(httpd_req_t *request)
{
    if (!request_arrived_on_softap(request)) {
        return json_error(request, "403 Forbidden", "provisioning is available only through the SoftAP");
    }
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t session_create_handler(httpd_req_t *request)
{
    if (!request_arrived_on_softap(request)) {
        return json_error(request, "403 Forbidden", "login is available only through the SoftAP");
    }
    int64_t locked_until;
    taskENTER_CRITICAL(&service_state_lock);
    locked_until = login_locked_until_ms;
    taskEXIT_CRITICAL(&service_state_lock);
    if (now_ms() < locked_until) {
        return json_error(request, "429 Too Many Requests", "login temporarily locked");
    }
    cJSON *body = NULL;
    if (receive_json(request, &body) != ESP_OK) {
        return ESP_OK;
    }
    cJSON *secret = cJSON_GetObjectItemCaseSensitive(body, "secret");
    bool valid = cJSON_IsString(secret)
        && provisioning_security_constant_time_equal(secret->valuestring, setup_secret_value);
    if (cJSON_IsString(secret)) secure_clear(secret->valuestring, strlen(secret->valuestring));
    cJSON_Delete(body);
    if (!valid) {
        taskENTER_CRITICAL(&service_state_lock);
        login_failures++;
        if (login_failures >= CONFIG_PROVISIONING_LOGIN_MAX_FAILURES) {
            login_locked_until_ms = now_ms() + CONFIG_PROVISIONING_LOGIN_LOCKOUT_SECONDS * 1000LL;
            login_failures = 0;
        }
        taskEXIT_CRITICAL(&service_state_lock);
        return json_error(request, "401 Unauthorized", "invalid credentials");
    }
    char new_session[33];
    char new_csrf[33];
    uint64_t new_generation;
    random_hex(new_session);
    random_hex(new_csrf);
    taskENTER_CRITICAL(&service_state_lock);
    login_failures = 0;
    session_generation++;
    if (session_generation == 0) session_generation = 1;
    new_generation = session_generation;
    memcpy(session_token, new_session, sizeof(session_token));
    memcpy(csrf_token, new_csrf, sizeof(csrf_token));
    session_expires_ms = now_ms() + CONFIG_PROVISIONING_SESSION_SECONDS * 1000LL;
    taskEXIT_CRITICAL(&service_state_lock);
    request_stream_shutdown(false);
    char cookie[128];
    char generation_text[21];
    snprintf(cookie, sizeof(cookie), "IEM_SESSION=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%d", new_session, CONFIG_PROVISIONING_SESSION_SECONDS);
    snprintf(generation_text, sizeof(generation_text), "%" PRIu64, new_generation);
    httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    cJSON *response = cJSON_CreateObject();
    cJSON_AddTrueToObject(response, "ok");
    cJSON_AddStringToObject(response, "csrf_token", new_csrf);
    cJSON_AddStringToObject(response, "stream_generation", generation_text);
    esp_err_t result = send_json(request, response);
    cJSON *response_csrf = cJSON_GetObjectItemCaseSensitive(response, "csrf_token");
    if (cJSON_IsString(response_csrf)) {
        secure_clear(response_csrf->valuestring, strlen(response_csrf->valuestring));
    }
    cJSON_Delete(response);
    secure_clear(cookie, sizeof(cookie));
    secure_clear(generation_text, sizeof(generation_text));
    secure_clear(new_session, sizeof(new_session));
    secure_clear(new_csrf, sizeof(new_csrf));
    if (machine != NULL) {
        provisioning_state_authenticated_activity(machine, (uint64_t)now_ms());
    }
    return result;
}

static esp_err_t session_delete_handler(httpd_req_t *request)
{
    if (require_auth(request, true) != ESP_OK) {
        return ESP_OK;
    }
    session_clear();
    request_stream_shutdown(false);
    httpd_resp_set_hdr(request, "Set-Cookie", "IEM_SESSION=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    return send_json_text(request, "{\"ok\":true}");
}

static const char *configuration_state_name(device_config_state_t state)
{
    switch (state) {
        case DEVICE_CONFIG_STATE_ACTIVE: return "active";
        case DEVICE_CONFIG_STATE_PENDING: return "pending";
        case DEVICE_CONFIG_STATE_ROLLBACK: return "rollback";
        default: return "none";
    }
}

static esp_err_t status_handler(httpd_req_t *request)
{
    if (require_auth(request, false) != ESP_OK) {
        return ESP_OK;
    }
    device_config_metadata_t metadata = {0};
    esp_err_t metadata_result = device_config_get_metadata(&metadata);
    if (metadata_result != ESP_OK) {
        return json_error(request, "503 Service Unavailable", "configuration metadata is unavailable");
    }
    device_health_snapshot_t health = {0};
    device_health_get_snapshot(&health);
    char station_ip[16] = "unavailable";
    bool station_connected = wifi_station_is_connected();
    if (station_connected) wifi_get_station_ip(station_ip, sizeof(station_ip));
    int rssi = 0;
    bool has_rssi = station_connected && wifi_get_station_rssi(&rssi) == ESP_OK;
    machine_status_t machine_status = MACHINE_STATUS_UNKNOWN;
    machine_status_get(&machine_status);
    const esp_app_desc_t *application = esp_app_get_description();
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "device_id", current_configuration != NULL ? current_configuration->device_id : "unprovisioned");
    cJSON_AddStringToObject(response, "firmware_version", application->version);
    cJSON_AddNumberToObject(response, "uptime_seconds", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(response, "reset_reason", esp_reset_reason());
    cJSON_AddNumberToObject(response, "free_heap_bytes", esp_get_free_heap_size());
    cJSON_AddNumberToObject(response, "minimum_free_heap_bytes", esp_get_minimum_free_heap_size());
    cJSON_AddStringToObject(response, "configuration_state", configuration_state_name(metadata.state));
    cJSON_AddNumberToObject(response, "candidate_revision", metadata.candidate_revision);
    cJSON_AddNumberToObject(response, "boot_attempts", metadata.boot_attempts);
    cJSON_AddNumberToObject(response, "boot_count", metadata.boot_count);
    cJSON_AddStringToObject(response, "station_ip", station_ip);
    cJSON_AddStringToObject(response, "wifi_ssid", current_configuration != NULL ? current_configuration->wifi_ssid : "");
    if (has_rssi) cJSON_AddNumberToObject(response, "wifi_rssi_dbm", rssi); else cJSON_AddNullToObject(response, "wifi_rssi_dbm");
    cJSON_AddBoolToObject(response, "sntp_synchronized", system_time_is_valid());
    cJSON_AddBoolToObject(response, "mqtt_tls_connected", mqtt_is_connected());
    char mqtt_error[96] = {0};
    mqtt_copy_last_error(mqtt_error, sizeof(mqtt_error));
    cJSON_AddStringToObject(response, "mqtt_last_error", mqtt_error);
    cJSON_AddStringToObject(response, "sensor_status", device_health_component_status_to_string(health.components[DEVICE_HEALTH_COMPONENT_SENSOR].status));
    cJSON_AddStringToObject(response, "machine_status", machine_status_to_string(machine_status));
    cJSON_AddStringToObject(response, "device_health", device_health_overall_status_to_string(health.status));
    cJSON *counters = cJSON_AddObjectToObject(response, "telemetry_counters");
    cJSON_AddNumberToObject(counters, "samples_ok", health.counters.samples_ok);
    cJSON_AddNumberToObject(counters, "samples_rejected", health.counters.samples_rejected);
    cJSON_AddNumberToObject(counters, "publish_ok", health.counters.publish_ok);
    cJSON_AddNumberToObject(counters, "publish_failed", health.counters.publish_failed);
    cJSON_AddStringToObject(response, "last_rollback_reason", metadata.last_rollback_reason);
    esp_err_t result = send_json(request, response);
    cJSON_Delete(response);
    return result;
}

static esp_err_t config_handler(httpd_req_t *request)
{
    if (require_auth(request, false) != ESP_OK) {
        return ESP_OK;
    }
    cJSON *response = cJSON_CreateObject();
    if (current_configuration == NULL) {
        cJSON_AddStringToObject(response, "state", "unprovisioned");
    } else {
        device_config_redacted_t redacted;
        device_config_get_redacted_snapshot(current_configuration, &redacted);
        cJSON_AddNumberToObject(response, "schema_version", redacted.schema_version);
        cJSON_AddNumberToObject(response, "revision", redacted.revision);
        cJSON_AddStringToObject(response, "device_id", redacted.device_id);
        cJSON_AddStringToObject(response, "wifi_ssid", redacted.wifi_ssid);
        cJSON_AddBoolToObject(response, "wifi_password_configured", redacted.wifi_password_configured);
        cJSON_AddStringToObject(response, "mqtt_broker_uri", redacted.mqtt_broker_uri);
        cJSON_AddStringToObject(response, "mqtt_username", redacted.mqtt_username);
        cJSON_AddBoolToObject(response, "mqtt_password_configured", redacted.mqtt_password_configured);
        cJSON_AddStringToObject(response, "mqtt_client_id", redacted.mqtt_client_id);
        cJSON_AddBoolToObject(response, "mqtt_ca_certificate_configured", redacted.mqtt_ca_certificate_configured);
        cJSON_AddNumberToObject(response, "telemetry_interval_seconds", redacted.telemetry_interval_seconds);
        cJSON_AddStringToObject(response, "machine_status_provider",
            redacted.machine_status_provider == DEVICE_CONFIG_MACHINE_GPIO ? "gpio" : "disabled");
        cJSON_AddNumberToObject(response, "machine_status_gpio", redacted.machine_status_gpio);
        cJSON_AddStringToObject(response, "machine_status_active_level",
            redacted.machine_status_active_high ? "high" : "low");
        const char *pull = "none";
        if (redacted.machine_status_pull == DEVICE_CONFIG_PULL_UP) pull = "up";
        else if (redacted.machine_status_pull == DEVICE_CONFIG_PULL_DOWN) pull = "down";
        cJSON_AddStringToObject(response, "machine_status_pull", pull);
        cJSON_AddBoolToObject(response, "maintenance_on_boot", redacted.maintenance_on_boot);
        cJSON_AddNumberToObject(response, "maintenance_window_seconds", redacted.maintenance_window_seconds);
        cJSON_AddNumberToObject(response, "maintenance_max_session_seconds", redacted.maintenance_max_session_seconds);
    }
    esp_err_t result = send_json(request, response);
    cJSON_Delete(response);
    return result;
}

static bool copy_json_string(cJSON *object, const char *name, char *target, size_t size, bool required)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (item == NULL && !required) return true;
    if (!cJSON_IsString(item) || item->valuestring[0] == '\0') return false;
    int written = snprintf(target, size, "%s", item->valuestring);
    return written >= 0 && (size_t)written < size;
}

static bool json_uint(cJSON *object, const char *name, uint32_t *target, bool required)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (item == NULL && !required) return true;
    if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > UINT32_MAX
        || item->valuedouble != (double)(uint32_t)item->valuedouble) return false;
    *target = (uint32_t)item->valuedouble;
    return true;
}

static esp_err_t parse_configuration(cJSON *root, device_config_t *configuration, char *error, size_t error_size)
{
    bool has_active = current_configuration != NULL;
    if (has_active) {
        *configuration = *current_configuration;
        configuration->revision++;
    } else {
        memset(configuration, 0, sizeof(*configuration));
        configuration->revision = 1;
        configuration->machine_status_gpio = 27;
        configuration->machine_status_pull = DEVICE_CONFIG_PULL_NONE;
    }
    uint32_t schema_version = 0;
    if (!json_uint(root, "schema_version", &schema_version, true)
        || schema_version != DEVICE_CONFIG_SCHEMA_VERSION) {
        snprintf(error, error_size, "schema_version is missing or unsupported");
        return ESP_ERR_INVALID_ARG;
    }
    configuration->schema_version = schema_version;
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(root, "mqtt");
    cJSON *source = cJSON_IsObject(mqtt) ? mqtt : root;
    if (!copy_json_string(root, "device_id", configuration->device_id, sizeof(configuration->device_id), true)
        || !copy_json_string(root, "wifi_ssid", configuration->wifi_ssid, sizeof(configuration->wifi_ssid), true)
        || !copy_json_string(root, "wifi_password", configuration->wifi_password, sizeof(configuration->wifi_password), !has_active)
        || !copy_json_string(source, cJSON_IsObject(mqtt) ? "broker_uri" : "mqtt_broker_uri", configuration->mqtt_broker_uri, sizeof(configuration->mqtt_broker_uri), true)
        || !copy_json_string(source, cJSON_IsObject(mqtt) ? "username" : "mqtt_username", configuration->mqtt_username, sizeof(configuration->mqtt_username), true)
        || !copy_json_string(source, cJSON_IsObject(mqtt) ? "password" : "mqtt_password", configuration->mqtt_password, sizeof(configuration->mqtt_password), !has_active)
        || !copy_json_string(source, cJSON_IsObject(mqtt) ? "client_id" : "mqtt_client_id", configuration->mqtt_client_id, sizeof(configuration->mqtt_client_id), true)
        || !copy_json_string(source, cJSON_IsObject(mqtt) ? "ca_certificate" : "mqtt_ca_certificate", configuration->mqtt_ca_certificate, sizeof(configuration->mqtt_ca_certificate), !has_active)
        || !json_uint(root, "telemetry_interval_seconds", &configuration->telemetry_interval_seconds, true)) {
        snprintf(error, error_size, "one or more required fields have invalid type, value or length");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *maintenance = cJSON_GetObjectItemCaseSensitive(root, "maintenance");
    cJSON *maintenance_source = cJSON_IsObject(maintenance) ? maintenance : root;
    cJSON *maintenance_enabled = cJSON_GetObjectItemCaseSensitive(maintenance_source, cJSON_IsObject(maintenance) ? "on_boot" : "maintenance_on_boot");
    if (!cJSON_IsBool(maintenance_enabled)) {
        snprintf(error, error_size, "maintenance_on_boot must be boolean");
        return ESP_ERR_INVALID_ARG;
    }
    configuration->maintenance_on_boot = cJSON_IsTrue(maintenance_enabled);
    if (!json_uint(maintenance_source, cJSON_IsObject(maintenance) ? "window_seconds" : "maintenance_window_seconds", &configuration->maintenance_window_seconds, true)
        || !json_uint(maintenance_source, cJSON_IsObject(maintenance) ? "max_session_seconds" : "maintenance_max_session_seconds", &configuration->maintenance_max_session_seconds, true)) {
        snprintf(error, error_size, "maintenance timeouts must be integers");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON *machine_object = cJSON_GetObjectItemCaseSensitive(root, "machine_status");
    cJSON *machine_source = cJSON_IsObject(machine_object) ? machine_object : root;
    cJSON *provider = cJSON_GetObjectItemCaseSensitive(machine_source, cJSON_IsObject(machine_object) ? "provider" : "machine_status_provider");
    if (!cJSON_IsString(provider) || (strcmp(provider->valuestring, "disabled") != 0 && strcmp(provider->valuestring, "gpio") != 0)) {
        snprintf(error, error_size, "machine_status_provider must be disabled or gpio");
        return ESP_ERR_INVALID_ARG;
    }
    configuration->machine_status_provider = strcmp(provider->valuestring, "gpio") == 0
        ? DEVICE_CONFIG_MACHINE_GPIO : DEVICE_CONFIG_MACHINE_DISABLED;
    uint32_t gpio = (uint32_t)configuration->machine_status_gpio;
    if (!json_uint(
            machine_source,
            cJSON_IsObject(machine_object) ? "gpio" : "machine_status_gpio",
            &gpio,
            configuration->machine_status_provider == DEVICE_CONFIG_MACHINE_GPIO
        )) {
        snprintf(error, error_size, "machine_status_gpio must be an integer");
        return ESP_ERR_INVALID_ARG;
    }
    configuration->machine_status_gpio = (int32_t)gpio;
    cJSON *active = cJSON_GetObjectItemCaseSensitive(machine_source, cJSON_IsObject(machine_object) ? "active_level" : "machine_status_active_level");
    if (active != NULL && (!cJSON_IsString(active)
        || (strcmp(active->valuestring, "high") != 0 && strcmp(active->valuestring, "low") != 0))) {
        snprintf(error, error_size, "machine_status_active_level must be high or low");
        return ESP_ERR_INVALID_ARG;
    }
    if (active != NULL) {
        configuration->machine_status_active_high = strcmp(active->valuestring, "high") == 0;
    }
    cJSON *pull = cJSON_GetObjectItemCaseSensitive(machine_source, cJSON_IsObject(machine_object) ? "pull" : "machine_status_pull");
    if (pull == NULL || (cJSON_IsString(pull) && strcmp(pull->valuestring, "none") == 0)) configuration->machine_status_pull = DEVICE_CONFIG_PULL_NONE;
    else if (cJSON_IsString(pull) && strcmp(pull->valuestring, "up") == 0) configuration->machine_status_pull = DEVICE_CONFIG_PULL_UP;
    else if (cJSON_IsString(pull) && strcmp(pull->valuestring, "down") == 0) configuration->machine_status_pull = DEVICE_CONFIG_PULL_DOWN;
    else { snprintf(error, error_size, "machine_status_pull is invalid"); return ESP_ERR_INVALID_ARG; }
    return device_config_validate(configuration, error, error_size);
}

static esp_err_t candidate_put_handler(httpd_req_t *request)
{
    if (require_auth(request, true) != ESP_OK) return ESP_OK;
    cJSON *body = NULL;
    if (receive_json(request, &body) != ESP_OK) return ESP_OK;
    device_config_t *candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL) {
        clear_candidate_json_secrets(body);
        cJSON_Delete(body);
        return json_error(request, "503 Service Unavailable", "candidate buffer unavailable");
    }
    char error[160] = {0};
    esp_err_t parse_result = parse_configuration(body, candidate, error, sizeof(error));
    clear_candidate_json_secrets(body);
    cJSON_Delete(body);
    if (parse_result == ESP_OK) parse_result = device_config_stage_candidate(candidate);
    memset(candidate, 0, sizeof(*candidate));
    free(candidate);
    if (parse_result == ESP_ERR_INVALID_STATE) {
        return json_error(request, "409 Conflict", "candidate validation is already in progress");
    }
    if (parse_result != ESP_OK) return json_error(request, "422 Unprocessable Content", error[0] ? error : "candidate persistence failed");
    return send_json_text(request, "{\"ok\":true,\"state\":\"pending\"}");
}

static esp_err_t candidate_delete_handler(httpd_req_t *request)
{
    if (require_auth(request, true) != ESP_OK) return ESP_OK;
    esp_err_t result = device_config_cancel_candidate();
    if (result == ESP_ERR_INVALID_STATE) {
        return json_error(request, "409 Conflict", "candidate validation is already in progress");
    }
    if (result != ESP_OK) return json_error(request, "500 Internal Server Error", "candidate cancellation failed");
    return send_json_text(request, "{\"ok\":true}");
}

static esp_err_t delayed_restart_response(httpd_req_t *request, bool factory_reset)
{
    if (require_auth(request, true) != ESP_OK) return ESP_OK;
    if (factory_reset) {
        char confirmation[64];
        if (httpd_req_get_hdr_value_str(request, "X-Confirm-Factory-Reset", confirmation, sizeof(confirmation)) != ESP_OK
            || !provisioning_security_factory_confirmed(confirmation)) {
            return json_error(request, "409 Conflict", "strong factory-reset confirmation is required");
        }
        if (device_config_factory_reset() != ESP_OK) return json_error(request, "500 Internal Server Error", "factory reset failed");
    }
    send_json_text(request, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}

static esp_err_t apply_handler(httpd_req_t *request)
{
    if (require_auth(request, true) != ESP_OK) return ESP_OK;
    if (device_config_candidate_validation_in_progress()) {
        return json_error(request, "409 Conflict", "candidate validation is already in progress");
    }
    device_config_metadata_t metadata;
    if (device_config_get_metadata(&metadata) != ESP_OK || metadata.state != DEVICE_CONFIG_STATE_PENDING) {
        return json_error(request, "409 Conflict", "no validated candidate is pending");
    }
    send_json_text(request, "{\"ok\":true,\"rebooting\":true}");
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
    return ESP_OK;
}
static esp_err_t reboot_handler(httpd_req_t *request) { return delayed_restart_response(request, false); }
static esp_err_t factory_reset_handler(httpd_req_t *request) { return delayed_restart_response(request, true); }

static bool request_cursor(httpd_req_t *request, uint64_t *cursor)
{
    *cursor = 0;
    size_t query_length = httpd_req_get_url_query_len(request);
    if (query_length == 0) return true;
    if (query_length >= 128) return false;
    char query[128];
    char value[24];
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK
        || httpd_query_key_value(query, "after", value, sizeof(value)) != ESP_OK) {
        return false;
    }
    char *end = NULL;
    unsigned long long parsed = strtoull(value, &end, 10);
    if (value[0] == '\0' || end == NULL || *end != '\0') return false;
    *cursor = (uint64_t)parsed;
    return true;
}

static bool request_stream_generation(httpd_req_t *request, char output[21])
{
    size_t query_length = httpd_req_get_url_query_len(request);
    if (query_length == 0 || query_length >= 128) return false;
    char query[128] = {0};
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK
        || httpd_query_key_value(query, "generation", output, 21) != ESP_OK) {
        secure_clear(query, sizeof(query));
        return false;
    }
    secure_clear(query, sizeof(query));
    uint64_t parsed = 0;
    return provisioning_security_parse_stream_generation(output, &parsed);
}

static esp_err_t stream_no_content(httpd_req_t *request)
{
    httpd_resp_set_status(request, "204 No Content");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, NULL, 0);
}

static cJSON *logs_json(uint64_t after, uint64_t *cursor, bool *has_more)
{
    diagnostic_log_batch_t *batch = calloc(1, sizeof(*batch));
    if (batch == NULL) return NULL;
    diagnostic_log_get_batch(batch, after);
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        free(batch);
        return NULL;
    }
    cJSON_AddNumberToObject(root, "cursor", batch->cursor);
    cJSON_AddNumberToObject(root, "first_available_sequence", batch->first_available_sequence);
    cJSON_AddNumberToObject(root, "next_sequence", batch->next_sequence);
    cJSON_AddNumberToObject(root, "overwritten", batch->overwritten);
    cJSON_AddNumberToObject(root, "lost_before_cursor", batch->lost_before_cursor);
    cJSON_AddBoolToObject(root, "has_more", batch->has_more);
    cJSON *array = cJSON_AddArrayToObject(root, "records");
    for (size_t index = 0; index < batch->count; index++) {
        const diagnostic_log_record_t *record = &batch->records[index];
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "sequence", record->sequence);
        cJSON_AddNumberToObject(item, "relative_ms", record->relative_ms);
        char level[2] = {record->level, '\0'};
        cJSON_AddStringToObject(item, "level", level);
        cJSON_AddStringToObject(item, "component", record->component);
        cJSON_AddStringToObject(item, "message", record->message);
        cJSON_AddItemToArray(array, item);
    }
    *cursor = batch->cursor;
    *has_more = batch->has_more;
    free(batch);
    return root;
}

static esp_err_t logs_handler(httpd_req_t *request)
{
    if (require_auth(request, false) != ESP_OK) return ESP_OK;
    uint64_t after;
    if (!request_cursor(request, &after)) {
        return json_error(request, "400 Bad Request", "after cursor is invalid");
    }
    uint64_t cursor;
    bool has_more;
    cJSON *root = logs_json(after, &cursor, &has_more);
    if (root == NULL) return json_error(request, "503 Service Unavailable", "diagnostic batch unavailable");
    esp_err_t result = send_json(request, root);
    cJSON_Delete(root);
    return result;
}

static void complete_stream_context(diagnostic_stream_context_t *context)
{
    if (context != NULL && provisioning_async_mark_complete(&context->lifecycle)) {
        esp_err_t complete_result = httpd_req_async_handler_complete(context->request);
        if (complete_result != ESP_OK) {
            ESP_LOGE(TAG, "SSE async request completion failed: %s", esp_err_to_name(complete_result));
        }
    }
}

static void logs_stream_worker(void *parameters)
{
    diagnostic_stream_context_t *context = parameters;
    httpd_req_t *request = context->request;
    httpd_resp_set_type(request, "text/event-stream");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    uint64_t sequence;
    if (!request_cursor(request, &sequence)) sequence = 0;
    for (int iteration = 0; iteration < 30; iteration++) {
        if (!stream_session_is_valid(
                context->session_generation,
                context->worker_generation
            )) break;
        uint64_t next_cursor = sequence;
        bool has_more = false;
        cJSON *root = logs_json(sequence, &next_cursor, &has_more);
        if (root == NULL) break;
        char *json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (json == NULL || httpd_resp_send_chunk(request, "data: ", 6) != ESP_OK
            || httpd_resp_send_chunk(request, json, HTTPD_RESP_USE_STRLEN) != ESP_OK
            || httpd_resp_send_chunk(request, "\n\n", 2) != ESP_OK) {
            cJSON_free(json);
            break;
        }
        cJSON_free(json);
        sequence = next_cursor;
        vTaskDelay(has_more ? 1 : pdMS_TO_TICKS(1000));
    }
    httpd_resp_send_chunk(request, NULL, 0);
    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    complete_stream_context(context);
    if (stream_lock()) {
        provisioning_stream_complete(&stream_lifecycle, context->worker_generation);
        stream_unlock();
    }
    ESP_LOGI(TAG, "Diagnostic SSE worker stopped; stack high-water mark=%u bytes", (unsigned)high_water_mark);
    secure_clear(context, sizeof(*context));
    free(context);
    vTaskDelete(NULL);
}

static esp_err_t logs_stream_handler(httpd_req_t *request)
{
    uint64_t authorized_generation = 0;
    char requested_generation[21] = {0};
    if (!authenticated(request, false, &authorized_generation)
        || !request_stream_generation(request, requested_generation)) {
        secure_clear(requested_generation, sizeof(requested_generation));
        return stream_no_content(request);
    }
    int64_t expires_ms;
    uint64_t current_generation;
    uint64_t worker_generation = 0;
    if (!stream_lock()) {
        secure_clear(requested_generation, sizeof(requested_generation));
        return stream_no_content(request);
    }
    taskENTER_CRITICAL(&service_state_lock);
    expires_ms = session_expires_ms;
    current_generation = session_generation;
    bool generation_authorized = provisioning_security_stream_request_authorized(
        requested_generation,
        authorized_generation,
        expires_ms,
        now_ms()
    ) && current_generation == authorized_generation;
    bool admitted = generation_authorized
        && provisioning_stream_admit(&stream_lifecycle, &worker_generation);
    taskEXIT_CRITICAL(&service_state_lock);
    stream_unlock();
    secure_clear(requested_generation, sizeof(requested_generation));
    if (!generation_authorized) return stream_no_content(request);
    if (!admitted) {
        return json_error(request, "409 Conflict", "only one diagnostic stream is allowed");
    }
    httpd_req_t *async_request = NULL;
    esp_err_t result = httpd_req_async_handler_begin(request, &async_request);
    if (result != ESP_OK) {
        if (stream_lock()) {
            provisioning_stream_complete(&stream_lifecycle, worker_generation);
            stream_unlock();
        }
        return json_error(request, "503 Service Unavailable", "asynchronous diagnostic stream unavailable");
    }
    diagnostic_stream_context_t *context = calloc(1, sizeof(*context));
    if (context == NULL) {
        provisioning_async_lifecycle_t lifecycle = {0};
        json_error(async_request, "503 Service Unavailable", "diagnostic stream context unavailable");
        if (provisioning_async_mark_complete(&lifecycle)) {
            httpd_req_async_handler_complete(async_request);
        }
        if (stream_lock()) {
            provisioning_stream_complete(&stream_lifecycle, worker_generation);
            stream_unlock();
        }
        return ESP_OK;
    }
    context->request = async_request;
    context->session_generation = authorized_generation;
    context->worker_generation = worker_generation;
    bool socket_attached = false;
    if (stream_lock()) {
        socket_attached = provisioning_stream_attach_socket(
            &stream_lifecycle,
            worker_generation,
            httpd_req_to_sockfd(async_request)
        );
        stream_unlock();
    }
    if (!socket_attached) {
        json_error(async_request, "503 Service Unavailable", "diagnostic stream is stopping");
        complete_stream_context(context);
        if (stream_lock()) {
            provisioning_stream_complete(&stream_lifecycle, worker_generation);
            stream_unlock();
        }
        secure_clear(context, sizeof(*context));
        free(context);
        return ESP_OK;
    }
    BaseType_t created = xTaskCreate(
        logs_stream_worker,
        "diagnostic_sse",
        5120,
        context,
        3,
        NULL
    );
    if (created != pdPASS) {
        json_error(async_request, "503 Service Unavailable", "diagnostic worker unavailable");
        complete_stream_context(context);
        if (stream_lock()) {
            provisioning_stream_complete(&stream_lifecycle, worker_generation);
            stream_unlock();
        }
        secure_clear(context, sizeof(*context));
        free(context);
        return ESP_OK;
    }
    return ESP_OK;
}

static esp_err_t register_handler(const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t descriptor = {.uri = uri, .method = method, .handler = handler};
    return httpd_register_uri_handler(server, &descriptor);
}

esp_err_t provisioning_service_start(
    const device_config_t *runtime_configuration,
    const char *setup_secret,
    provisioning_state_machine_t *state_machine
)
{
    if (server != NULL || setup_secret == NULL || state_machine == NULL) return ESP_ERR_INVALID_ARG;
    if (stream_mutex == NULL) {
        stream_mutex = xSemaphoreCreateMutexStatic(&stream_mutex_storage);
        if (stream_mutex == NULL) return ESP_ERR_NO_MEM;
    }
    if (!stream_lock()) return ESP_FAIL;
    provisioning_stream_lifecycle_init(&stream_lifecycle);
    stream_unlock();
    current_configuration = runtime_configuration;
    setup_secret_value = setup_secret;
    machine = state_machine;
    httpd_config_t configuration = HTTPD_DEFAULT_CONFIG();
    configuration.max_uri_handlers = 12;
    configuration.max_open_sockets = 4;
    configuration.lru_purge_enable = true;
    configuration.recv_wait_timeout = 5;
    configuration.send_wait_timeout = 2;
    configuration.stack_size = 8192;
    esp_err_t result = httpd_start(&server, &configuration);
    if (result != ESP_OK) {
        current_configuration = NULL;
        setup_secret_value = NULL;
        machine = NULL;
        return result;
    }
    struct {
        const char *uri;
        httpd_method_t method;
        esp_err_t (*handler)(httpd_req_t *);
    } handlers[] = {
        {"/", HTTP_GET, root_handler},
        {"/api/session", HTTP_POST, session_create_handler},
        {"/api/session", HTTP_DELETE, session_delete_handler},
        {"/api/status", HTTP_GET, status_handler},
        {"/api/config", HTTP_GET, config_handler},
        {"/api/config/candidate", HTTP_PUT, candidate_put_handler},
        {"/api/config/candidate", HTTP_DELETE, candidate_delete_handler},
        {"/api/config/apply", HTTP_POST, apply_handler},
        {"/api/logs", HTTP_GET, logs_handler},
        {"/api/logs/stream", HTTP_GET, logs_stream_handler},
        {"/api/reboot", HTTP_POST, reboot_handler},
        {"/api/factory-reset", HTTP_POST, factory_reset_handler},
    };
    for (size_t index = 0; index < sizeof(handlers) / sizeof(handlers[0]); index++) {
        result = register_handler(handlers[index].uri, handlers[index].method, handlers[index].handler);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "HTTP handler registration failed for %s", handlers[index].uri);
            httpd_handle_t partial = server;
            server = NULL;
            httpd_stop(partial);
            current_configuration = NULL;
            setup_secret_value = NULL;
            machine = NULL;
            return result;
        }
    }
    ESP_LOGI(TAG, "Authenticated local provisioning HTTP server started");
    return ESP_OK;
}

esp_err_t provisioning_service_stop(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&service_stop_in_progress, &expected, true)) {
        return ESP_ERR_TIMEOUT;
    }
    if (server == NULL) {
        atomic_store(&service_stop_in_progress, false);
        return ESP_OK;
    }
    uint64_t worker_generation = request_stream_shutdown(true);
    int64_t deadline_ms = now_ms() + 4000;
    while (worker_generation != 0) {
        bool stopped = false;
        if (stream_lock()) {
            stopped = provisioning_stream_stop_satisfied(
                &stream_lifecycle,
                worker_generation
            );
            stream_unlock();
        }
        if (stopped) break;
        if (now_ms() >= deadline_ms) {
            ESP_LOGE(TAG, "Timed out waiting for diagnostic SSE worker shutdown");
            atomic_store(&service_stop_in_progress, false);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    httpd_handle_t handle = server;
    session_clear();
    esp_err_t result = httpd_stop(handle);
    if (result == ESP_OK) {
        if (stream_lock()) {
            provisioning_stream_lifecycle_init(&stream_lifecycle);
            stream_unlock();
        }
        taskENTER_CRITICAL(&service_state_lock);
        server = NULL;
        current_configuration = NULL;
        setup_secret_value = NULL;
        machine = NULL;
        login_failures = 0;
        login_locked_until_ms = 0;
        taskEXIT_CRITICAL(&service_state_lock);
    }
    atomic_store(&service_stop_in_progress, false);
    return result;
}

bool provisioning_service_is_running(void)
{
    bool running;
    taskENTER_CRITICAL(&service_state_lock);
    running = server != NULL;
    taskEXIT_CRITICAL(&service_state_lock);
    return running;
}

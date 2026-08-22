from pathlib import Path
import subprocess
import textwrap


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def write_freertos_mutex_stubs(root: Path):
    freertos = root / "freertos"
    freertos.mkdir(parents=True, exist_ok=True)
    (freertos / "FreeRTOS.h").write_text(textwrap.dedent(r"""
        #pragma once
        #include <pthread.h>
        #include <stdint.h>
        typedef pthread_mutex_t StaticSemaphore_t;
        typedef pthread_mutex_t *SemaphoreHandle_t;
        typedef int BaseType_t;
        typedef uint32_t TickType_t;
        #define pdTRUE 1
        #define pdFALSE 0
        #define portMAX_DELAY UINT32_MAX
    """), encoding="utf-8")
    (freertos / "semphr.h").write_text(textwrap.dedent(r"""
        #pragma once
        #include "FreeRTOS.h"
        static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage) {
            return pthread_mutex_init(storage, 0) == 0 ? storage : 0;
        }
        static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t ticks) {
            (void)ticks; return pthread_mutex_lock(mutex) == 0 ? pdTRUE : pdFALSE;
        }
        static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t mutex) {
            return pthread_mutex_unlock(mutex) == 0 ? pdTRUE : pdFALSE;
        }
    """), encoding="utf-8")


def compile_and_run(tmp_path: Path, name: str, sources: list[Path], include_dirs: list[Path], body: str, flags=None):
    harness = tmp_path / f"{name}.c"
    executable = tmp_path / name
    harness.write_text(textwrap.dedent(body), encoding="utf-8")
    command = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", *(flags or [])]
    for directory in include_dirs:
        command.extend(["-I", str(directory)])
    command.extend([str(source) for source in sources])
    command.extend([str(harness), "-o", str(executable)])
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)


def test_sensor_read_failure_invalidates_provider_and_recovers_next_cycle(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/sensor"
    (tmp_path / "esp_err.h").write_text(textwrap.dedent("""
        #pragma once
        typedef int esp_err_t;
        #define ESP_OK 0
        #define ESP_FAIL -1
        #define ESP_ERR_INVALID_ARG 2
    """), encoding="utf-8")
    (tmp_path / "esp_log.h").write_text(textwrap.dedent("""
        #pragma once
        #define ESP_LOGE(tag, format, ...) ((void)(tag))
        #define ESP_LOGW(tag, format, ...) ((void)(tag))
        #define ESP_LOGI(tag, format, ...) ((void)(tag))
        static inline const char *esp_err_to_name(int error) { (void)error; return "stub"; }
    """), encoding="utf-8")
    (tmp_path / "bme280.h").write_text(textwrap.dedent("""
        #pragma once
        #include "esp_err.h"
        typedef struct {
            float temperature_c;
            float pressure_hpa;
            float humidity_percent;
        } bme280_measurement_t;
        esp_err_t bme280_init(void);
        esp_err_t bme280_deinit(void);
        esp_err_t bme280_read(bme280_measurement_t *measurement);
    """), encoding="utf-8")
    compile_and_run(
        tmp_path,
        "sensor_recovery_test",
        [component / "sensor.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include "bme280.h"
        #include "sensor.h"

        static int init_calls;
        static int deinit_calls;
        static int read_calls;

        esp_err_t bme280_init(void) { init_calls++; return ESP_OK; }
        esp_err_t bme280_deinit(void) { deinit_calls++; return ESP_OK; }
        esp_err_t bme280_read(bme280_measurement_t *measurement) {
            read_calls++;
            if (read_calls == 1) return ESP_FAIL;
            measurement->temperature_c = 24.5f;
            measurement->humidity_percent = 51.0f;
            measurement->pressure_hpa = 1008.0f;
            return ESP_OK;
        }

        int main(void) {
            sensor_data_t data = {0};
            assert(sensor_read(&data) == ESP_FAIL);
            assert(init_calls == 1 && read_calls == 1 && deinit_calls == 1);
            assert(sensor_read(&data) == ESP_OK);
            assert(init_calls == 2 && read_calls == 2 && deinit_calls == 1);
            assert(data.temperature == 24.5f && data.humidity == 51.0f);
            return 0;
        }
        """,
    )


def test_invalid_measurement_schedules_provider_reinitialization():
    source = (
        REPOSITORY_ROOT / "firmware/components/telemetry/telemetry_model.c"
    ).read_text(encoding="utf-8")
    validation_branch = source[source.index("if (validation_result != ESP_OK)"):]
    assert validation_branch.index("sensor_invalidate()") < validation_branch.index(
        "return validation_result"
    )


def test_bme280_recovery_never_loses_or_duplicates_an_i2c_handle():
    source = (
        REPOSITORY_ROOT / "firmware/components/bme280/bme280.c"
    ).read_text(encoding="utf-8")
    remove = source[
        source.index("static esp_err_t bme280_remove_device"):
        source.index("static esp_err_t bme280_wait_for_calibration_copy")
    ]
    assert remove.index("return result;") < remove.index("bme280_device = NULL;")

    initialize = source[source.index("esp_err_t bme280_init(void)"):]
    assert initialize.index("bme280_remove_device()") < initialize.index(
        "i2c_master_bus_add_device"
    )


def test_provisioning_state_concurrent_uint64_access(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/provisioning_state"
    write_freertos_mutex_stubs(tmp_path)
    compile_and_run(
        tmp_path,
        "provisioning_state_concurrency_test",
        [component / "provisioning_state.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include <pthread.h>
        #include "provisioning_state.h"
        static provisioning_state_machine_t state;
        static void *activity(void *unused) {
            (void)unused;
            for (uint64_t i = 1; i < 100000; i++)
                provisioning_state_configuration_loaded(&state, true, true, i, 1000, 1000);
            return 0;
        }
        static void *ticker(void *unused) {
            (void)unused;
            for (uint64_t i = 1; i < 100000; i++) {
                provisioning_state_snapshot_t snapshot;
                assert(provisioning_state_get_snapshot(&state, &snapshot));
                assert(snapshot.opened_ms == snapshot.last_authenticated_ms);
            }
            return 0;
        }
        int main(void) {
            provisioning_state_init(&state);
            provisioning_state_configuration_loaded(&state, true, true, 0, 1000, 1000);
            pthread_t writers[2];
            assert(pthread_create(&writers[0], 0, activity, 0) == 0);
            assert(pthread_create(&writers[1], 0, ticker, 0) == 0);
            pthread_join(writers[0], 0); pthread_join(writers[1], 0);
            return 0;
        }
        """,
        flags=["-pthread"],
    )


def test_stable_device_config_storage_format_roundtrip_and_rejection(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/device_config"
    (tmp_path / "esp_err.h").write_text(textwrap.dedent("""
        typedef int esp_err_t;
        #define ESP_OK 0
        #define ESP_ERR_INVALID_ARG 2
        #define ESP_ERR_INVALID_SIZE 3
        #define ESP_ERR_NOT_SUPPORTED 8
    """), encoding="utf-8")
    compile_and_run(
        tmp_path,
        "device_config_storage_format_test",
        [component / "device_config_storage.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include <stdint.h>
        #include <stdlib.h>
        #include <string.h>
        #include "device_config_storage.h"
        int main(void) {
            device_config_t input = {0}, decoded = {0};
            input.schema_version = 1; input.revision = 42;
            strcpy(input.device_id, "edge-node-03"); strcpy(input.wifi_ssid, "factory");
            strcpy(input.wifi_password, "secret123"); strcpy(input.mqtt_broker_uri, "mqtts://broker:8883");
            strcpy(input.mqtt_username, "edge-node-03"); strcpy(input.mqtt_password, "credential");
            strcpy(input.mqtt_client_id, "iem-edge-node-03"); strcpy(input.mqtt_ca_certificate, "CA");
            input.telemetry_interval_seconds = 5; input.machine_status_provider = DEVICE_CONFIG_MACHINE_GPIO;
            input.machine_status_gpio = 27; input.machine_status_active_high = true;
            input.maintenance_on_boot = true; input.maintenance_window_seconds = 300;
            input.maintenance_max_session_seconds = 900;
            size_t size = device_config_storage_configuration_size();
            uint8_t *first = calloc(1, size), *second = calloc(1, size);
            assert(first && second);
            assert(device_config_storage_encode_configuration(&input, first, size) == ESP_OK);
            assert(memcmp(first, "IEMC", 4) == 0 && first[4] == 1 && first[5] == 0);
            assert(device_config_storage_decode_configuration(first, size, &decoded) == ESP_OK);
            assert(decoded.revision == input.revision && strcmp(decoded.mqtt_password, "credential") == 0);
            assert(device_config_storage_encode_configuration(&decoded, second, size) == ESP_OK);
            assert(memcmp(first, second, size) == 0); /* deterministic re-encode / migration primitive */
            first[4] = 2;
            assert(device_config_storage_decode_configuration(first, size, &decoded) == ESP_ERR_NOT_SUPPORTED);
            first[4] = 1;
            assert(device_config_storage_decode_configuration(first, size - 1, &decoded) == ESP_ERR_INVALID_SIZE);
            first[8] ^= 1;
            assert(device_config_storage_decode_configuration(first, size, &decoded) == ESP_ERR_INVALID_SIZE);

            device_config_metadata_t metadata = {.state=DEVICE_CONFIG_STATE_PENDING,.candidate_revision=42,.boot_attempts=1,.boot_count=7};
            strcpy(metadata.last_rollback_reason, "test");
            size_t metadata_size = device_config_storage_metadata_size();
            uint8_t *encoded_metadata = calloc(1, metadata_size);
            device_config_metadata_t decoded_metadata;
            assert(device_config_storage_encode_metadata(&metadata, encoded_metadata, metadata_size) == ESP_OK);
            assert(memcmp(encoded_metadata, "IEMM", 4) == 0);
            assert(device_config_storage_decode_metadata(encoded_metadata, metadata_size, &decoded_metadata) == ESP_OK);
            assert(decoded_metadata.boot_count == 7 && strcmp(decoded_metadata.last_rollback_reason, "test") == 0);
            assert(device_config_storage_decode_metadata(encoded_metadata, metadata_size - 1, &decoded_metadata) == ESP_ERR_INVALID_SIZE);
            free(encoded_metadata); free(second); free(first); return 0;
        }
        """,
    )


def test_mqtt_last_error_concurrent_copy_is_coherent(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/mqtt_client_app"
    write_freertos_mutex_stubs(tmp_path)
    compile_and_run(
        tmp_path,
        "mqtt_error_state_concurrency_test",
        [component / "mqtt_error_state.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include <pthread.h>
        #include <string.h>
        #include "mqtt_error_state.h"
        static mqtt_error_state_t state = MQTT_ERROR_STATE_INITIALIZER;
        static const char *a = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        static const char *b = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
        static void *writer(void *unused) {
            (void)unused;
            for (int i = 0; i < 100000; i++) mqtt_error_state_set(&state, (i & 1) ? a : b);
            return 0;
        }
        int main(void) {
            assert(mqtt_error_state_init(&state));
            pthread_t task; assert(pthread_create(&task, 0, writer, 0) == 0);
            for (int i = 0; i < 100000; i++) {
                char copy[MQTT_ERROR_TEXT_CAPACITY];
                assert(mqtt_error_state_copy(&state, copy, sizeof(copy)));
                assert(copy[0] == '\0' || strcmp(copy, a) == 0 || strcmp(copy, b) == 0);
            }
            pthread_join(task, 0); return 0;
        }
        """,
        flags=["-pthread"],
    )


def test_hardening_source_guards_cover_async_sse_stack_and_lifecycle():
    service = (REPOSITORY_ROOT / "firmware/components/provisioning_service/provisioning_service.c").read_text()
    app = (REPOSITORY_ROOT / "firmware/main/app_main.c").read_text()
    config = (REPOSITORY_ROOT / "firmware/components/device_config/device_config.c").read_text()
    health = (REPOSITORY_ROOT / "firmware/components/device_health_service/device_health_service.c").read_text()
    diagnostic = (REPOSITORY_ROOT / "firmware/components/diagnostic_log/diagnostic_log.c").read_text()

    handler = service[service.index("static esp_err_t logs_stream_handler"):service.index("static esp_err_t register_handler")]
    worker = service[service.index("static void logs_stream_worker"):service.index("static esp_err_t logs_stream_handler")]
    assert "httpd_req_async_handler_begin" in handler and "xTaskCreate" in handler
    assert "for (" not in handler and "httpd_queue_work" not in service
    assert "httpd_req_async_handler_complete" in handler  # allocation failure completes directly
    assert "complete_stream_context(context)" in handler  # worker-create failure
    assert "complete_stream_context(context)" in worker  # success, send error, expiry and stop converge here
    assert "context->worker_generation" in worker
    assert "provisioning_stream_complete(&stream_lifecycle, context->worker_generation)" in worker
    assert "provisioning_stream_stop_satisfied" in service
    assert "worker_generation = request_stream_shutdown(true)" in service
    assert "xSemaphoreCreateBinary" not in service and "stream_done" not in service
    assert "atomic_compare_exchange_strong(&stream_active" not in handler
    assert '"204 No Content"' in service
    assert "stream_generation" in service and "encodeURIComponent(streamGeneration)" in service
    assert "closeLive();clearConfigurationInputs();csrf='';streamGeneration=''" in service
    assert "o.textContent=JSON.stringify(j" not in service
    assert '{"/api/status", HTTP_GET, status_handler}' in service
    assert '{"/api/config", HTTP_GET, config_handler}' in service
    assert "httpd_stop(partial)" in service
    assert "device_config_metadata_t metadata = {0}" in service
    assert "type=password" in service and "delete q.mqtt.password" in service
    assert "loadCurrent()" in service and "closeLive()" in service and "logout()" in service
    receive = service[service.index("static esp_err_t receive_json"):service.index("static bool cookie_token")]
    assert "secure_clear(body, request->content_len + 1)" in receive
    cleanup = service[service.index("static void clear_candidate_json_secrets"):service.index("static bool cookie_token")]
    assert 'clear_json_string(candidate, "wifi_password")' in cleanup
    assert 'clear_json_string(candidate, "mqtt_password")' in cleanup
    assert 'clear_json_string(mqtt, "password")' in cleanup
    candidate_handler = service[service.index("static esp_err_t candidate_put_handler"):service.index("static esp_err_t candidate_delete_handler")]
    assert candidate_handler.count("clear_candidate_json_secrets(body)") == 2
    assert "finally{s.value=''}" in service
    assert "finally{wp.value='';mp.value=''}" in service
    assert "finally{i.value=''}" in service and "p.value=''" in service
    assert "function clearConfigurationInputs(){c.value='';p.value='';wp.value='';mp.value=''}" in service
    assert "else{clearConfigurationInputs();ce.textContent=JSON.stringify(q,null,2)}" in service
    assert "catch(e){clearConfigurationInputs();ce.textContent='Configuration unavailable'}" in service
    assert "if(r.ok)clearConfigurationInputs()" in service
    assert "async function logout(){closeLive();clearConfigurationInputs()" in service
    assert "device_config_candidate_validation_in_progress()" in service
    assert '"409 Conflict", "candidate validation is already in progress"' in service

    provisioning_start = app.index("provisioning_service_start")
    maintenance_start = app.index('"maintenance_window"', provisioning_start)
    assert provisioning_start < maintenance_start < app.index("mqtt_init")
    activation = config[config.index("esp_err_t device_config_activate_candidate"):config.index("esp_err_t device_config_rollback_candidate")]
    assert "calloc" in activation and "secure_clear" in activation
    assert "device_config_t candidate;" not in activation
    assert "atomic_flag" not in (REPOSITORY_ROOT / "firmware/components/provisioning_state/provisioning_state.c").read_text()
    assert "atomic_flag" not in (REPOSITORY_ROOT / "firmware/components/mqtt_client_app/mqtt_error_state.c").read_text()
    assert app.index("serial_recovery_start()") < app.index("device_config_get_or_create_setup_secret")
    assert 'ESP_LOGW(TAG, "First-boot provisioning secret:' not in app
    assert app.index("IEM first-boot setup code") < app.index("diagnostic_log_init()", app.index("IEM first-boot setup code"))
    health_start = health[health.index("esp_err_t device_health_service_start"):]
    assert health_start.index("if (task_handle != NULL)") < health_start.index("snprintf(runtime_device_id")
    assert "diagnostic_sink_installed" in diagnostic
    assert diagnostic.index("if (diagnostic_sink_installed)") < diagnostic.index("esp_log_set_vprintf")
    assert "isalnum" not in config and "<ctype.h>" not in config
    assert "retrying bounded cleanup" not in app

    wifi = (REPOSITORY_ROOT / "firmware/components/wifi/wifi.c").read_text()
    rssi = wifi[wifi.index("esp_err_t wifi_get_station_rssi"):wifi.index("esp_err_t wifi_get_softap_ipv4")]
    assert rssi.index("atomic_load(&station_connected)") < rssi.index("esp_wifi_sta_get_ap_info")
    assert 'ESP_RETURN_ON_ERROR(esp_wifi_sta_get_ap_info' not in rssi
    status = service[service.index("static esp_err_t status_handler"):service.index("static esp_err_t config_handler")]
    assert "station_connected && wifi_get_station_rssi(&rssi) == ESP_OK" in status
    assert 'cJSON_AddNullToObject(response, "wifi_rssi_dbm")' in status


def test_provisioning_state_transitions_and_timeouts(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/provisioning_state"
    write_freertos_mutex_stubs(tmp_path)
    compile_and_run(
        tmp_path,
        "provisioning_state_test",
        [component / "provisioning_state.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include <string.h>
        #include "provisioning_state.h"

        int main(void) {
            provisioning_state_machine_t state;
            provisioning_state_init(&state);
            assert(state.state == PROVISIONING_LOAD_CONFIGURATION);
            provisioning_state_configuration_loaded(&state, false, true, 100, 0, 0);
            assert(state.state == PROVISIONING_UNPROVISIONED);
            assert(!provisioning_state_tick(&state, 999999));

            provisioning_state_configuration_loaded(&state, true, false, 100, 60, 120);
            assert(state.state == PROVISIONING_OPERATIONAL);

            provisioning_state_configuration_loaded(&state, true, true, 1000, 60, 120);
            assert(state.state == PROVISIONING_MAINTENANCE_WINDOW);
            assert(provisioning_state_tick(&state, 60999));
            provisioning_state_authenticated_activity(&state, 60000);
            assert(provisioning_state_tick(&state, 119999));
            assert(!provisioning_state_tick(&state, 121000));
            assert(state.state == PROVISIONING_OPERATIONAL);

            provisioning_state_configuration_loaded(&state, true, true, 0, 60, 120);
            provisioning_state_authenticated_activity(&state, 119000);
            assert(!provisioning_state_tick(&state, 120000));
            assert(strcmp(provisioning_state_name(state.state), "operational") == 0);
            return 0;
        }
        """,
    )


def test_diagnostic_ring_wraparound_loss_and_sequence_filter(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/diagnostic_log"
    stub = tmp_path / "esp_err.h"
    stub.write_text("typedef int esp_err_t;\n", encoding="utf-8")
    compile_and_run(
        tmp_path,
        "diagnostic_ring_test",
        [component / "diagnostic_ring.c"],
        [tmp_path, component / "include"],
        r"""
        #include <assert.h>
        #include <stdio.h>
        #include "diagnostic_ring.h"

        int main(void) {
            diagnostic_ring_reset();
            for (int index = 0; index < DIAGNOSTIC_LOG_CAPACITY + 3; index++) {
                diagnostic_log_record_t record = {.level = 'I'};
                snprintf(record.component, sizeof(record.component), "test");
                snprintf(record.message, sizeof(record.message), "message-%d", index);
                diagnostic_ring_push(&record);
            }
            diagnostic_log_batch_t batch;
            diagnostic_ring_batch(&batch, 1);
            assert(batch.count == DIAGNOSTIC_LOG_BATCH_CAPACITY);
            assert(batch.overwritten == 3 && batch.lost_before_cursor == 2);
            assert(batch.records[0].sequence == 4 && batch.cursor == 11 && batch.has_more);
            diagnostic_ring_batch(&batch, DIAGNOSTIC_LOG_CAPACITY + 1);
            assert(batch.count == 2 && !batch.has_more);
            assert(batch.records[0].sequence == DIAGNOSTIC_LOG_CAPACITY + 2);
            diagnostic_ring_reset();
            diagnostic_ring_batch(&batch, 0);
            assert(batch.count == 0 && batch.overwritten == 0 && batch.next_sequence == 1);
            return 0;
        }
        """,
    )


def test_provisioning_security_auth_csrf_body_and_factory_confirmation(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/provisioning_service"
    compile_and_run(
        tmp_path,
        "provisioning_security_test",
        [component / "provisioning_security.c"],
        [component / "include"],
        r"""
        #include <assert.h>
        #include "provisioning_security.h"

        int main(void) {
            assert(provisioning_security_constant_time_equal("same", "same"));
            assert(!provisioning_security_constant_time_equal("same", "different"));
            assert(provisioning_security_json_body_allowed("application/json", 8, 8));
            assert(provisioning_security_json_body_allowed("application/json; charset=utf-8", 8, 8));
            assert(!provisioning_security_json_body_allowed("text/plain", 8, 8));
            assert(!provisioning_security_json_body_allowed("application/json", 9, 8));
            assert(!provisioning_security_json_body_allowed("application/json", 0, 8));

            assert(!provisioning_security_session_authorized("token", 0, "csrf", 0, 2000, 1000, false));
            assert(!provisioning_security_session_authorized("token", "token", "csrf", 0, 1000, 1000, false));
            assert(provisioning_security_session_authorized("token", "token", "csrf", 0, 2000, 1000, false));
            assert(!provisioning_security_session_authorized("token", "token", "csrf", 0, 2000, 1000, true));
            assert(!provisioning_security_session_authorized("token", "token", "csrf", "wrong", 2000, 1000, true));
            assert(provisioning_security_session_authorized("token", "token", "csrf", "csrf", 2000, 1000, true));

            assert(!provisioning_security_factory_confirmed(0));
            assert(!provisioning_security_factory_confirmed("ERASE"));
            assert(provisioning_security_factory_confirmed("ERASE-DEVICE-CONFIGURATION"));

            assert(provisioning_security_stream_session_valid(7, 7, 2000, 1000, false, false));
            assert(!provisioning_security_stream_session_valid(7, 8, 2000, 1000, false, false));
            assert(!provisioning_security_stream_session_valid(7, 7, 1000, 1000, false, false));
            assert(!provisioning_security_stream_session_valid(7, 7, 2000, 1000, true, false));
            assert(!provisioning_security_stream_session_valid(7, 7, 2000, 1000, false, true));
            uint64_t generation = 0;
            assert(provisioning_security_parse_stream_generation("7", &generation) && generation == 7);
            assert(provisioning_security_parse_stream_generation("18446744073709551615", &generation));
            assert(!provisioning_security_parse_stream_generation(0, &generation));
            assert(!provisioning_security_parse_stream_generation("", &generation));
            assert(!provisioning_security_parse_stream_generation("0", &generation));
            assert(!provisioning_security_parse_stream_generation("7x", &generation));
            assert(!provisioning_security_parse_stream_generation("18446744073709551616", &generation));
            assert(provisioning_security_stream_request_authorized("7", 7, 2000, 1000));
            assert(!provisioning_security_stream_request_authorized("7", 8, 2000, 1000)); /* new login */
            assert(!provisioning_security_stream_request_authorized("7", 7, 0, 1000)); /* logout */
            assert(!provisioning_security_stream_request_authorized("7", 7, 1000, 1000)); /* expiry */
            assert(!provisioning_security_stream_request_authorized(0, 7, 2000, 1000));
            assert(!provisioning_security_stream_request_authorized("malformed", 7, 2000, 1000));
            provisioning_async_lifecycle_t lifecycle = {0};
            assert(provisioning_async_mark_complete(&lifecycle));
            assert(!provisioning_async_mark_complete(&lifecycle));

            provisioning_stream_lifecycle_t stream;
            provisioning_stream_lifecycle_init(&stream);
            uint64_t old_worker = 0, new_worker = 0;
            assert(provisioning_stream_admit(&stream, &old_worker));
            assert(old_worker != 0 && provisioning_stream_attach_socket(&stream, old_worker, 10));
            int socket_fd = -1;
            assert(provisioning_stream_request_shutdown(&stream, false, &socket_fd) == old_worker);
            assert(socket_fd == 10 && !provisioning_stream_worker_current(&stream, old_worker));
            assert(provisioning_stream_complete(&stream, old_worker));
            assert(provisioning_stream_admit(&stream, &new_worker) && new_worker > old_worker);
            assert(provisioning_stream_attach_socket(&stream, new_worker, 11));
            assert(!provisioning_stream_complete(&stream, old_worker)); /* deterministic stale completion */
            uint64_t stop_target = provisioning_stream_request_shutdown(&stream, true, &socket_fd);
            assert(stop_target == new_worker && socket_fd == 11);
            assert(!provisioning_stream_stop_satisfied(&stream, stop_target));
            assert(!provisioning_stream_admit(&stream, &generation));
            assert(provisioning_stream_complete(&stream, new_worker));
            assert(provisioning_stream_stop_satisfied(&stream, stop_target));
            return 0;
        }
        """,
    )


def test_actual_device_config_storage_candidate_rollback_and_redaction(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/device_config"
    stubs = tmp_path / "stubs"
    (stubs / "mbedtls").mkdir(parents=True)
    write_freertos_mutex_stubs(stubs)
    headers = {
        "esp_err.h": """
            typedef int esp_err_t;
            #define ESP_OK 0
            #define ESP_FAIL 1
            #define ESP_ERR_INVALID_ARG 2
            #define ESP_ERR_INVALID_SIZE 3
            #define ESP_ERR_NVS_NOT_FOUND 4
                #define ESP_ERR_NVS_NO_FREE_PAGES 5
                #define ESP_ERR_NVS_NEW_VERSION_FOUND 6
                #define ESP_ERR_NO_MEM 7
                #define ESP_ERR_NOT_SUPPORTED 8
                #define ESP_ERR_INVALID_STATE 9
                #define ESP_ERR_NVS_TYPE_MISMATCH 10
                #define ESP_ERR_NVS_INVALID_LENGTH 11
        """,
        "esp_check.h": """
            #define ESP_RETURN_ON_ERROR(x, tag, fmt, ...) do { esp_err_t r_ = (x); if (r_ != ESP_OK) return r_; } while (0)
        """,
            "esp_log.h": """
                #define ESP_LOGW(tag, fmt, ...) ((void)(tag))
                #define ESP_LOGE(tag, fmt, ...) ((void)(tag))
            """,
            "esp_random.h": "void esp_fill_random(void *buffer, unsigned length);\n",
            "bootloader_random.h": "void bootloader_random_enable(void); void bootloader_random_disable(void);\n",
        "nvs.h": """
            #include <stddef.h>
            #include "esp_err.h"
            typedef int nvs_handle_t; typedef int nvs_open_mode_t;
            #define NVS_READONLY 0
            #define NVS_READWRITE 1
            esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *);
            esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
            esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
            esp_err_t nvs_erase_key(nvs_handle_t, const char *);
            esp_err_t nvs_commit(nvs_handle_t);
            void nvs_close(nvs_handle_t);
            esp_err_t nvs_erase_all(nvs_handle_t);
            esp_err_t nvs_get_str(nvs_handle_t, const char *, char *, size_t *);
            esp_err_t nvs_set_str(nvs_handle_t, const char *, const char *);
        """,
        "nvs_flash.h": """
            #include "esp_err.h"
            esp_err_t nvs_flash_init(void); esp_err_t nvs_flash_erase(void);
        """,
        "sdkconfig.h": "#define CONFIG_PROVISIONING_CANDIDATE_MAX_ATTEMPTS 2\n",
        "mbedtls/x509_crt.h": """
            #include <stddef.h>
            typedef struct { int unused; } mbedtls_x509_crt;
            void mbedtls_x509_crt_init(mbedtls_x509_crt *);
            int mbedtls_x509_crt_parse(mbedtls_x509_crt *, const unsigned char *, size_t);
            void mbedtls_x509_crt_free(mbedtls_x509_crt *);
        """,
    }
    for name, content in headers.items():
        target = stubs / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(textwrap.dedent(content), encoding="utf-8")
    compile_and_run(
        tmp_path,
        "device_config_test",
        [component / "device_config.c", component / "device_config_storage.c"],
        [stubs, component / "include"],
        r"""
        #include <assert.h>
        #include <stdbool.h>
        #include <stdint.h>
        #include <stdio.h>
        #include <string.h>
        #include "device_config.h"
        #include "device_config_storage.h"
        #include "mbedtls/x509_crt.h"
        #include "nvs.h"

        typedef struct { char key[20]; unsigned char data[6000]; size_t size; bool used; } slot_t;
        static slot_t slots[8];
        static slot_t pending;
        static bool pending_erase;
        static bool fail_commit;
        static int commits_until_failure;
        static esp_err_t flash_init_result = ESP_OK;
        static int flash_erases;
        static esp_err_t setup_get_error;

        static slot_t *find(const char *key) {
            for (size_t i = 0; i < 8; i++) if (slots[i].used && strcmp(slots[i].key, key) == 0) return &slots[i];
            return 0;
        }
        static slot_t *allocate(const char *key) {
            slot_t *slot = find(key); if (slot) return slot;
            for (size_t i = 0; i < 8; i++) if (!slots[i].used) { slots[i].used = true; snprintf(slots[i].key, sizeof(slots[i].key), "%s", key); return &slots[i]; }
            return 0;
        }
        esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle) { (void)name; (void)mode; *handle = 1; return ESP_OK; }
        void nvs_close(nvs_handle_t handle) { (void)handle; }
        esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
            (void)h; slot_t *slot = find(key); if (!slot) return ESP_ERR_NVS_NOT_FOUND;
            size_t actual = slot->size; if (out == 0) { *size = actual; return ESP_OK; }
            if (*size < actual) return ESP_ERR_INVALID_SIZE;
            memcpy(out, slot->data, actual); *size = actual; return ESP_OK;
        }
        esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t size) {
            (void)h; memset(&pending, 0, sizeof(pending)); pending.used = true;
            snprintf(pending.key, sizeof(pending.key), "%s", key); memcpy(pending.data, data, size); pending.size = size; pending_erase = false; return ESP_OK;
        }
        esp_err_t nvs_erase_key(nvs_handle_t h, const char *key) {
            (void)h; memset(&pending, 0, sizeof(pending)); pending.used = true; snprintf(pending.key, sizeof(pending.key), "%s", key); pending_erase = true;
            return find(key) ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
        }
        esp_err_t nvs_commit(nvs_handle_t h) {
            (void)h; if (commits_until_failure > 0 && --commits_until_failure == 0) { memset(&pending, 0, sizeof(pending)); return ESP_FAIL; }
            if (fail_commit) { fail_commit = false; memset(&pending, 0, sizeof(pending)); return ESP_FAIL; }
            if (!pending.used) return ESP_OK;
            slot_t *slot = find(pending.key);
            if (pending_erase) { if (slot) memset(slot, 0, sizeof(*slot)); }
            else { slot = allocate(pending.key); assert(slot); *slot = pending; }
            memset(&pending, 0, sizeof(pending)); pending_erase = false; return ESP_OK;
        }
        esp_err_t nvs_erase_all(nvs_handle_t h) { (void)h; memset(slots, 0, sizeof(slots)); return ESP_OK; }
        esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *out, size_t *length) {
            if (strcmp(key, "setup_secret") == 0 && setup_get_error != ESP_OK) return setup_get_error;
            return nvs_get_blob(h, key, out, length);
        }
        esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *value) { return nvs_set_blob(h, key, value, strlen(value) + 1); }
        esp_err_t nvs_flash_init(void) { return flash_init_result; }
        esp_err_t nvs_flash_erase(void) { flash_erases++; memset(slots, 0, sizeof(slots)); return ESP_OK; }
        static int entropy_step;
        void bootloader_random_enable(void) { assert(entropy_step++ == 0); }
        void esp_fill_random(void *buffer, unsigned length) { assert(entropy_step++ == 1); memset(buffer, 0x5a, length); }
        void bootloader_random_disable(void) { assert(entropy_step++ == 2); }
        void mbedtls_x509_crt_init(mbedtls_x509_crt *certificate) { (void)certificate; }
        int mbedtls_x509_crt_parse(mbedtls_x509_crt *certificate, const unsigned char *data, size_t length) { (void)certificate; (void)length; return strstr((const char *)data, "BEGIN CERTIFICATE") ? 0 : -1; }
        void mbedtls_x509_crt_free(mbedtls_x509_crt *certificate) { (void)certificate; }

        static device_config_t valid(uint32_t revision) {
            device_config_t c = {0}; c.schema_version = 1; c.revision = revision;
            snprintf(c.device_id, sizeof(c.device_id), "edge-node-03"); snprintf(c.wifi_ssid, sizeof(c.wifi_ssid), "test-network");
            snprintf(c.wifi_password, sizeof(c.wifi_password), "wifi-secret"); snprintf(c.mqtt_broker_uri, sizeof(c.mqtt_broker_uri), "mqtts://broker.example:8883");
            snprintf(c.mqtt_ca_certificate, sizeof(c.mqtt_ca_certificate), "-----BEGIN CERTIFICATE-----\\ntest\\n-----END CERTIFICATE-----");
            snprintf(c.mqtt_username, sizeof(c.mqtt_username), "%s", c.device_id); snprintf(c.mqtt_password, sizeof(c.mqtt_password), "mqtt-secret");
            snprintf(c.mqtt_client_id, sizeof(c.mqtt_client_id), "iem-edge-node-03"); c.telemetry_interval_seconds = 5;
            c.machine_status_provider = DEVICE_CONFIG_MACHINE_GPIO; c.machine_status_gpio = 27; c.machine_status_active_high = true; c.machine_status_pull = DEVICE_CONFIG_PULL_NONE;
            c.maintenance_on_boot = true; c.maintenance_window_seconds = 60; c.maintenance_max_session_seconds = 120; return c;
        }
        static void assert_invalid(device_config_t configuration) {
            char error[128] = {0};
            assert(device_config_validate(&configuration, error, sizeof(error)) == ESP_ERR_INVALID_ARG);
            assert(error[0] != '\0');
        }
        int main(void) {
            assert(device_config_storage_init() == ESP_OK);
            device_config_t loaded; bool candidate = false; device_config_metadata_t metadata;
            assert(device_config_load_active(&loaded) == ESP_ERR_NVS_NOT_FOUND);
            device_config_t first = valid(1); assert(device_config_validate(&first, 0, 0) == ESP_OK);

            device_config_t invalid = first; invalid.schema_version = 99; assert_invalid(invalid);
            invalid = first; invalid.revision = 0; assert_invalid(invalid);
            invalid = first; snprintf(invalid.device_id, sizeof(invalid.device_id), "collector"); assert_invalid(invalid);
            invalid = first; snprintf(invalid.device_id, sizeof(invalid.device_id), "legacy-device"); assert_invalid(invalid);
            invalid = first; invalid.device_id[0] = (char)0xc3; invalid.device_id[1] = (char)0xa9; invalid.device_id[2] = '\0'; assert_invalid(invalid);
            invalid = first; invalid.wifi_ssid[0] = '\0'; assert_invalid(invalid);
            invalid = first; snprintf(invalid.wifi_password, sizeof(invalid.wifi_password), "short"); assert_invalid(invalid);
            invalid = first; snprintf(invalid.mqtt_broker_uri, sizeof(invalid.mqtt_broker_uri), "mqtt://broker.example:1883"); assert_invalid(invalid);
            invalid = first; snprintf(invalid.mqtt_broker_uri, sizeof(invalid.mqtt_broker_uri), "mqtts://broker.example"); assert_invalid(invalid);
            invalid = first; snprintf(invalid.mqtt_username, sizeof(invalid.mqtt_username), "other-device"); assert_invalid(invalid);
            invalid = first; invalid.mqtt_password[0] = '\0'; assert_invalid(invalid);
            invalid = first; snprintf(invalid.mqtt_client_id, sizeof(invalid.mqtt_client_id), "%s", invalid.device_id); assert_invalid(invalid);
            invalid = first; snprintf(invalid.mqtt_ca_certificate, sizeof(invalid.mqtt_ca_certificate), "not-a-certificate"); assert_invalid(invalid);
            invalid = first; invalid.telemetry_interval_seconds = 0; assert_invalid(invalid);
            invalid = first; invalid.machine_status_provider = (device_config_machine_provider_t)99; assert_invalid(invalid);
            invalid = first; invalid.machine_status_gpio = 0; assert_invalid(invalid);
            invalid = first; invalid.machine_status_gpio = 34; invalid.machine_status_pull = DEVICE_CONFIG_PULL_UP; assert_invalid(invalid);
            invalid = first; invalid.machine_status_pull = (device_config_pull_t)99; assert_invalid(invalid);
            invalid = first; invalid.maintenance_window_seconds = 59; assert_invalid(invalid);
            invalid = first; invalid.maintenance_max_session_seconds = 59; assert_invalid(invalid);

            assert(device_config_stage_candidate(&first) == ESP_OK);
            assert(device_config_load_for_boot(&loaded, &candidate, &metadata) == ESP_OK && candidate && loaded.revision == 1 && metadata.boot_attempts == 1);
            assert(device_config_candidate_validation_in_progress());
            device_config_t attempted_replacement = valid(2);
            assert(device_config_stage_candidate(&attempted_replacement) == ESP_ERR_INVALID_STATE);
            assert(device_config_cancel_candidate() == ESP_ERR_INVALID_STATE);
            slot_t candidate_backup = *find("candidate");
            device_config_t different_candidate = valid(99);
            slot_t *candidate_slot = find("candidate");
            assert(device_config_storage_encode_configuration(
                &different_candidate, candidate_slot->data, device_config_storage_configuration_size()) == ESP_OK);
            candidate_slot->size = device_config_storage_configuration_size();
            assert(device_config_activate_candidate(&loaded) == ESP_ERR_INVALID_STATE);
            *candidate_slot = candidate_backup;
            memset(candidate_slot, 0, sizeof(*candidate_slot));
            assert(device_config_activate_candidate(&loaded) == ESP_ERR_NVS_NOT_FOUND);
            *candidate_slot = candidate_backup;
            assert(device_config_activate_candidate(&loaded) == ESP_OK);
            assert(device_config_load_active(&loaded) == ESP_OK && loaded.revision == 1);
            device_config_redacted_t redacted; assert(device_config_get_redacted_snapshot(&loaded, &redacted) == ESP_OK);
            assert(redacted.wifi_password_configured && redacted.mqtt_password_configured && redacted.mqtt_ca_certificate_configured);
            slot_t metadata_backup = *find("metadata");
            find("metadata")->data[4] = 2;
            assert(device_config_get_metadata(&metadata) == ESP_ERR_NOT_SUPPORTED);
            *find("metadata") = metadata_backup;

            device_config_t second = valid(2); fail_commit = true;
            assert(device_config_stage_candidate(&second) == ESP_FAIL);
            assert(device_config_load_active(&loaded) == ESP_OK && loaded.revision == 1);

            assert(device_config_stage_candidate(&second) == ESP_OK);
            assert(device_config_load_for_boot(&loaded, &candidate, &metadata) == ESP_OK && candidate);
            assert(device_config_load_for_boot(&loaded, &candidate, &metadata) == ESP_OK && candidate);
            assert(device_config_load_for_boot(&loaded, &candidate, &metadata) == ESP_OK && !candidate && loaded.revision == 1);
            assert(metadata.state == DEVICE_CONFIG_STATE_ROLLBACK);

            device_config_t third = valid(3);
            commits_until_failure = 2; /* candidate blob committed, metadata transition interrupted */
            assert(device_config_stage_candidate(&third) == ESP_FAIL);
            assert(device_config_load_active(&loaded) == ESP_OK && loaded.revision == 1);
            assert(device_config_stage_candidate(&third) == ESP_OK);
            assert(device_config_load_for_boot(&loaded, &candidate, &metadata) == ESP_OK && candidate);
            device_config_t verified_third = loaded;
            commits_until_failure = 1; /* active replacement interrupted */
            assert(device_config_activate_candidate(&verified_third) == ESP_FAIL);
            assert(device_config_load_active(&loaded) == ESP_OK && loaded.revision == 1);
            commits_until_failure = 2; /* active committed, activation metadata interrupted */
            assert(device_config_activate_candidate(&verified_third) == ESP_FAIL);
            assert(device_config_load_active(&loaded) == ESP_OK && loaded.revision == 3);
            assert(device_config_get_metadata(&metadata) == ESP_OK && metadata.state == DEVICE_CONFIG_STATE_PENDING);
            assert(device_config_activate_candidate(&verified_third) == ESP_OK);
            assert(device_config_get_metadata(&metadata) == ESP_OK && metadata.state == DEVICE_CONFIG_STATE_ACTIVE);

            slot_t *active = find("active"); assert(active); active->data[12] = 99;
            assert(device_config_load_active(&loaded) == ESP_ERR_INVALID_ARG);
            active->data[4] = 2; active->data[5] = 0;
            assert(device_config_load_active(&loaded) == ESP_ERR_NOT_SUPPORTED);
            active->size = 3; assert(device_config_load_active(&loaded) == ESP_ERR_INVALID_SIZE);

            bool generated = false; char setup[DEVICE_CONFIG_SETUP_SECRET_LENGTH + 1];
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_OK && generated);
            assert(entropy_step == 3 && strcmp(setup, "5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a") == 0);
            entropy_step = 99;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_OK && !generated);
            assert(entropy_step == 99);

            slot_t valid_setup = *find("setup_secret");
            slot_t *setup_slot = find("setup_secret");
            memcpy(setup_slot->data, "abcd", 5); setup_slot->size = 5;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_ERR_INVALID_SIZE);
            assert(!generated && setup[0] == '\0' && entropy_step == 99 && find("setup_secret") != 0);
            *setup_slot = valid_setup;
            memset(setup_slot->data, 'a', 33); setup_slot->data[33] = '\0'; setup_slot->size = 34;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_ERR_INVALID_SIZE);
            assert(!generated && setup[0] == '\0' && entropy_step == 99);
            *setup_slot = valid_setup;
            setup_slot->data[7] = 'z';
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_ERR_INVALID_ARG);
            assert(!generated && setup[0] == '\0' && entropy_step == 99);
            *setup_slot = valid_setup;
            setup_get_error = ESP_ERR_NVS_TYPE_MISMATCH;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_ERR_NVS_TYPE_MISMATCH);
            assert(!generated && setup[0] == '\0' && find("setup_secret") != 0);
            setup_get_error = ESP_FAIL;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_FAIL);
            assert(!generated && setup[0] == '\0' && find("setup_secret") != 0);
            setup_get_error = ESP_OK;
            memset(setup_slot, 0, sizeof(*setup_slot));
            entropy_step = 0; fail_commit = true;
            assert(device_config_get_or_create_setup_secret(setup, &generated) == ESP_FAIL);
            assert(!generated && setup[0] == '\0' && entropy_step == 3 && find("setup_secret") == 0);
            flash_init_result = ESP_ERR_NVS_NEW_VERSION_FOUND;
            assert(device_config_storage_init() == ESP_ERR_NVS_NEW_VERSION_FOUND && flash_erases == 0);
            flash_init_result = ESP_OK;
            assert(device_config_storage_recover() == ESP_OK);
            assert(flash_erases == 1);
            assert(device_config_load_active(&loaded) == ESP_ERR_NVS_NOT_FOUND);
            return 0;
        }
        """,
        flags=["-pthread"],
    )


def test_softap_local_endpoint_ipv4_ipv6_and_fail_closed_socket_paths(tmp_path):
    component = REPOSITORY_ROOT / "firmware/components/provisioning_service"
    compile_and_run(
        tmp_path,
        "softap_endpoint_test",
        [component / "provisioning_softap_endpoint.c"],
        [component / "include"],
        r"""
        #include <assert.h>
        #include <arpa/inet.h>
        #include <stdint.h>
        #include <string.h>
        #include <sys/socket.h>
        #include <unistd.h>
        #include "provisioning_softap_endpoint.h"

        int main(void) {
            uint32_t ap = inet_addr("192.168.4.1");
            uint32_t sta = inet_addr("10.10.0.5");
            uint32_t mask = inet_addr("255.255.255.0");
            struct sockaddr_in ipv4 = {.sin_family = AF_INET};
            ipv4.sin_addr.s_addr = ap;
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv4, sizeof(ipv4), ap) == PROVISIONING_ENDPOINT_IPV4_SOFTAP);
            ipv4.sin_addr.s_addr = sta;
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv4, sizeof(ipv4), ap) == PROVISIONING_ENDPOINT_IPV4_OTHER);

            struct sockaddr_in6 ipv6 = {.sin6_family = AF_INET6};
            ipv6.sin6_addr.s6_addr[10] = 0xff; ipv6.sin6_addr.s6_addr[11] = 0xff;
            memcpy(&ipv6.sin6_addr.s6_addr[12], &ap, sizeof(ap));
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv6, sizeof(ipv6), ap) == PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP);
            memcpy(&ipv6.sin6_addr.s6_addr[12], &sta, sizeof(sta));
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv6, sizeof(ipv6), ap) == PROVISIONING_ENDPOINT_IPV4_MAPPED_OTHER);
            memset(&ipv6.sin6_addr, 0, sizeof(ipv6.sin6_addr)); ipv6.sin6_addr.s6_addr[15] = 1;
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv6, sizeof(ipv6), ap) == PROVISIONING_ENDPOINT_IPV6_NATIVE);
            assert(provisioning_softap_classify_local_endpoint((struct sockaddr *)&ipv6, sizeof(sa_family_t), ap) == PROVISIONING_ENDPOINT_TRUNCATED);
            struct sockaddr unexpected = {.sa_family = AF_UNIX};
            assert(provisioning_softap_classify_local_endpoint(&unexpected, sizeof(unexpected), ap) == PROVISIONING_ENDPOINT_UNEXPECTED_FAMILY);

            int family = 123;
            assert(provisioning_softap_authorize_socket(-1, false, ap, mask, &family) == PROVISIONING_ENDPOINT_NETIF_UNAVAILABLE);
            assert(provisioning_softap_authorize_socket(-1, true, ap, mask, &family) == PROVISIONING_ENDPOINT_INVALID_SOCKET);
            assert(provisioning_softap_authorize_socket(99999, true, ap, mask, &family) == PROVISIONING_ENDPOINT_GETSOCKNAME_FAILED);
            assert(!provisioning_softap_endpoint_is_authorized(PROVISIONING_ENDPOINT_IPV6_NATIVE));
            assert(provisioning_softap_endpoint_is_authorized(PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP));
            return 0;
        }
        """,
    )

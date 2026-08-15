#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool provisioning_security_constant_time_equal(const char *left, const char *right);
bool provisioning_security_json_body_allowed(const char *content_type, size_t length, size_t maximum);
bool provisioning_security_session_authorized(
    const char *stored_session,
    const char *request_session,
    const char *stored_csrf,
    const char *request_csrf,
    int64_t expires_ms,
    int64_t now_ms,
    bool modifying
);
bool provisioning_security_factory_confirmed(const char *confirmation);

typedef struct {
    bool completed;
} provisioning_async_lifecycle_t;

typedef struct {
    uint64_t next_worker_generation;
    uint64_t active_worker_generation;
    uint64_t completed_worker_generation;
    int active_socket;
    bool active;
    bool shutdown_requested;
    bool service_stopping;
} provisioning_stream_lifecycle_t;

bool provisioning_security_stream_session_valid(
    uint64_t stream_generation,
    uint64_t current_generation,
    int64_t expires_ms,
    int64_t now_ms,
    bool shutdown,
    bool service_stopping
);
bool provisioning_security_parse_stream_generation(const char *value, uint64_t *generation);
bool provisioning_security_stream_request_authorized(
    const char *requested_generation,
    uint64_t authenticated_generation,
    int64_t expires_ms,
    int64_t now_ms
);
bool provisioning_async_mark_complete(provisioning_async_lifecycle_t *lifecycle);

void provisioning_stream_lifecycle_init(provisioning_stream_lifecycle_t *lifecycle);
bool provisioning_stream_admit(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t *worker_generation
);
bool provisioning_stream_attach_socket(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation,
    int socket_fd
);
bool provisioning_stream_worker_current(
    const provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
);
uint64_t provisioning_stream_request_shutdown(
    provisioning_stream_lifecycle_t *lifecycle,
    bool service_stopping,
    int *socket_fd
);
bool provisioning_stream_complete(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
);
bool provisioning_stream_stop_satisfied(
    const provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
);

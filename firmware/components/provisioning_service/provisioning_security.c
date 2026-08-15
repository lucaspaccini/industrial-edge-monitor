#include "provisioning_security.h"

#include <limits.h>
#include <string.h>

bool provisioning_security_constant_time_equal(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    unsigned difference = (unsigned)(left_length ^ right_length);
    size_t maximum = left_length > right_length ? left_length : right_length;
    for (size_t index = 0; index < maximum; index++) {
        unsigned char a = index < left_length ? (unsigned char)left[index] : 0;
        unsigned char b = index < right_length ? (unsigned char)right[index] : 0;
        difference |= a ^ b;
    }
    return difference == 0;
}

bool provisioning_security_json_body_allowed(const char *content_type, size_t length, size_t maximum)
{
    return content_type != NULL
        && strncmp(content_type, "application/json", 16) == 0
        && (content_type[16] == '\0' || content_type[16] == ';')
        && length > 0
        && length <= maximum;
}

bool provisioning_security_session_authorized(
    const char *stored_session,
    const char *request_session,
    const char *stored_csrf,
    const char *request_csrf,
    int64_t expires_ms,
    int64_t now_ms,
    bool modifying
)
{
    if (stored_session == NULL || stored_session[0] == '\0' || request_session == NULL
        || now_ms >= expires_ms
        || !provisioning_security_constant_time_equal(stored_session, request_session)) {
        return false;
    }
    return !modifying || (request_csrf != NULL && stored_csrf != NULL
        && provisioning_security_constant_time_equal(stored_csrf, request_csrf));
}

bool provisioning_security_factory_confirmed(const char *confirmation)
{
    return confirmation != NULL
        && provisioning_security_constant_time_equal(
            confirmation,
            "ERASE-DEVICE-CONFIGURATION"
        );
}

bool provisioning_security_stream_session_valid(
    uint64_t stream_generation,
    uint64_t current_generation,
    int64_t expires_ms,
    int64_t now_ms,
    bool shutdown,
    bool service_stopping
)
{
    return !shutdown && !service_stopping
        && stream_generation != 0
        && stream_generation == current_generation
        && now_ms < expires_ms;
}

bool provisioning_security_parse_stream_generation(const char *value, uint64_t *generation)
{
    if (value == NULL || generation == NULL || value[0] == '\0') return false;
    uint64_t parsed = 0;
    for (size_t index = 0; value[index] != '\0'; index++) {
        unsigned char character = (unsigned char)value[index];
        if (character < '0' || character > '9') return false;
        uint64_t digit = (uint64_t)(character - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0) return false;
    *generation = parsed;
    return true;
}

bool provisioning_security_stream_request_authorized(
    const char *requested_generation,
    uint64_t authenticated_generation,
    int64_t expires_ms,
    int64_t now_ms
)
{
    uint64_t parsed = 0;
    return authenticated_generation != 0
        && now_ms < expires_ms
        && provisioning_security_parse_stream_generation(requested_generation, &parsed)
        && parsed == authenticated_generation;
}

bool provisioning_async_mark_complete(provisioning_async_lifecycle_t *lifecycle)
{
    if (lifecycle == NULL || lifecycle->completed) return false;
    lifecycle->completed = true;
    return true;
}

void provisioning_stream_lifecycle_init(provisioning_stream_lifecycle_t *lifecycle)
{
    if (lifecycle == NULL) return;
    memset(lifecycle, 0, sizeof(*lifecycle));
    lifecycle->active_socket = -1;
}

bool provisioning_stream_admit(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t *worker_generation
)
{
    if (lifecycle == NULL || worker_generation == NULL
        || lifecycle->active || lifecycle->service_stopping) {
        return false;
    }
    lifecycle->next_worker_generation++;
    if (lifecycle->next_worker_generation == 0) lifecycle->next_worker_generation = 1;
    lifecycle->active_worker_generation = lifecycle->next_worker_generation;
    lifecycle->active_socket = -1;
    lifecycle->active = true;
    lifecycle->shutdown_requested = false;
    *worker_generation = lifecycle->active_worker_generation;
    return true;
}

bool provisioning_stream_attach_socket(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation,
    int socket_fd
)
{
    if (lifecycle == NULL || socket_fd < 0 || !lifecycle->active
        || lifecycle->active_worker_generation != worker_generation) {
        return false;
    }
    lifecycle->active_socket = socket_fd;
    return !lifecycle->shutdown_requested && !lifecycle->service_stopping;
}

bool provisioning_stream_worker_current(
    const provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
)
{
    return lifecycle != NULL && lifecycle->active
        && lifecycle->active_worker_generation == worker_generation
        && !lifecycle->shutdown_requested && !lifecycle->service_stopping;
}

uint64_t provisioning_stream_request_shutdown(
    provisioning_stream_lifecycle_t *lifecycle,
    bool service_stopping,
    int *socket_fd
)
{
    if (socket_fd != NULL) *socket_fd = -1;
    if (lifecycle == NULL) return 0;
    if (service_stopping) lifecycle->service_stopping = true;
    if (!lifecycle->active) return 0;
    lifecycle->shutdown_requested = true;
    if (socket_fd != NULL) *socket_fd = lifecycle->active_socket;
    return lifecycle->active_worker_generation;
}

bool provisioning_stream_complete(
    provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
)
{
    if (lifecycle == NULL || !lifecycle->active
        || lifecycle->active_worker_generation != worker_generation) {
        return false;
    }
    lifecycle->completed_worker_generation = worker_generation;
    lifecycle->active_worker_generation = 0;
    lifecycle->active_socket = -1;
    lifecycle->active = false;
    lifecycle->shutdown_requested = false;
    return true;
}

bool provisioning_stream_stop_satisfied(
    const provisioning_stream_lifecycle_t *lifecycle,
    uint64_t worker_generation
)
{
    if (lifecycle == NULL || worker_generation == 0) return true;
    return lifecycle->completed_worker_generation >= worker_generation
        && (!lifecycle->active || lifecycle->active_worker_generation != worker_generation);
}

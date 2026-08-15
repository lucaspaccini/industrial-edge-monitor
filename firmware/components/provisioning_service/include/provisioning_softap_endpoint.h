#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

typedef enum {
    PROVISIONING_ENDPOINT_IPV4_SOFTAP = 0,
    PROVISIONING_ENDPOINT_IPV4_OTHER,
    PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP,
    PROVISIONING_ENDPOINT_IPV4_MAPPED_OTHER,
    PROVISIONING_ENDPOINT_IPV6_NATIVE,
    PROVISIONING_ENDPOINT_NETIF_UNAVAILABLE,
    PROVISIONING_ENDPOINT_INVALID_SOCKET,
    PROVISIONING_ENDPOINT_GETSOCKNAME_FAILED,
    PROVISIONING_ENDPOINT_TRUNCATED,
    PROVISIONING_ENDPOINT_UNEXPECTED_FAMILY,
} provisioning_endpoint_class_t;

provisioning_endpoint_class_t provisioning_softap_classify_local_endpoint(
    const struct sockaddr *local,
    socklen_t length,
    uint32_t softap_ipv4
);
provisioning_endpoint_class_t provisioning_softap_authorize_socket(
    int socket_fd,
    bool softap_ready,
    uint32_t softap_ipv4,
    uint32_t softap_netmask,
    int *family
);
bool provisioning_softap_endpoint_is_authorized(provisioning_endpoint_class_t classification);
const char *provisioning_softap_endpoint_class_name(provisioning_endpoint_class_t classification);

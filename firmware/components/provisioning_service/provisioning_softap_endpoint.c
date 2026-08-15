#include "provisioning_softap_endpoint.h"

#include <netinet/in.h>
#include <stddef.h>
#include <string.h>

provisioning_endpoint_class_t provisioning_softap_classify_local_endpoint(
    const struct sockaddr *local,
    socklen_t length,
    uint32_t softap_ipv4
)
{
    if (local == NULL || length < sizeof(sa_family_t)) {
        return PROVISIONING_ENDPOINT_TRUNCATED;
    }
    if (local->sa_family == AF_INET) {
        if (length < sizeof(struct sockaddr_in)) return PROVISIONING_ENDPOINT_TRUNCATED;
        const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)local;
        return ipv4->sin_addr.s_addr == softap_ipv4
            ? PROVISIONING_ENDPOINT_IPV4_SOFTAP
            : PROVISIONING_ENDPOINT_IPV4_OTHER;
    }
    if (local->sa_family == AF_INET6) {
        if (length < sizeof(struct sockaddr_in6)) return PROVISIONING_ENDPOINT_TRUNCATED;
        const struct sockaddr_in6 *ipv6 = (const struct sockaddr_in6 *)local;
        const uint8_t mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
        if (memcmp(ipv6->sin6_addr.s6_addr, mapped_prefix, sizeof(mapped_prefix)) != 0) {
            return PROVISIONING_ENDPOINT_IPV6_NATIVE;
        }
        uint32_t mapped_ipv4;
        memcpy(&mapped_ipv4, &ipv6->sin6_addr.s6_addr[12], sizeof(mapped_ipv4));
        return mapped_ipv4 == softap_ipv4
            ? PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP
            : PROVISIONING_ENDPOINT_IPV4_MAPPED_OTHER;
    }
    return PROVISIONING_ENDPOINT_UNEXPECTED_FAMILY;
}

provisioning_endpoint_class_t provisioning_softap_authorize_socket(
    int socket_fd,
    bool softap_ready,
    uint32_t softap_ipv4,
    uint32_t softap_netmask,
    int *family
)
{
    if (family != NULL) *family = AF_UNSPEC;
    if (!softap_ready || softap_ipv4 == 0 || softap_netmask == 0) {
        return PROVISIONING_ENDPOINT_NETIF_UNAVAILABLE;
    }
    if (socket_fd < 0) return PROVISIONING_ENDPOINT_INVALID_SOCKET;
    struct sockaddr_storage local = {0};
    socklen_t length = sizeof(local);
    if (getsockname(socket_fd, (struct sockaddr *)&local, &length) != 0) {
        return PROVISIONING_ENDPOINT_GETSOCKNAME_FAILED;
    }
    if (family != NULL && length >= sizeof(sa_family_t)) {
        *family = ((const struct sockaddr *)&local)->sa_family;
    }
    if (length > sizeof(local)) return PROVISIONING_ENDPOINT_TRUNCATED;
    return provisioning_softap_classify_local_endpoint(
        (const struct sockaddr *)&local,
        length,
        softap_ipv4
    );
}

bool provisioning_softap_endpoint_is_authorized(provisioning_endpoint_class_t classification)
{
    return classification == PROVISIONING_ENDPOINT_IPV4_SOFTAP
        || classification == PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP;
}

const char *provisioning_softap_endpoint_class_name(provisioning_endpoint_class_t classification)
{
    switch (classification) {
        case PROVISIONING_ENDPOINT_IPV4_SOFTAP: return "ipv4-softap";
        case PROVISIONING_ENDPOINT_IPV4_OTHER: return "ipv4-other";
        case PROVISIONING_ENDPOINT_IPV4_MAPPED_SOFTAP: return "ipv4-mapped-softap";
        case PROVISIONING_ENDPOINT_IPV4_MAPPED_OTHER: return "ipv4-mapped-other";
        case PROVISIONING_ENDPOINT_IPV6_NATIVE: return "ipv6-native";
        case PROVISIONING_ENDPOINT_NETIF_UNAVAILABLE: return "softap-netif-unavailable";
        case PROVISIONING_ENDPOINT_INVALID_SOCKET: return "invalid-socket";
        case PROVISIONING_ENDPOINT_GETSOCKNAME_FAILED: return "getsockname-failed";
        case PROVISIONING_ENDPOINT_TRUNCATED: return "sockaddr-truncated";
        case PROVISIONING_ENDPOINT_UNEXPECTED_FAMILY: return "unexpected-family";
        default: return "unknown";
    }
}

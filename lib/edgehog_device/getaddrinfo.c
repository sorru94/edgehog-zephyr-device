/*
 * (C) Copyright 2024, SECO Mind Srl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "getaddrinfo.h"

#include <zephyr/net/socket_offload.h>

#include "heap.h"

/************************************************
 *        Defines, constants and typedef        *
 ***********************************************/

#if defined(CONFIG_DNS_RESOLVER) || defined(CONFIG_NET_IP)
#define ANY_RESOLVER

#if defined(CONFIG_DNS_RESOLVER_AI_MAX_ENTRIES)
#define AI_ARR_MAX CONFIG_DNS_RESOLVER_AI_MAX_ENTRIES
#else
#define AI_ARR_MAX 1
#endif /* defined(CONFIG_DNS_RESOLVER_AI_MAX_ENTRIES) */

/* Initialize static fields of addrinfo structure. A macro to let it work
 * with any sockaddr_* type.
 */
#define INIT_ADDRINFO(addrinfo, sockaddr)                                                          \
    {                                                                                              \
        (addrinfo)->ai_addr = &(addrinfo)->_ai_addr;                                               \
        (addrinfo)->ai_addrlen = sizeof(*(sockaddr));                                              \
        (addrinfo)->ai_canonname = (addrinfo)->_ai_canonname;                                      \
        (addrinfo)->_ai_canonname[0] = '\0';                                                       \
        (addrinfo)->ai_next = NULL;                                                                \
    }

#endif

/************************************************
 *         Static functions declaration         *
 ***********************************************/

#if defined(CONFIG_NET_IP)
static int try_resolve_literal_addr(const char *host, const char *service,
    const struct zsock_addrinfo *hints, struct zsock_addrinfo *res);
#endif /* CONFIG_NET_IP */

/************************************************
 *         Global functions definitions         *
 ***********************************************/

int edgehog_getaddrinfo(const char *host, const char *service, const struct zsock_addrinfo *hints,
    struct zsock_addrinfo **res)
{
    if (IS_ENABLED(CONFIG_NET_SOCKETS_OFFLOAD)) {
        return socket_offload_getaddrinfo(host, service, hints, res);
    }

    int ret = DNS_EAI_FAIL;

#if defined(ANY_RESOLVER)
    *res = edgehog_calloc(AI_ARR_MAX, sizeof(struct zsock_addrinfo));
    if (!(*res)) {
        return DNS_EAI_MEMORY;
    }
#endif

#if defined(CONFIG_NET_IP)
    /* Resolve literal address even if DNS is not available */
    if (ret) {
        ret = try_resolve_literal_addr(host, service, hints, *res);
    }
#endif

#if defined(CONFIG_DNS_RESOLVER)
    if (ret) {
        ret = z_zsock_getaddrinfo_internal(host, service, hints, *res);
    }
#endif

#if defined(ANY_RESOLVER)
    if (ret) {
        edgehog_free(*res);
        *res = NULL;
    }
#endif

    return ret;
}

// NOLINTNEXTLINE(readability-identifier-length)
void edgehog_freeaddrinfo(struct zsock_addrinfo *ai)
{
    if (IS_ENABLED(CONFIG_NET_SOCKETS_OFFLOAD)) {
        return socket_offload_freeaddrinfo(ai);
    }

    edgehog_free(ai);
}

/************************************************
 *         Static functions definitions         *
 ***********************************************/

// NOLINTBEGIN: This function is pretty much identical to the one contained in Zephyr upstream.
#if defined(CONFIG_NET_IP)
static int try_resolve_literal_addr(const char *host, const char *service,
    const struct zsock_addrinfo *hints, struct zsock_addrinfo *res)
{
    int family = AF_UNSPEC;
    int resolved_family = AF_UNSPEC;
    long port = 0;
    bool result;
    int socktype = SOCK_STREAM;
    int protocol = IPPROTO_TCP;

    if (!host) {
        return DNS_EAI_NONAME;
    }

    if (hints) {
        family = hints->ai_family;
        if (hints->ai_socktype == SOCK_DGRAM) {
            socktype = SOCK_DGRAM;
            protocol = IPPROTO_UDP;
        }
    }

    result = net_ipaddr_parse(host, strlen(host), &res->_ai_addr);

    if (!result) {
        return DNS_EAI_NONAME;
    }

    resolved_family = res->_ai_addr.sa_family;

    if ((family != AF_UNSPEC) && (resolved_family != family)) {
        return DNS_EAI_NONAME;
    }

    if (service) {
        port = strtol(service, NULL, 10);
        if (port < 1 || port > 65535) {
            return DNS_EAI_NONAME;
        }
    }

    res->ai_family = resolved_family;
    res->ai_socktype = socktype;
    res->ai_protocol = protocol;

    switch (resolved_family) {
        case AF_INET: {
            struct sockaddr_in *addr = (struct sockaddr_in *) &res->_ai_addr;

            INIT_ADDRINFO(res, addr);
            addr->sin_port = htons(port);
            addr->sin_family = AF_INET;
            break;
        }

        case AF_INET6: {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &res->_ai_addr;

            INIT_ADDRINFO(res, addr);
            addr->sin6_port = htons(port);
            addr->sin6_family = AF_INET6;
            break;
        }

        default:
            return DNS_EAI_NONAME;
    }

    return 0;
}
#endif /* CONFIG_NET_IP */
// NOLINTEND

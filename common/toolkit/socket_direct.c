/**
 * @file
 *
 * STUN candidate discovery, UDP hole punching, and rendezvous signaling.
 */

#include "socket_private.h"
#include "string.h"
#include "datetime.h"

#include <curl/curl.h>
#include <openssl/rand.h>
#ifdef WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#endif
#define SOCKET_STUN_MAGIC 0x2112a442U
#define SOCKET_PUNCH_PROBE "ATRINIK-PUNCH-1"
typedef struct socket_punch_job {
    socket_direct_candidate_t candidate;
    socket_punch_pacer_t pacer;
} socket_punch_job_t;

typedef struct socket_candidate_kind_info {
    const char *name;
    socket_connection_mode_t mode;
    double timeout;
} socket_candidate_kind_info_t;

static const socket_candidate_kind_info_t socket_candidate_kinds[SOCKET_CANDIDATE_NUM] = {
    [SOCKET_CANDIDATE_LAN] = {"lan", SOCKET_CONNECTION_MODE_QUIC_LAN, 1.0},
    [SOCKET_CANDIDATE_IPV6] = {"ipv6", SOCKET_CONNECTION_MODE_QUIC_IPV6, 2.0},
    [SOCKET_CANDIDATE_PRFLX] = {"prflx", SOCKET_CONNECTION_MODE_QUIC_SRFLX, 3.0},
    [SOCKET_CANDIDATE_MAPPED] = {"mapped", SOCKET_CONNECTION_MODE_QUIC_MAPPED, 5.0},
    [SOCKET_CANDIDATE_SRFLX] = {"srflx", SOCKET_CONNECTION_MODE_QUIC_SRFLX, 5.0},
    [SOCKET_CANDIDATE_DIRECTORY] = {"directory", SOCKET_CONNECTION_MODE_QUIC_DIRECTORY, 5.0},
};

const char *socket_candidate_kind_name(socket_candidate_kind_t kind) {
    if ((unsigned int)kind >= SOCKET_CANDIDATE_NUM) {
        return "unknown";
    }
    return socket_candidate_kinds[kind].name;
}

bool socket_candidate_kind_parse(const char *name, socket_candidate_kind_t *kind) {
    HARD_ASSERT(name != NULL);
    HARD_ASSERT(kind != NULL);

    for (socket_candidate_kind_t i = 0; i < SOCKET_CANDIDATE_NUM; i++) {
        if (strcmp(name, socket_candidate_kinds[i].name) == 0) {
            *kind = i;
            return true;
        }
    }
    return false;
}

socket_connection_mode_t socket_candidate_kind_mode(socket_candidate_kind_t kind) {
    return (unsigned int)kind < SOCKET_CANDIDATE_NUM ? socket_candidate_kinds[kind].mode
                                                     : SOCKET_CONNECTION_MODE_QUIC;
}

double socket_candidate_kind_timeout(socket_candidate_kind_t kind) {
    return (unsigned int)kind < SOCKET_CANDIDATE_NUM ? socket_candidate_kinds[kind].timeout : 5.0;
}

static bool socket_rendezvous_ticket_valid(const char *ticket) {
    return string_is_hex_fixed(ticket, 64, true);
}

static bool socket_rendezvous_host_valid(const char *host) {
    struct in_addr address4;
    if (host != NULL && inet_pton(AF_INET, host, &address4) == 1) {
        return true;
    }
#ifdef HAVE_IPV6
    struct in6_addr address6;
    return host != NULL && inet_pton(AF_INET6, host, &address6) == 1;
#else
    return false;
#endif
}

bool socket_rendezvous_client_candidate_parse(const char *message,
                                              char *host,
                                              size_t host_size,
                                              uint16_t *port,
                                              char ticket[65]) {
    char parsed_host[65], parsed_ticket[65];
    unsigned int parsed_port;
    int consumed = 0;
    if (message == NULL || host == NULL || host_size == 0 || port == NULL || ticket == NULL ||
        sscanf(message,
               "{\"type\":\"client_candidate\",\"host\":\"%64[0-9a-fA-F:.]\","
               "\"port\":%u,\"ticket\":\"%64[0-9a-f]\"}%n",
               parsed_host,
               &parsed_port,
               parsed_ticket,
               &consumed) != 3 ||
        message[consumed] != '\0' || parsed_port == 0 || parsed_port > UINT16_MAX ||
        !socket_rendezvous_host_valid(parsed_host) ||
        !socket_rendezvous_ticket_valid(parsed_ticket) || strlen(parsed_host) >= host_size) {
        return false;
    }
    snprintf(host, host_size, "%s", parsed_host);
    snprintf(ticket, 65, "%s", parsed_ticket);
    *port = (uint16_t)parsed_port;
    return true;
}

bool socket_rendezvous_server_candidate_parse(const char *message,
                                              const char *expected_ticket,
                                              socket_direct_candidate_t *candidate) {
    char host[65], kind[16], ticket[65];
    unsigned int port;
    int consumed = 0;
    socket_candidate_kind_t parsed_kind;
    if (message == NULL || expected_ticket == NULL || candidate == NULL ||
        !socket_rendezvous_ticket_valid(expected_ticket) ||
        sscanf(message,
               "{\"type\":\"server_candidate\",\"host\":\"%64[0-9a-fA-F:.]\","
               "\"port\":%u,\"kind\":\"%15[a-z0-9]\","
               "\"ticket\":\"%64[0-9a-f]\"}%n",
               host,
               &port,
               kind,
               ticket,
               &consumed) != 4 ||
        message[consumed] != '\0' || port == 0 || port > UINT16_MAX ||
        strcmp(ticket, expected_ticket) != 0 || !socket_rendezvous_host_valid(host) ||
        !socket_candidate_kind_parse(kind, &parsed_kind)) {
        return false;
    }
    snprintf(VS(candidate->host), "%s", host);
    candidate->port = (uint16_t)port;
    candidate->kind = parsed_kind;
    return true;
}

bool socket_rendezvous_message_render(char *buffer,
                                      size_t size,
                                      const char *type,
                                      const char *host,
                                      uint16_t port,
                                      socket_candidate_kind_t kind,
                                      const char *ticket) {
    if (buffer == NULL || size == 0 || type == NULL || !socket_rendezvous_ticket_valid(ticket)) {
        return false;
    }
    int length;
    if (strcmp(type, "complete") == 0) {
        length = snprintf(buffer, size, "{\"type\":\"complete\",\"ticket\":\"%s\"}", ticket);
    } else if (strcmp(type, "client_candidate") == 0 && socket_rendezvous_host_valid(host)) {
        length = snprintf(buffer,
                          size,
                          "{\"type\":\"client_candidate\",\"host\":\"%s\","
                          "\"port\":%" PRIu16 ",\"ticket\":\"%s\"}",
                          host,
                          port,
                          ticket);
    } else if (strcmp(type, "server_candidate") == 0 && socket_rendezvous_host_valid(host) &&
               (unsigned int)kind < SOCKET_CANDIDATE_NUM) {
        length = snprintf(buffer,
                          size,
                          "{\"type\":\"server_candidate\",\"host\":\"%s\","
                          "\"port\":%" PRIu16 ",\"kind\":\"%s\","
                          "\"ticket\":\"%s\"}",
                          host,
                          port,
                          socket_candidate_kind_name(kind),
                          ticket);
    } else {
        return false;
    }
    return length >= 0 && (size_t)length < size;
}

bool socket_rendezvous_complete_parse(const char *message, const char *expected_ticket) {
    char expected[128];
    return message != NULL &&
           socket_rendezvous_message_render(VS(expected),
                                            "complete",
                                            NULL,
                                            0,
                                            SOCKET_CANDIDATE_NUM,
                                            expected_ticket) &&
           strcmp(message, expected) == 0;
}

static bool socket_candidate_add(socket_direct_candidate_t *candidates,
                                 size_t *count,
                                 size_t capacity,
                                 const char *host,
                                 uint16_t port,
                                 socket_candidate_kind_t kind) {
    for (size_t i = 0; i < *count; i++) {
        if (candidates[i].port == port && strcmp(candidates[i].host, host) == 0) {
            return true;
        }
    }
    if (*count >= capacity) {
        return false;
    }

    snprintf(VS(candidates[*count].host), "%s", host);
    candidates[*count].port = port;
    candidates[*count].kind = kind;
    (*count)++;
    return true;
}

static bool socket_candidate_address_valid(const struct sockaddr *address) {
    if (address->sa_family == AF_INET) {
        const struct sockaddr_in *address4 = (const struct sockaddr_in *)address;
        uint32_t value = ntohl(address4->sin_addr.s_addr);
        return value != INADDR_ANY && (value >> 24) != 127 && (value & 0xf0000000U) != 0xe0000000U;
    }
#ifdef HAVE_IPV6
    if (address->sa_family == AF_INET6) {
        const struct sockaddr_in6 *address6 = (const struct sockaddr_in6 *)address;
        return !IN6_IS_ADDR_UNSPECIFIED(&address6->sin6_addr) &&
               !IN6_IS_ADDR_LOOPBACK(&address6->sin6_addr) &&
               !IN6_IS_ADDR_LINKLOCAL(&address6->sin6_addr) &&
               !IN6_IS_ADDR_MULTICAST(&address6->sin6_addr);
    }
#endif
    return false;
}

static bool socket_candidate_address_global(const struct sockaddr *address) {
    if (address->sa_family == AF_INET) {
        const struct sockaddr_in *address4 = (const struct sockaddr_in *)address;
        uint32_t value = ntohl(address4->sin_addr.s_addr);
        return (value & 0xff000000U) != 0 && (value & 0xff000000U) != 0x0a000000U &&
               (value & 0xfff00000U) != 0xac100000U && (value & 0xffff0000U) != 0xc0a80000U &&
               (value & 0xffc00000U) != 0x64400000U && (value & 0xffff0000U) != 0xa9fe0000U &&
               (value & 0xff000000U) != 0x7f000000U && value < 0xe0000000U;
    }
#ifdef HAVE_IPV6
    if (address->sa_family == AF_INET6) {
        const struct sockaddr_in6 *address6 = (const struct sockaddr_in6 *)address;
        return (address6->sin6_addr.s6_addr[0] & 0xe0) == 0x20;
    }
#endif
    return false;
}

bool socket_host_is_global(const char *host) {
    HARD_ASSERT(host != NULL);

    struct sockaddr_storage address;
    memset(&address, 0, sizeof(address));
    struct sockaddr_in *address4 = (struct sockaddr_in *)&address;
    if (inet_pton(AF_INET, host, &address4->sin_addr) == 1) {
        address4->sin_family = AF_INET;
        return socket_candidate_address_global((const struct sockaddr *)&address);
    }
#ifdef HAVE_IPV6
    struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)&address;
    if (inet_pton(AF_INET6, host, &address6->sin6_addr) == 1) {
        address6->sin6_family = AF_INET6;
        return socket_candidate_address_global((const struct sockaddr *)&address);
    }
#endif
    return false;
}

size_t
socket_local_candidates(uint16_t port, socket_direct_candidate_t *candidates, size_t capacity) {
    HARD_ASSERT(candidates != NULL || capacity == 0);

    size_t count = 0;
#ifdef WIN32
    ULONG size = 16 * 1024;
    IP_ADAPTER_ADDRESSES *adapters = emalloc(size);
    ULONG rc = GetAdaptersAddresses(AF_UNSPEC,
                                    GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                        GAA_FLAG_SKIP_DNS_SERVER,
                                    NULL,
                                    adapters,
                                    &size);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        adapters = erealloc(adapters, size);
        rc = GetAdaptersAddresses(AF_UNSPEC,
                                  GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                      GAA_FLAG_SKIP_DNS_SERVER,
                                  NULL,
                                  adapters,
                                  &size);
    }
    if (rc == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter != NULL; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp) {
                continue;
            }
            for (IP_ADAPTER_UNICAST_ADDRESS *entry = adapter->FirstUnicastAddress; entry != NULL;
                 entry = entry->Next) {
                const struct sockaddr *address = entry->Address.lpSockaddr;
                if (address == NULL || !socket_candidate_address_valid(address)) {
                    continue;
                }
                char host[65];
                if (getnameinfo(address,
                                entry->Address.iSockaddrLength,
                                VS(host),
                                NULL,
                                0,
                                NI_NUMERICHOST) == 0) {
                    socket_candidate_add(candidates,
                                         &count,
                                         capacity,
                                         host,
                                         port,
                                         socket_candidate_address_global(address)
                                             ? SOCKET_CANDIDATE_IPV6
                                             : SOCKET_CANDIDATE_LAN);
                }
            }
        }
    }
    efree(adapters);
#else
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) {
        return 0;
    }
    for (struct ifaddrs *entry = interfaces; entry != NULL; entry = entry->ifa_next) {
        if (entry->ifa_addr == NULL || (entry->ifa_flags & IFF_UP) == 0 ||
            (entry->ifa_flags & IFF_LOOPBACK) != 0 ||
            !socket_candidate_address_valid(entry->ifa_addr)) {
            continue;
        }

        socklen_t address_length = entry->ifa_addr->sa_family == AF_INET
                                       ? sizeof(struct sockaddr_in)
                                       : sizeof(struct sockaddr_in6);
        char host[65];
        if (getnameinfo(entry->ifa_addr, address_length, VS(host), NULL, 0, NI_NUMERICHOST) == 0) {
            socket_candidate_add(candidates,
                                 &count,
                                 capacity,
                                 host,
                                 port,
                                 socket_candidate_address_global(entry->ifa_addr)
                                     ? SOCKET_CANDIDATE_IPV6
                                     : SOCKET_CANDIDATE_LAN);
        }
    }
    freeifaddrs(interfaces);
#endif
    return count;
}

static uint16_t socket_stun_u16(const unsigned char *b) {
    return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static uint32_t socket_stun_u32(const unsigned char *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

bool socket_stun_discover(socket_t *sc,
                          const char *endpoint,
                          char *host,
                          size_t host_size,
                          uint16_t *port) {
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(endpoint != NULL);
    HARD_ASSERT(host != NULL);
    HARD_ASSERT(port != NULL);

    const char *separator = strrchr(endpoint, ':');
    if (separator == NULL || separator == endpoint || separator[1] == '\0' ||
        strlen(separator + 1) >= 6) {
        LOG(ERROR, "Invalid STUN endpoint: %s", endpoint);
        return false;
    }
    char stun_host[MAX_BUF], stun_port[6];
    size_t stun_host_length = (size_t)(separator - endpoint);
    if (stun_host_length >= sizeof(stun_host)) {
        return false;
    }
    memcpy(stun_host, endpoint, stun_host_length);
    stun_host[stun_host_length] = '\0';
    snprintf(VS(stun_port), "%s", separator + 1);

    struct addrinfo hints, *addresses = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ((struct sockaddr *)&sc->addr)->sa_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV;
    int rc = getaddrinfo(stun_host, stun_port, &hints, &addresses);
    if (rc != 0) {
        LOG(ERROR, "Cannot resolve STUN endpoint %s: %s", endpoint, gai_strerror(rc));
        return false;
    }

    unsigned char request[20] = {0};
    request[1] = 1;
    request[4] = 0x21;
    request[5] = 0x12;
    request[6] = 0xa4;
    request[7] = 0x42;
    if (RAND_bytes(request + 8, 12) != 1) {
        freeaddrinfo(addresses);
        return false;
    }

    bool sent = false;
    for (struct addrinfo *ai = addresses; ai != NULL; ai = ai->ai_next) {
        if (sendto(sc->handle,
                   (const char *)request,
                   sizeof(request),
                   0,
                   ai->ai_addr,
                   ai->ai_addrlen) == (ssize_t)sizeof(request)) {
            sent = true;
            break;
        }
    }
    freeaddrinfo(addresses);
    if (!sent) {
        LOG(ERROR, "Failed to send STUN request to %s", endpoint);
        return false;
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sc->handle, &readfds);
    struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    if (select(sc->handle + 1, &readfds, NULL, NULL, &timeout) != 1) {
        LOG(ERROR, "STUN request to %s timed out", endpoint);
        return false;
    }

    unsigned char response[1024];
    ssize_t length = recvfrom(sc->handle, (char *)response, sizeof(response), 0, NULL, NULL);
    if (length < 20 || socket_stun_u16(response) != 0x0101 ||
        socket_stun_u32(response + 4) != SOCKET_STUN_MAGIC ||
        memcmp(response + 8, request + 8, 12) != 0) {
        LOG(ERROR, "Invalid STUN response from %s", endpoint);
        return false;
    }

    size_t message_length = socket_stun_u16(response + 2);
    if (message_length > (size_t)length - 20) {
        return false;
    }
    for (size_t offset = 20; offset + 4 <= 20 + message_length;) {
        uint16_t type = socket_stun_u16(response + offset);
        size_t value_length = socket_stun_u16(response + offset + 2);
        const unsigned char *value = response + offset + 4;
        if (offset + 4 + value_length > (size_t)length) {
            return false;
        }
        if (type == 0x0020 && value_length >= 8) {
            int family = value[1];
            *port = socket_stun_u16(value + 2) ^ (uint16_t)(SOCKET_STUN_MAGIC >> 16);
            unsigned char address[16];
            size_t address_length = family == 1 ? 4 : family == 2 ? 16 : 0;
            if (address_length == 0 || value_length < 4 + address_length) {
                return false;
            }
            unsigned char mask[16] = {0x21, 0x12, 0xa4, 0x42};
            memcpy(mask + 4, request + 8, 12);
            for (size_t i = 0; i < address_length; i++) {
                address[i] = value[4 + i] ^ mask[i];
            }
            return inet_ntop(family == 1 ? AF_INET : AF_INET6, address, host, host_size) != NULL;
        }
        offset += 4 + ((value_length + 3) & ~(size_t)3);
    }

    LOG(ERROR, "STUN response did not contain XOR-MAPPED-ADDRESS");
    return false;
}

bool socket_udp_punch(socket_t *sc, const char *host, uint16_t port) {
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(host != NULL);

    char port_string[6];
    snprintf(VS(port_string), "%" PRIu16, port);
    struct addrinfo hints, *addresses = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ((struct sockaddr *)&sc->addr)->sa_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV;
    if (getaddrinfo(host, port_string, &hints, &addresses) != 0) {
        return false;
    }

    static const char probe[] = SOCKET_PUNCH_PROBE;
    bool ok = false;
    for (struct addrinfo *ai = addresses; ai != NULL; ai = ai->ai_next) {
        if (sendto(sc->handle, probe, sizeof(probe) - 1, 0, ai->ai_addr, ai->ai_addrlen) ==
            (ssize_t)(sizeof(probe) - 1)) {
            ok = true;
        }
    }
    freeaddrinfo(addresses);
    return ok;
}

bool socket_udp_punch_receive(socket_t *sc, char *host, size_t host_size, uint16_t *port) {
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(host != NULL);
    HARD_ASSERT(port != NULL);

    char datagram[UINT16_MAX];
    struct sockaddr_storage source;
    socklen_t source_length = sizeof(source);
    ssize_t length = recvfrom(sc->handle,
                              datagram,
                              sizeof(datagram),
                              MSG_PEEK,
                              (struct sockaddr *)&source,
                              &source_length);
    if ((size_t)length != sizeof(SOCKET_PUNCH_PROBE) - 1 ||
        memcmp(datagram, SOCKET_PUNCH_PROBE, sizeof(SOCKET_PUNCH_PROBE) - 1) != 0) {
        return false;
    }

    char probe[sizeof(SOCKET_PUNCH_PROBE)];
    source_length = sizeof(source);
    length =
        recvfrom(sc->handle, probe, sizeof(probe), 0, (struct sockaddr *)&source, &source_length);
    if ((size_t)length != sizeof(SOCKET_PUNCH_PROBE) - 1) {
        return false;
    }

    char service[6];
    if (getnameinfo((const struct sockaddr *)&source,
                    source_length,
                    host,
                    (socklen_t)host_size,
                    VS(service),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return false;
    }
    uint64_t value;
    if (!string_parse_uint64(service, 10, 1, UINT16_MAX, &value)) {
        return false;
    }
    *port = (uint16_t)value;
    return true;
}

void socket_punch_pacer_start(socket_punch_pacer_t *pacer, uint64_t now_ms, unsigned int grace_ms) {
    HARD_ASSERT(pacer != NULL);

    pacer->next_action_ms = now_ms;
    pacer->attempts = 0;
    pacer->grace_ms = grace_ms;
    pacer->active = true;
}

socket_punch_action_t socket_punch_pacer_poll(const socket_punch_pacer_t *pacer, uint64_t now_ms) {
    HARD_ASSERT(pacer != NULL);

    if (!pacer->active || now_ms < pacer->next_action_ms) {
        return SOCKET_PUNCH_WAIT;
    }
    return pacer->attempts < SOCKET_PUNCH_COUNT ? SOCKET_PUNCH_SEND : SOCKET_PUNCH_COMPLETE;
}

void socket_punch_pacer_advance(socket_punch_pacer_t *pacer,
                                uint64_t now_ms,
                                socket_punch_action_t action) {
    HARD_ASSERT(pacer != NULL);

    if (action == SOCKET_PUNCH_SEND) {
        pacer->attempts++;
        pacer->next_action_ms =
            now_ms +
            (pacer->attempts < SOCKET_PUNCH_COUNT ? SOCKET_PUNCH_INTERVAL_MS : pacer->grace_ms);
    } else if (action == SOCKET_PUNCH_COMPLETE) {
        pacer->active = false;
    }
}

static bool socket_local_candidate(socket_t *sc, char *host, size_t host_size, uint16_t *port) {
    socklen_t peer_length;
    int family = ((struct sockaddr *)&sc->addr)->sa_family;
    if (family == AF_INET) {
        peer_length = sizeof(struct sockaddr_in);
#ifdef HAVE_IPV6
    } else if (family == AF_INET6) {
        peer_length = sizeof(struct sockaddr_in6);
#endif
    } else {
        return false;
    }

    static const char probe[] = SOCKET_PUNCH_PROBE;
    if (sendto(sc->handle,
               probe,
               sizeof(probe) - 1,
               0,
               (const struct sockaddr *)&sc->addr,
               peer_length) != (ssize_t)(sizeof(probe) - 1)) {
        return false;
    }

    struct sockaddr_storage local;
    socklen_t local_length = sizeof(local);
    if (getsockname(sc->handle, (struct sockaddr *)&local, &local_length) != 0) {
        return false;
    }
    char service[6];
    if (getnameinfo((const struct sockaddr *)&local,
                    local_length,
                    host,
                    (socklen_t)host_size,
                    VS(service),
                    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        return false;
    }
    uint64_t value;
    if (!string_parse_uint64(service, 10, 1, UINT16_MAX, &value)) {
        return false;
    }
    *port = (uint16_t)value;
    return true;
}

static void socket_udp_punch_schedule(socket_punch_job_t *jobs,
                                      size_t capacity,
                                      const socket_direct_candidate_t *candidate) {
    if (candidate->kind != SOCKET_CANDIDATE_MAPPED && candidate->kind != SOCKET_CANDIDATE_SRFLX) {
        return;
    }

    socket_punch_job_t *available = NULL;
    for (size_t i = 0; i < capacity; i++) {
        if (jobs[i].pacer.active && jobs[i].candidate.port == candidate->port &&
            strcmp(jobs[i].candidate.host, candidate->host) == 0) {
            return;
        }
        if (!jobs[i].pacer.active && available == NULL) {
            available = &jobs[i];
        }
    }
    if (available == NULL) {
        LOG(ERROR, "Client UDP punch queue is full");
        return;
    }

    available->candidate = *candidate;
    socket_punch_pacer_start(&available->pacer, datetime_monotonic_ms(), 0);
    LOG(INFO,
        "Opening a paced UDP path to %s QUIC candidate %s:%" PRIu16,
        socket_candidate_kind_name(candidate->kind),
        candidate->host,
        candidate->port);
}

static void socket_udp_punch_update(socket_t *sc,
                                    socket_punch_job_t *jobs,
                                    size_t capacity,
                                    unsigned int *attempts,
                                    unsigned int *successful) {
    uint64_t now = datetime_monotonic_ms();
    for (size_t i = 0; i < capacity; i++) {
        socket_punch_job_t *job = &jobs[i];
        socket_punch_action_t action = socket_punch_pacer_poll(&job->pacer, now);
        if (action == SOCKET_PUNCH_WAIT) {
            continue;
        }

        if (action == SOCKET_PUNCH_COMPLETE) {
            socket_punch_pacer_advance(&job->pacer, now, action);
            continue;
        }

        (*attempts)++;
        if (socket_udp_punch(sc, job->candidate.host, job->candidate.port)) {
            (*successful)++;
        }
        socket_punch_pacer_advance(&job->pacer, now, action);
    }
}

static size_t socket_udp_punch_collect(socket_t *sc,
                                       socket_direct_candidate_t *candidates,
                                       size_t *count,
                                       size_t capacity) {
    size_t received = 0;
    while (received < SOCKET_PUNCH_DRAIN_MAX) {
        char host[65];
        uint16_t port;
        if (!socket_udp_punch_receive(sc, VS(host), &port)) {
            return received;
        }
        received++;

        bool already_recorded = false;
        bool has_peer_reflexive = false;
        for (size_t i = 0; i < *count; i++) {
            if (candidates[i].kind == SOCKET_CANDIDATE_PRFLX) {
                has_peer_reflexive = true;
            }
            if (candidates[i].port == port && strcmp(candidates[i].host, host) == 0) {
                if (candidates[i].kind != SOCKET_CANDIDATE_PRFLX) {
                    candidates[i].kind = SOCKET_CANDIDATE_PRFLX;
                    LOG(INFO,
                        "Confirmed peer-reflexive QUIC candidate %s:%lu "
                        "from a UDP punch",
                        host,
                        (unsigned long)port);
                }
                already_recorded = true;
                break;
            }
        }
        if (already_recorded || has_peer_reflexive) {
            continue;
        }

        size_t previous_count = *count;
        socket_candidate_add(candidates, count, capacity, host, port, SOCKET_CANDIDATE_PRFLX);
        if (*count != previous_count) {
            LOG(INFO,
                "Learned peer-reflexive QUIC candidate %s:%lu from a UDP "
                "punch",
                host,
                (unsigned long)port);
        }
    }
    return received;
}

bool socket_rendezvous_url(const char *base_url,
                           const char *server_id,
                           const char *role,
                           char *url,
                           size_t url_size) {
    HARD_ASSERT(base_url != NULL);
    HARD_ASSERT(server_id != NULL);
    HARD_ASSERT(role != NULL);
    HARD_ASSERT(url != NULL);

    CURLU *parsed = curl_url();
    char *scheme = NULL;
    char *rendered = NULL;
    char path[MAX_BUF];
    char query[64];
    bool ok = parsed != NULL && curl_url_set(parsed, CURLUPART_URL, base_url, 0) == CURLUE_OK &&
              curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) == CURLUE_OK;
    if (ok) {
        const char *websocket_scheme = strcmp(scheme, "https") == 0  ? "wss"
                                       : strcmp(scheme, "http") == 0 ? "ws"
                                                                     : NULL;
        ok = websocket_scheme != NULL &&
             snprintf(VS(path), "/v2/rendezvous/%s", server_id) < (int)sizeof(path) &&
             snprintf(VS(query), "role=%s", role) < (int)sizeof(query) &&
             curl_url_set(parsed, CURLUPART_SCHEME, websocket_scheme, 0) == CURLUE_OK &&
             curl_url_set(parsed, CURLUPART_PATH, path, 0) == CURLUE_OK &&
             curl_url_set(parsed, CURLUPART_QUERY, query, 0) == CURLUE_OK &&
             curl_url_set(parsed, CURLUPART_FRAGMENT, NULL, 0) == CURLUE_OK &&
             curl_url_get(parsed, CURLUPART_URL, &rendered, 0) == CURLUE_OK &&
             snprintf(url, url_size, "%s", rendered) < (int)url_size;
    }

    curl_free(rendered);
    curl_free(scheme);
    curl_url_cleanup(parsed);
    return ok;
}

#if LIBCURL_VERSION_NUM >= 0x075600
socket_websocket_receive_state_t
socket_websocket_receive(void *handle, char *buffer, size_t capacity, size_t *used) {
    HARD_ASSERT(handle != NULL);
    HARD_ASSERT(buffer != NULL);
    HARD_ASSERT(used != NULL);
    if (capacity < 2 || *used >= capacity - 1) {
        return SOCKET_WEBSOCKET_CLOSED;
    }

    size_t received = 0;
    const struct curl_ws_frame *frame = NULL;
    CURLcode result = curl_ws_recv(handle, buffer + *used, capacity - 1 - *used, &received, &frame);
    if (result == CURLE_AGAIN) {
        return SOCKET_WEBSOCKET_EMPTY;
    }
    if (result != CURLE_OK || frame == NULL || (frame->flags & CURLWS_CLOSE) != 0 ||
        (frame->flags & CURLWS_TEXT) == 0 || received > capacity - 1 - *used) {
        return SOCKET_WEBSOCKET_CLOSED;
    }

    *used += received;
    if (frame->bytesleft != 0) {
        return SOCKET_WEBSOCKET_PARTIAL;
    }
    buffer[*used] = '\0';
    return SOCKET_WEBSOCKET_MESSAGE;
}

size_t socket_rendezvous_client(socket_t *sc,
                                const char *url,
                                const char *stun_endpoint,
                                socket_direct_candidate_t *candidates,
                                size_t capacity) {
    char host[65];
    uint16_t port;
    if (url == NULL) {
        return 0;
    }
    bool have_candidate =
        stun_endpoint != NULL && socket_stun_discover(sc, stun_endpoint, VS(host), &port);
    if (!have_candidate) {
        have_candidate = socket_local_candidate(sc, VS(host), &port);
    }
    if (!have_candidate) {
        LOG(ERROR, "Cannot determine a local rendezvous candidate");
        return 0;
    }

    unsigned char random_ticket[32];
    char ticket[65];
    if (RAND_bytes(VS(random_ticket)) != 1 ||
        string_tohex(VS(random_ticket), VS(ticket), false) != 64) {
        return 0;
    }
    string_tolower(ticket);

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        return 0;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
#ifdef WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "ca-bundle.crt");
#endif
    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        LOG(ERROR, "Rendezvous connection failed: %s", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        return 0;
    }

    char candidate[256];
    if (!socket_rendezvous_message_render(VS(candidate),
                                          "client_candidate",
                                          host,
                                          port,
                                          SOCKET_CANDIDATE_NUM,
                                          ticket)) {
        curl_easy_cleanup(curl);
        return 0;
    }
    size_t sent = 0;
    result = curl_ws_send(curl, candidate, strlen(candidate), &sent, 0, CURLWS_TEXT);
    if (result != CURLE_OK || sent != strlen(candidate)) {
        curl_easy_cleanup(curl);
        return 0;
    }

    char response[512];
    size_t used = 0;
    uint64_t deadline_ms = datetime_monotonic_ms() + 5000;
    size_t count = 0;
    bool complete = false;
    socket_punch_job_t punch_jobs[SOCKET_DIRECT_MAX_CANDIDATES] = {0};
    unsigned int punch_attempts = 0;
    unsigned int punches_sent = 0;
    size_t punches_received = 0;
    while (!complete) {
        socket_udp_punch_update(sc,
                                punch_jobs,
                                arraysize(punch_jobs),
                                &punch_attempts,
                                &punches_sent);
        punches_received += socket_udp_punch_collect(sc, candidates, &count, capacity);

        socket_websocket_receive_state_t receive_state =
            socket_websocket_receive(curl, VS(response), &used);
        if (receive_state == SOCKET_WEBSOCKET_EMPTY) {
            if (datetime_monotonic_ms() >= deadline_ms) {
                break;
            }
            usleep(20000);
            continue;
        }
        if (receive_state == SOCKET_WEBSOCKET_PARTIAL) {
            continue;
        }
        if (receive_state != SOCKET_WEBSOCKET_MESSAGE) {
            break;
        }
        socket_direct_candidate_t parsed_candidate;
        if (socket_rendezvous_server_candidate_parse(response, ticket, &parsed_candidate)) {
            socket_candidate_add(candidates,
                                 &count,
                                 capacity,
                                 parsed_candidate.host,
                                 parsed_candidate.port,
                                 parsed_candidate.kind);
            socket_udp_punch_schedule(punch_jobs, arraysize(punch_jobs), &parsed_candidate);
        } else {
            complete = socket_rendezvous_complete_parse(response, ticket);
        }
        used = 0;
    }

    socket_udp_punch_update(sc, punch_jobs, arraysize(punch_jobs), &punch_attempts, &punches_sent);
    punches_received += socket_udp_punch_collect(sc, candidates, &count, capacity);
    LOG(INFO,
        "Rendezvous UDP punch summary: sent %u/%u probes, received %" PRIu64,
        punches_sent,
        punch_attempts,
        (uint64_t)punches_received);
    curl_easy_cleanup(curl);
    return complete ? count : 0;
}
#else
socket_websocket_receive_state_t
socket_websocket_receive(void *handle, char *buffer, size_t capacity, size_t *used) {
    (void)handle;
    (void)buffer;
    (void)capacity;
    (void)used;
    return SOCKET_WEBSOCKET_CLOSED;
}

size_t socket_rendezvous_client(socket_t *sc,
                                const char *url,
                                const char *stun_endpoint,
                                socket_direct_candidate_t *candidates,
                                size_t capacity) {
    return 0;
}
#endif

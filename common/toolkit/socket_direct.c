/**
 * @file
 *
 * STUN candidate discovery, UDP hole punching, and rendezvous signaling.
 */

#include "socket_private.h"
#include "string.h"

#include <curl/curl.h>
#include <openssl/rand.h>
#define SOCKET_STUN_MAGIC 0x2112a442U

static uint16_t
socket_stun_u16 (const unsigned char *b)
{
    return (uint16_t) (((uint16_t) b[0] << 8) | b[1]);
}

static uint32_t
socket_stun_u32 (const unsigned char *b)
{
    return ((uint32_t) b[0] << 24) | ((uint32_t) b[1] << 16) |
           ((uint32_t) b[2] << 8) | b[3];
}

bool
socket_stun_discover (socket_t    *sc,
                      const char  *endpoint,
                      char        *host,
                      size_t       host_size,
                      uint16_t    *port)
{
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(endpoint != NULL);
    HARD_ASSERT(host != NULL);
    HARD_ASSERT(port != NULL);

    const char *separator = strrchr(endpoint, ':');
    if (separator == NULL || separator == endpoint ||
        separator[1] == '\0' || strlen(separator + 1) >= 6) {
        LOG(ERROR, "Invalid STUN endpoint: %s", endpoint);
        return false;
    }
    char stun_host[MAX_BUF], stun_port[6];
    size_t stun_host_length = (size_t) (separator - endpoint);
    if (stun_host_length >= sizeof(stun_host)) {
        return false;
    }
    memcpy(stun_host, endpoint, stun_host_length);
    stun_host[stun_host_length] = '\0';
    snprintf(VS(stun_port), "%s", separator + 1);

    struct addrinfo hints, *addresses = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ((struct sockaddr *) &sc->addr)->sa_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV;
    int rc = getaddrinfo(stun_host, stun_port, &hints, &addresses);
    if (rc != 0) {
        LOG(ERROR, "Cannot resolve STUN endpoint %s: %s",
            endpoint, gai_strerror(rc));
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
        if (sendto(sc->handle, request, sizeof(request), 0,
                   ai->ai_addr, ai->ai_addrlen) == (ssize_t) sizeof(request)) {
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
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    if (select(sc->handle + 1, &readfds, NULL, NULL, &timeout) != 1) {
        LOG(ERROR, "STUN request to %s timed out", endpoint);
        return false;
    }

    unsigned char response[1024];
    ssize_t length = recvfrom(sc->handle, response, sizeof(response),
                              0, NULL, NULL);
    if (length < 20 || socket_stun_u16(response) != 0x0101 ||
        socket_stun_u32(response + 4) != SOCKET_STUN_MAGIC ||
        memcmp(response + 8, request + 8, 12) != 0) {
        LOG(ERROR, "Invalid STUN response from %s", endpoint);
        return false;
    }

    size_t message_length = socket_stun_u16(response + 2);
    if (message_length > (size_t) length - 20) {
        return false;
    }
    for (size_t offset = 20; offset + 4 <= 20 + message_length;) {
        uint16_t type = socket_stun_u16(response + offset);
        size_t value_length = socket_stun_u16(response + offset + 2);
        const unsigned char *value = response + offset + 4;
        if (offset + 4 + value_length > (size_t) length) {
            return false;
        }
        if (type == 0x0020 && value_length >= 8) {
            int family = value[1];
            *port = socket_stun_u16(value + 2) ^
                    (uint16_t) (SOCKET_STUN_MAGIC >> 16);
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
            return inet_ntop(family == 1 ? AF_INET : AF_INET6,
                             address, host, host_size) != NULL;
        }
        offset += 4 + ((value_length + 3) & ~(size_t) 3);
    }

    LOG(ERROR, "STUN response did not contain XOR-MAPPED-ADDRESS");
    return false;
}

bool
socket_udp_punch (socket_t *sc, const char *host, uint16_t port)
{
    HARD_ASSERT(sc != NULL);
    HARD_ASSERT(host != NULL);

    char port_string[6];
    snprintf(VS(port_string), "%" PRIu16, port);
    struct addrinfo hints, *addresses = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ((struct sockaddr *) &sc->addr)->sa_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_NUMERICSERV;
    if (getaddrinfo(host, port_string, &hints, &addresses) != 0) {
        return false;
    }

    static const unsigned char probe[] = "ATRINIK-PUNCH-1";
    bool ok = false;
    for (struct addrinfo *ai = addresses; ai != NULL; ai = ai->ai_next) {
        if (sendto(sc->handle, probe, sizeof(probe) - 1, 0,
                   ai->ai_addr, ai->ai_addrlen) ==
            (ssize_t) (sizeof(probe) - 1)) {
            ok = true;
        }
    }
    freeaddrinfo(addresses);
    return ok;
}


#if LIBCURL_VERSION_NUM >= 0x075600
bool
socket_rendezvous_client (socket_t   *sc,
                          const char *url,
                          const char *stun_endpoint)
{
    char host[65];
    uint16_t port;
    if (url == NULL || stun_endpoint == NULL ||
        !socket_stun_discover(sc,
                              stun_endpoint,
                              VS(host),
                              &port)) {
        return false;
    }

    unsigned char random_ticket[32];
    char ticket[65];
    if (RAND_bytes(VS(random_ticket)) != 1 ||
        string_tohex(VS(random_ticket), VS(ticket), false) != 64) {
        return false;
    }
    string_tolower(ticket);

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        LOG(ERROR, "Rendezvous connection failed: %s",
            curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        return false;
    }

    char candidate[256];
    snprintf(VS(candidate),
             "{\"type\":\"candidate\",\"host\":\"%s\","
             "\"port\":%" PRIu16 ",\"ticket\":\"%s\"}",
             host,
             port,
             ticket);
    size_t sent = 0;
    result = curl_ws_send(curl,
                          candidate,
                          strlen(candidate),
                          &sent,
                          0,
                          CURLWS_TEXT);
    if (result != CURLE_OK || sent != strlen(candidate)) {
        curl_easy_cleanup(curl);
        return false;
    }

    char response[256];
    size_t used = 0;
    TIMER_START(wait);
    bool ready = false;
    while (!ready) {
        size_t received = 0;
        const struct curl_ws_frame *frame = NULL;
        result = curl_ws_recv(curl,
                              response + used,
                              sizeof(response) - 1 - used,
                              &received,
                              &frame);
        if (result == CURLE_AGAIN) {
            TIMER_UPDATE(wait);
            if (TIMER_GET(wait) > 5.0) {
                break;
            }
            usleep(20000);
            continue;
        }
        if (result != CURLE_OK || frame == NULL ||
            (frame->flags & CURLWS_CLOSE) != 0 ||
            (frame->flags & CURLWS_TEXT) == 0 ||
            used + received >= sizeof(response) - 1) {
            break;
        }

        used += received;
        if (frame->bytesleft != 0) {
            continue;
        }
        response[used] = '\0';
        char expected[128];
        snprintf(VS(expected),
                 "{\"type\":\"ready\",\"ticket\":\"%s\"}",
                 ticket);
        ready = strcmp(response, expected) == 0;
        used = 0;
    }

    curl_easy_cleanup(curl);
    return ready;
}
#else
bool
socket_rendezvous_client (socket_t   *sc,
                          const char *url,
                          const char *stun_endpoint)
{
    return false;
}
#endif

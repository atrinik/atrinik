/**
 * @file
 *
 * Native process used by the UDP/QUIC end-to-end automation.
 */

#include <toolkit/logger.h>
#include <toolkit/path.h>
#include <toolkit/socket.h>
#include <toolkit/datetime.h>
#include <toolkit/toolkit.h>
#include <port_mapping.h>

#define DRIVER_TIMEOUT_MS 8000U
#define DRIVER_DISCONNECT_TIMEOUT_MS 1000U
#define DRIVER_BUFFER_SIZE 1024U
#define DRIVER_ASSET_STREAMS 3U
#define DRIVER_ASSET_SIZE (64U * 1024U)
#define DRIVER_ASSET_QUANTUM 1024U

#define DRIVER_REQUIRE(condition, message)              \
    do {                                                \
        if (!(condition)) {                             \
            fprintf(stderr, "driver: %s\n", (message)); \
            return false;                               \
        }                                               \
    } while (0)

static void driver_pause(void) {
    usleep(1000);
}

static bool driver_parse_port(const char *value, uint16_t *port) {
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed > UINT16_MAX) {
        return false;
    }
    *port = (uint16_t)parsed;
    return true;
}

static socket_t *driver_listener(uint16_t port, const char *identity_path) {
    return socket_quic_server_create("127.0.0.1", port, false, identity_path);
}

static bool driver_write_all(socket_t *socket, const void *data, size_t length) {
    size_t offset = 0;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (offset < length && datetime_monotonic_ms() < deadline) {
        size_t amount = 0;
        if (!socket_write(socket, (const uint8_t *)data + offset, length - offset, &amount)) {
            return false;
        }
        offset += amount;
        if (amount == 0) {
            driver_pause();
        }
    }
    return offset == length;
}

static bool driver_read_all(socket_t *socket, void *data, size_t length) {
    size_t offset = 0;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (offset < length && datetime_monotonic_ms() < deadline) {
        bool ready = socket_wait(socket, true, false, socket_quic_timeout(socket, 20));
        if (!socket_quic_service(socket, ready, false)) {
            driver_pause();
            continue;
        }
        size_t amount = 0;
        if (!socket_read(socket, (uint8_t *)data + offset, length - offset, &amount)) {
            return false;
        }
        offset += amount;
        if (amount == 0) {
            driver_pause();
        }
    }
    return offset == length;
}

static bool driver_stream_write_all(socket_stream_t *stream, const void *data, size_t length) {
    size_t offset = 0;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (offset < length && datetime_monotonic_ms() < deadline) {
        size_t amount = 0;
        socket_stream_result_t result =
            socket_stream_write(stream, (const uint8_t *)data + offset, length - offset, &amount);
        if (result == SOCKET_STREAM_RESULT_ERROR || result == SOCKET_STREAM_RESULT_FINISHED) {
            return false;
        }
        offset += amount;
        if (amount == 0) {
            driver_pause();
        }
    }
    return offset == length;
}

static bool driver_stream_read_request(socket_stream_t *stream, uint8_t *value) {
    bool received = false;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (datetime_monotonic_ms() < deadline) {
        uint8_t byte;
        size_t amount = 0;
        socket_stream_result_t result = socket_stream_read(stream, &byte, sizeof(byte), &amount);
        if (result == SOCKET_STREAM_RESULT_ERROR) {
            return false;
        }
        if (result == SOCKET_STREAM_RESULT_FINISHED) {
            return received;
        }
        if (amount != 0) {
            if (received) {
                return false;
            }
            *value = byte;
            received = true;
        } else {
            driver_pause();
        }
    }
    return false;
}

static bool driver_wait_for_close(socket_t *socket, const char *marker) {
    uint64_t started = datetime_monotonic_ms();
    uint64_t deadline = started + DRIVER_DISCONNECT_TIMEOUT_MS;
    while (datetime_monotonic_ms() < deadline) {
        bool ready = socket_wait(socket, true, false, socket_quic_timeout(socket, 20));
        socket_quic_service(socket, ready, false);
        uint8_t byte;
        size_t amount;
        if (!socket_read(socket, &byte, sizeof(byte), &amount)) {
            printf("%s %" PRIu64 "\n", marker, datetime_monotonic_ms() - started);
            fflush(stdout);
            return true;
        }
        DRIVER_REQUIRE(amount == 0, "received unexpected data while waiting for peer closure");
    }
    return false;
}

static bool driver_fingerprint(uint16_t port, const char *identity_path) {
    socket_t *listener = driver_listener(port, identity_path);
    DRIVER_REQUIRE(listener != NULL, "could not create QUIC listener");

    char fingerprint[65];
    bool ok = socket_certificate_sha256(listener, fingerprint);
    if (ok) {
        printf("FINGERPRINT %s\n", fingerprint);
        fflush(stdout);
    }
    socket_destroy(listener);
    return ok;
}

static bool driver_server(uint16_t port, const char *identity_path, const char *expected_payload) {
    size_t expected_length = strlen(expected_payload);
    DRIVER_REQUIRE(expected_length != 0 && expected_length < DRIVER_BUFFER_SIZE,
                   "invalid echo payload length");

    socket_t *listener = driver_listener(port, identity_path);
    DRIVER_REQUIRE(listener != NULL, "could not create QUIC listener");
    char fingerprint[65];
    DRIVER_REQUIRE(socket_certificate_sha256(listener, fingerprint),
                   "could not read QUIC certificate fingerprint");
    uint16_t bound_port;
    DRIVER_REQUIRE(socket_local_port(listener, &bound_port), "could not read QUIC listener port");
    printf("READY %" PRIu16 " %s\n", bound_port, fingerprint);
    fflush(stdout);

    socket_t *connection = NULL;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (connection == NULL && datetime_monotonic_ms() < deadline) {
        if (socket_wait(listener, true, false, 20)) {
            connection = socket_accept(listener);
        }
    }
    if (connection == NULL) {
        socket_destroy(listener);
        DRIVER_REQUIRE(false, "timed out accepting QUIC connection");
    }

    char payload[DRIVER_BUFFER_SIZE] = {0};
    bool ok = driver_read_all(connection, payload, expected_length) &&
              memcmp(payload, expected_payload, expected_length) == 0 &&
              driver_write_all(connection, payload, expected_length);
    if (ok) {
        printf("ECHO %s\n", socket_get_id(connection));
        fflush(stdout);
    }
    socket_destroy(connection);
    socket_destroy(listener);
    return ok;
}

static bool
driver_client(const char *host, uint16_t port, const char *fingerprint, const char *payload) {
    size_t length = strlen(payload);
    DRIVER_REQUIRE(length != 0 && length < DRIVER_BUFFER_SIZE, "invalid echo payload length");

    socket_t *connection = socket_quic_client_create(host,
                                                     port,
                                                     fingerprint,
                                                     NULL,
                                                     NULL,
                                                     SOCKET_CONNECTION_PREFERENCE_AUTO);
    DRIVER_REQUIRE(connection != NULL, "QUIC connection failed");

    char echoed[DRIVER_BUFFER_SIZE] = {0};
    bool ok = driver_write_all(connection, payload, length) &&
              driver_read_all(connection, echoed, length) && memcmp(echoed, payload, length) == 0;
    if (ok) {
        printf("CLIENT %s\n", socket_get_id(connection));
        fflush(stdout);
    }
    socket_destroy(connection);
    return ok;
}

static bool driver_streams_server(uint16_t port, const char *identity_path) {
    socket_t *listener = driver_listener(port, identity_path);
    DRIVER_REQUIRE(listener != NULL, "could not create multi-stream listener");
    char fingerprint[65];
    uint16_t bound_port;
    DRIVER_REQUIRE(socket_certificate_sha256(listener, fingerprint) &&
                       socket_local_port(listener, &bound_port),
                   "could not inspect multi-stream listener");
    printf("READY %" PRIu16 " %s\n", bound_port, fingerprint);
    fflush(stdout);

    socket_t *connection = NULL;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (connection == NULL && datetime_monotonic_ms() < deadline) {
        if (socket_wait(listener, true, false, 20)) {
            connection = socket_accept(listener);
        }
    }
    DRIVER_REQUIRE(connection != NULL, "timed out accepting multi-stream connection");

    socket_stream_t *accepted[DRIVER_ASSET_STREAMS] = {0};
    socket_stream_t *streams[DRIVER_ASSET_STREAMS] = {0};
    bool accepted_read[DRIVER_ASSET_STREAMS] = {0};
    bool request_received[DRIVER_ASSET_STREAMS] = {0};
    size_t stream_count = 0;
    char start[5] = {0};
    size_t start_received = 0;
    deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while ((stream_count < DRIVER_ASSET_STREAMS || start_received < sizeof(start)) &&
           datetime_monotonic_ms() < deadline) {
        bool ready = socket_wait(connection, true, true, socket_quic_timeout(connection, 10));
        socket_quic_service(connection, ready, true);

        size_t amount = 0;
        if (start_received < sizeof(start) && !socket_read(connection,
                                                           start + start_received,
                                                           sizeof(start) - start_received,
                                                           &amount)) {
            break;
        }
        start_received += amount;
        while (stream_count < DRIVER_ASSET_STREAMS) {
            socket_stream_t *stream = socket_stream_accept(connection, SOCKET_STREAM_ASSET);
            if (stream == NULL) {
                break;
            }
            accepted[stream_count++] = stream;
        }
        for (size_t i = 0; i < stream_count; i++) {
            if (!accepted_read[i]) {
                uint8_t id = UINT8_MAX;
                if (driver_stream_read_request(accepted[i], &id) && id < DRIVER_ASSET_STREAMS &&
                    streams[id] == NULL) {
                    accepted_read[i] = true;
                    request_received[id] = true;
                    streams[id] = accepted[i];
                }
            }
        }
    }
    DRIVER_REQUIRE(start_received == sizeof(start) && memcmp(start, "start", sizeof(start)) == 0,
                   "game stream did not start the asset transfer");
    for (size_t i = 0; i < DRIVER_ASSET_STREAMS; i++) {
        DRIVER_REQUIRE(request_received[i], "asset request stream was not classified cleanly");
    }
    size_t sent[DRIVER_ASSET_STREAMS] = {0};
    bool cancelled = false;
    char probe[4] = {0};
    size_t probe_received = 0;
    bool probe_echoed = false;
    uint8_t body[DRIVER_ASSET_QUANTUM];
    deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while ((sent[0] < DRIVER_ASSET_SIZE || sent[1] < DRIVER_ASSET_SIZE || !cancelled ||
            !probe_echoed) &&
           datetime_monotonic_ms() < deadline) {
        size_t game_amount = 0;
        if (probe_received < sizeof(probe)) {
            DRIVER_REQUIRE(socket_read(connection,
                                       probe + probe_received,
                                       sizeof(probe) - probe_received,
                                       &game_amount),
                           "game probe stream failed during asset transfer");
            probe_received += game_amount;
        }
        if (!probe_echoed && probe_received == sizeof(probe)) {
            DRIVER_REQUIRE(memcmp(probe, "ping", sizeof(probe)) == 0,
                           "unexpected game probe during asset transfer");
            DRIVER_REQUIRE(driver_write_all(connection, probe, sizeof(probe)),
                           "could not echo game probe during asset transfer");
            probe_echoed = true;
        }
        for (size_t i = 0; i < DRIVER_ASSET_STREAMS; i++) {
            if (streams[i] == NULL || sent[i] == DRIVER_ASSET_SIZE) {
                continue;
            }
            memset(body, 'A' + (int)i, sizeof(body));
            size_t amount = 0;
            socket_stream_result_t result =
                socket_stream_write(streams[i], body, sizeof(body), &amount);
            if (result == SOCKET_STREAM_RESULT_ERROR || result == SOCKET_STREAM_RESULT_FINISHED) {
                DRIVER_REQUIRE(i == 2, "completed asset stream failed");
                socket_stream_destroy(streams[i]);
                streams[i] = NULL;
                cancelled = true;
                continue;
            }
            sent[i] += amount;
            if (sent[i] == DRIVER_ASSET_SIZE) {
                DRIVER_REQUIRE(socket_stream_conclude(streams[i]), "could not conclude asset body");
                socket_stream_destroy(streams[i]);
                streams[i] = NULL;
            }
        }
        socket_quic_service(connection, false, true);
        driver_pause();
    }
    DRIVER_REQUIRE(sent[0] == DRIVER_ASSET_SIZE && sent[1] == DRIVER_ASSET_SIZE && cancelled &&
                       probe_echoed,
                   "asset streams and game probe did not complete independently");

    char done[4] = {0};
    bool ok = driver_read_all(connection, done, sizeof(done)) &&
              memcmp(done, "done", sizeof(done)) == 0 &&
              driver_write_all(connection, done, sizeof(done));
    if (ok) {
        printf("STREAMS server fairness cancellation\n");
        fflush(stdout);
    }
    socket_destroy(connection);
    socket_destroy(listener);
    return ok;
}

static bool driver_streams_client(const char *host, uint16_t port, const char *fingerprint) {
    socket_t *connection = socket_quic_client_create(host,
                                                     port,
                                                     fingerprint,
                                                     NULL,
                                                     NULL,
                                                     SOCKET_CONNECTION_PREFERENCE_AUTO);
    DRIVER_REQUIRE(connection != NULL, "multi-stream QUIC connection failed");
    socket_stream_t *streams[DRIVER_ASSET_STREAMS] = {0};
    for (size_t i = 0; i < DRIVER_ASSET_STREAMS; i++) {
        streams[i] = socket_stream_open(connection, SOCKET_STREAM_ASSET);
        DRIVER_REQUIRE(streams[i] != NULL, "could not open bounded asset stream");
        uint8_t id = (uint8_t)i;
        DRIVER_REQUIRE(driver_stream_write_all(streams[i], &id, sizeof(id)) &&
                           socket_stream_conclude(streams[i]),
                       "could not send one-request asset stream");
    }

    DRIVER_REQUIRE(driver_write_all(connection, "start", 5), "could not start the asset transfer");

    size_t received[2] = {0};
    bool finished[2] = {0};
    uint64_t latency = UINT64_MAX;
    uint8_t body[DRIVER_ASSET_QUANTUM];
    while (!finished[0] || !finished[1]) {
        for (size_t i = 0; i < 2; i++) {
            if (finished[i]) {
                continue;
            }
            size_t amount = 0;
            socket_stream_result_t result =
                socket_stream_read(streams[i], body, sizeof(body), &amount);
            DRIVER_REQUIRE(result != SOCKET_STREAM_RESULT_ERROR, "asset stream failed");
            for (size_t j = 0; j < amount; j++) {
                DRIVER_REQUIRE(body[j] == 'A' + (int)i, "asset stream bytes crossed streams");
            }
            received[i] += amount;
            if (result == SOCKET_STREAM_RESULT_FINISHED) {
                DRIVER_REQUIRE(received[i] == DRIVER_ASSET_SIZE, "asset stream ended early");
                socket_stream_destroy(streams[i]);
                streams[i] = NULL;
                finished[i] = true;
            }
        }
        if (streams[2] != NULL) {
            size_t amount = 0;
            socket_stream_result_t result =
                socket_stream_read(streams[2], body, sizeof(body), &amount);
            DRIVER_REQUIRE(result != SOCKET_STREAM_RESULT_ERROR, "asset reset raced before data");
            if (amount != 0) {
                socket_stream_reset(streams[2], 42);
                socket_stream_destroy(streams[2]);
                streams[2] = NULL;
            }
        }
        if (latency == UINT64_MAX && received[0] != 0 && received[1] != 0) {
            uint64_t started = datetime_monotonic_ms();
            char echoed[4] = {0};
            DRIVER_REQUIRE(driver_write_all(connection, "ping", sizeof(echoed)) &&
                               driver_read_all(connection, echoed, sizeof(echoed)) &&
                               memcmp(echoed, "ping", sizeof(echoed)) == 0,
                           "game probe was blocked by active asset bodies");
            latency = datetime_monotonic_ms() - started;
            DRIVER_REQUIRE(latency < 250,
                           "game probe exceeded the active asset-load latency bound");
        }
        socket_quic_service(connection, false, false);
        driver_pause();
    }
    DRIVER_REQUIRE(streams[2] == NULL && latency != UINT64_MAX,
                   "active-body latency or asset cancellation was not exercised");

    char done[4] = {0};
    bool ok = driver_write_all(connection, "done", sizeof(done)) &&
              driver_read_all(connection, done, sizeof(done)) &&
              memcmp(done, "done", sizeof(done)) == 0;
    if (ok) {
        printf("STREAMS client latency_ms=%" PRIu64 " bytes=%u cancellation\n",
               latency,
               2U * DRIVER_ASSET_SIZE);
        fflush(stdout);
    }
    socket_destroy(connection);
    return ok;
}

static bool driver_disconnect_server(uint16_t port, const char *identity_path, bool close_locally) {
    socket_t *listener = driver_listener(port, identity_path);
    DRIVER_REQUIRE(listener != NULL, "could not create disconnect-test listener");
    char fingerprint[65];
    uint16_t bound_port;
    DRIVER_REQUIRE(socket_certificate_sha256(listener, fingerprint) &&
                       socket_local_port(listener, &bound_port),
                   "could not inspect disconnect-test listener");
    printf("READY %" PRIu16 " %s\n", bound_port, fingerprint);
    fflush(stdout);

    socket_t *connection = NULL;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (connection == NULL && datetime_monotonic_ms() < deadline) {
        if (socket_wait(listener, true, false, 20)) {
            connection = socket_accept(listener);
        }
    }
    DRIVER_REQUIRE(connection != NULL, "could not establish disconnect-test connection");

    bool ok;
    if (close_locally) {
        char marker;
        ok = driver_read_all(connection, &marker, 1) && marker == '!';
        socket_destroy(connection);
        printf("LOCAL_CLOSE\n");
        fflush(stdout);
    } else {
        ok =
            driver_write_all(connection, "!", 1) && driver_wait_for_close(connection, "PEER_CLOSE");
        socket_destroy(connection);
    }
    socket_destroy(listener);
    return ok;
}

static bool driver_disconnect_client(const char *host,
                                     uint16_t port,
                                     const char *fingerprint,
                                     bool close_locally) {
    socket_t *connection = socket_quic_client_create(host,
                                                     port,
                                                     fingerprint,
                                                     NULL,
                                                     NULL,
                                                     SOCKET_CONNECTION_PREFERENCE_AUTO);
    DRIVER_REQUIRE(connection != NULL, "disconnect-test QUIC connection failed");

    bool ok;
    if (close_locally) {
        char marker;
        ok = driver_read_all(connection, &marker, 1) && marker == '!';
        socket_destroy(connection);
        printf("LOCAL_CLOSE\n");
        fflush(stdout);
    } else {
        ok =
            driver_write_all(connection, "!", 1) && driver_wait_for_close(connection, "PEER_CLOSE");
        socket_destroy(connection);
    }
    return ok;
}

static bool driver_stun(uint16_t port, const char *identity_path, const char *endpoint) {
    socket_t *listener = driver_listener(port, identity_path);
    DRIVER_REQUIRE(listener != NULL, "could not create STUN source socket");

    char host[65];
    uint16_t external_port;
    bool ok = socket_stun_discover(listener, endpoint, VS(host), &external_port);
    if (ok) {
        printf("STUN %s:%" PRIu16 "\n", host, external_port);
        fflush(stdout);
    }
    socket_destroy(listener);
    return ok;
}

static bool driver_punch(uint16_t first_port,
                         const char *first_identity,
                         uint16_t second_port,
                         const char *second_identity) {
    socket_t *first = driver_listener(first_port, first_identity);
    socket_t *second = driver_listener(second_port, second_identity);
    if (first == NULL || second == NULL) {
        if (first != NULL) {
            socket_destroy(first);
        }
        if (second != NULL) {
            socket_destroy(second);
        }
        DRIVER_REQUIRE(false, "could not create UDP punch sockets");
    }

    DRIVER_REQUIRE(socket_local_port(first, &first_port) &&
                       socket_local_port(second, &second_port) && first_port != second_port,
                   "could not acquire distinct UDP punch ports");

    DRIVER_REQUIRE(socket_udp_punch(first, "127.0.0.1", second_port),
                   "could not send first UDP punch");
    char host[65];
    uint16_t observed_port = 0;
    uint64_t deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (datetime_monotonic_ms() < deadline &&
           !socket_udp_punch_receive(second, VS(host), &observed_port)) {
        driver_pause();
    }
    bool ok = observed_port == first_port && strcmp(host, "127.0.0.1") == 0;

    if (ok) {
        ok = socket_udp_punch(second, host, observed_port);
    }
    observed_port = 0;
    deadline = datetime_monotonic_ms() + DRIVER_TIMEOUT_MS;
    while (ok && datetime_monotonic_ms() < deadline &&
           !socket_udp_punch_receive(first, VS(host), &observed_port)) {
        driver_pause();
    }
    ok = ok && observed_port == second_port && strcmp(host, "127.0.0.1") == 0;
    if (ok) {
        printf("PUNCH 127.0.0.1:%" PRIu16 " 127.0.0.1:%" PRIu16 "\n", first_port, second_port);
        fflush(stdout);
    }
    socket_destroy(second);
    socket_destroy(first);
    return ok;
}

typedef struct driver_mapping_backend {
    bool succeeds;
    unsigned int opens;
    unsigned int processes;
    unsigned int closes;
} driver_mapping_backend_t;

static bool driver_mapping_open(void *data,
                                uint16_t port,
                                char *host,
                                size_t host_size,
                                uint16_t *external_port) {
    driver_mapping_backend_t *backend = data;
    backend->opens++;
    if (!backend->succeeds) {
        return false;
    }
    snprintf(host, host_size, "198.51.100.42");
    *external_port = (uint16_t)(port + 1000);
    return true;
}

static void driver_mapping_process(void *data) {
    driver_mapping_backend_t *backend = data;
    backend->processes++;
}

static void driver_mapping_close(void *data) {
    driver_mapping_backend_t *backend = data;
    backend->closes++;
}

static bool driver_mapping(void) {
    driver_mapping_backend_t pcp = {.succeeds = true};
    driver_mapping_backend_t upnp = {.succeeds = true};
    socket_port_mapping_backend_t backends[] = {
        {
            .name = "PCP/NAT-PMP",
            .data = &pcp,
            .open = driver_mapping_open,
            .process = driver_mapping_process,
            .close = driver_mapping_close,
        },
        {
            .name = "UPnP",
            .data = &upnp,
            .open = driver_mapping_open,
            .process = driver_mapping_process,
            .close = driver_mapping_close,
        },
    };
    socket_port_mapping_controller_t controller = {0};
    char host[65];
    uint16_t external_port;

    DRIVER_REQUIRE(socket_port_mapping_controller_open(&controller,
                                                       backends,
                                                       arraysize(backends),
                                                       1730,
                                                       VS(host),
                                                       &external_port),
                   "PCP mapping selection failed");
    DRIVER_REQUIRE(strcmp(socket_port_mapping_controller_name(&controller), "PCP/NAT-PMP") == 0,
                   "PCP was not preferred");
    socket_port_mapping_controller_process(&controller);
    socket_port_mapping_controller_close(&controller);
    DRIVER_REQUIRE(pcp.opens == 1 && pcp.processes == 1 && pcp.closes == 1 && upnp.opens == 0 &&
                       upnp.processes == 0 && upnp.closes == 0,
                   "PCP lifecycle counts were incorrect");
    DRIVER_REQUIRE(strcmp(host, "198.51.100.42") == 0 && external_port == 2730,
                   "PCP result was not propagated");

    memset(&pcp, 0, sizeof(pcp));
    memset(&upnp, 0, sizeof(upnp));
    upnp.succeeds = true;
    DRIVER_REQUIRE(socket_port_mapping_controller_open(&controller,
                                                       backends,
                                                       arraysize(backends),
                                                       1730,
                                                       VS(host),
                                                       &external_port),
                   "UPnP fallback selection failed");
    DRIVER_REQUIRE(strcmp(socket_port_mapping_controller_name(&controller), "UPnP") == 0,
                   "UPnP fallback was not selected");
    socket_port_mapping_controller_process(&controller);
    socket_port_mapping_controller_close(&controller);
    DRIVER_REQUIRE(pcp.opens == 1 && pcp.closes == 1 && upnp.opens == 1 && upnp.processes == 1 &&
                       upnp.closes == 1,
                   "fallback lifecycle counts were incorrect");

    memset(&pcp, 0, sizeof(pcp));
    memset(&upnp, 0, sizeof(upnp));
    DRIVER_REQUIRE(!socket_port_mapping_controller_open(&controller,
                                                        backends,
                                                        arraysize(backends),
                                                        1730,
                                                        VS(host),
                                                        &external_port),
                   "mapping unexpectedly succeeded");
    DRIVER_REQUIRE(socket_port_mapping_controller_name(&controller) == NULL && pcp.opens == 1 &&
                       pcp.closes == 1 && upnp.opens == 1 && upnp.closes == 1,
                   "failed mapping cleanup was incorrect");

    printf("MAPPING PCP/NAT-PMP UPnP cleanup\n");
    fflush(stdout);
    return true;
}

static void driver_usage(const char *program) {
    fprintf(stderr,
            "Usage:\n"
            "  %s fingerprint PORT IDENTITY\n"
            "  %s server PORT IDENTITY PAYLOAD\n"
            "  %s client HOST PORT FINGERPRINT PAYLOAD\n"
            "  %s streams-server PORT IDENTITY\n"
            "  %s streams-client HOST PORT FINGERPRINT\n"
            "  %s close-server PORT IDENTITY\n"
            "  %s wait-server PORT IDENTITY\n"
            "  %s close-client HOST PORT FINGERPRINT\n"
            "  %s wait-client HOST PORT FINGERPRINT\n"
            "  %s stun PORT IDENTITY ENDPOINT\n"
            "  %s punch PORT IDENTITY PORT IDENTITY\n"
            "  %s mapping\n",
            program,
            program,
            program,
            program,
            program,
            program,
            program,
            program,
            program,
            program,
            program,
            program);
}

int main(int argc, char **argv) {
    toolkit_import(logger);
    toolkit_import(path);
    toolkit_import(socket);

    bool ok = false;
    uint16_t first_port, second_port;
    if (argc == 4 && strcmp(argv[1], "fingerprint") == 0 &&
        driver_parse_port(argv[2], &first_port)) {
        ok = driver_fingerprint(first_port, argv[3]);
    } else if (argc == 5 && strcmp(argv[1], "server") == 0 &&
               driver_parse_port(argv[2], &first_port)) {
        ok = driver_server(first_port, argv[3], argv[4]);
    } else if (argc == 6 && strcmp(argv[1], "client") == 0 &&
               driver_parse_port(argv[3], &first_port)) {
        ok = driver_client(argv[2], first_port, argv[4], argv[5]);
    } else if (argc == 4 && strcmp(argv[1], "streams-server") == 0 &&
               driver_parse_port(argv[2], &first_port)) {
        ok = driver_streams_server(first_port, argv[3]);
    } else if (argc == 5 && strcmp(argv[1], "streams-client") == 0 &&
               driver_parse_port(argv[3], &first_port)) {
        ok = driver_streams_client(argv[2], first_port, argv[4]);
    } else if (argc == 4 && strcmp(argv[1], "close-server") == 0 &&
               driver_parse_port(argv[2], &first_port)) {
        ok = driver_disconnect_server(first_port, argv[3], true);
    } else if (argc == 4 && strcmp(argv[1], "wait-server") == 0 &&
               driver_parse_port(argv[2], &first_port)) {
        ok = driver_disconnect_server(first_port, argv[3], false);
    } else if (argc == 5 && strcmp(argv[1], "close-client") == 0 &&
               driver_parse_port(argv[3], &first_port)) {
        ok = driver_disconnect_client(argv[2], first_port, argv[4], true);
    } else if (argc == 5 && strcmp(argv[1], "wait-client") == 0 &&
               driver_parse_port(argv[3], &first_port)) {
        ok = driver_disconnect_client(argv[2], first_port, argv[4], false);
    } else if (argc == 5 && strcmp(argv[1], "stun") == 0 &&
               driver_parse_port(argv[2], &first_port)) {
        ok = driver_stun(first_port, argv[3], argv[4]);
    } else if (argc == 6 && strcmp(argv[1], "punch") == 0 &&
               driver_parse_port(argv[2], &first_port) &&
               driver_parse_port(argv[4], &second_port)) {
        ok = driver_punch(first_port, argv[3], second_port, argv[5]);
    } else if (argc == 2 && strcmp(argv[1], "mapping") == 0) {
        ok = driver_mapping();
    } else {
        driver_usage(argv[0]);
    }

    toolkit_deinit();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

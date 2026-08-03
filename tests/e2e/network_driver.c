/**
 * @file
 *
 * Native process used by the UDP/QUIC end-to-end automation.
 */

#include <toolkit/logger.h>
#include <toolkit/path.h>
#include <toolkit/socket.h>
#include <toolkit/socket_crypto.h>
#include <toolkit/datetime.h>
#include <toolkit/toolkit.h>
#include <port_mapping.h>

#define DRIVER_TIMEOUT_MS 8000U
#define DRIVER_BUFFER_SIZE 1024U

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
            "  %s stun PORT IDENTITY ENDPOINT\n"
            "  %s punch PORT IDENTITY PORT IDENTITY\n"
            "  %s mapping\n",
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
    toolkit_import(socket_crypto);

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

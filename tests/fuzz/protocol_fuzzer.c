#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <toolkit/clioptions.h>
#include <toolkit/map_protocol.h>
#include <toolkit/packet.h>
#include <toolkit/socket.h>

#define FUZZ_UNUSED __attribute__((unused))

static void FUZZ_UNUSED fuzz_packet(const uint8_t *data, size_t size) {
    packet_reader_t reader;
    packet_reader_init(&reader, data, size);

    while (packet_reader_remaining(&reader) != 0 &&
           packet_reader_error(&reader) == PACKET_ERROR_NONE) {
        switch (packet_reader_read_uint8(&reader) % 12) {
            case 0:
                (void)packet_reader_read_uint8(&reader);
                break;
            case 1:
                (void)packet_reader_read_int8(&reader);
                break;
            case 2:
                (void)packet_reader_read_uint16(&reader);
                break;
            case 3:
                (void)packet_reader_read_int16(&reader);
                break;
            case 4:
                (void)packet_reader_read_uint32(&reader);
                break;
            case 5:
                (void)packet_reader_read_int32(&reader);
                break;
            case 6:
                (void)packet_reader_read_uint64(&reader);
                break;
            case 7:
                (void)packet_reader_read_int64(&reader);
                break;
            case 8:
                (void)packet_reader_read_float(&reader);
                break;
            case 9:
                (void)packet_reader_read_double(&reader);
                break;
            case 10:
                (void)packet_reader_read_string_view(&reader, 1024);
                break;
            default: {
                size_t count;
                (void)packet_reader_read_count16(&reader, 4096, &count);
                break;
            }
        }
    }
    (void)packet_reader_finish(&reader);
}

static void FUZZ_UNUSED fuzz_envelope(const uint8_t *data, size_t size) {
    packet_reader_t reader;
    packet_reader_init(&reader, data, size);
    uint8_t command = packet_reader_read_uint8(&reader);
    if (packet_reader_error(&reader) != PACKET_ERROR_NONE) {
        return;
    }
    if (command >= MAX(CLIENT_CMD_NROF, SERVER_CMD_NROF)) {
        packet_reader_set_error(&reader, PACKET_ERROR_UNSUPPORTED);
        return;
    }
    (void)packet_reader_read_view(&reader, packet_reader_remaining(&reader));
    (void)packet_reader_finish(&reader);
}

typedef struct fuzz_command_metadata {
    const char *name;
    bool player_only;
} fuzz_command_metadata_t;

static const fuzz_command_metadata_t FUZZ_UNUSED server_commands[SERVER_CMD_NROF] = {
#define ATRINIK_SERVER_COMMAND(_id, _name, _handler, _player_only) \
    [SERVER_CMD_##_id] = {_name, _player_only},
#include <toolkit/socket_commands.def>
#undef ATRINIK_SERVER_COMMAND
};

static const fuzz_command_metadata_t FUZZ_UNUSED client_commands[CLIENT_CMD_NROF] = {
#define ATRINIK_CLIENT_COMMAND(_id, _name, _handler) [CLIENT_CMD_##_id] = {_name, false},
#include <toolkit/socket_commands.def>
#undef ATRINIK_CLIENT_COMMAND
};

static void FUZZ_UNUSED fuzz_dispatch(const uint8_t *data, size_t size) {
    packet_reader_t reader;
    packet_reader_init(&reader, data, size);
    bool server = packet_reader_read_uint8(&reader) % 2 != 0;
    uint8_t command = packet_reader_read_uint8(&reader);
    const fuzz_command_metadata_t *commands = server ? server_commands : client_commands;
    size_t command_count = server ? arraysize(server_commands) : arraysize(client_commands);
    if (packet_reader_error(&reader) != PACKET_ERROR_NONE) {
        return;
    }
    if (command >= command_count || commands[command].name == NULL) {
        packet_reader_set_error(&reader, PACKET_ERROR_UNSUPPORTED);
        return;
    }
    (void)commands[command].player_only;
    (void)packet_reader_read_view(&reader, packet_reader_remaining(&reader));
    (void)packet_reader_finish(&reader);
}

static void FUZZ_UNUSED fuzz_configuration(const uint8_t *data, size_t size) {
    if (size > 4096) {
        return;
    }
    static bool initialized;
    if (!initialized) {
        toolkit_import(clioptions);
        (void)clioptions_create("fuzz", NULL);
        initialized = true;
    }

    static const char prefix[] = "fuzz = ";
    char *line = malloc(sizeof(prefix) - 1 + size + 1);
    if (line == NULL) {
        return;
    }
    memcpy(line, prefix, sizeof(prefix) - 1);
    memcpy(line + sizeof(prefix) - 1, data, size);
    line[sizeof(prefix) - 1 + size] = '\0';
    char *error = NULL;
    (void)clioptions_load_str(line, &error);
    free(error);
    free(line);
}

static void FUZZ_UNUSED fuzz_asset(const uint8_t *data, size_t size) {
    socket_asset_request_t request;
    socket_asset_response_t response;
    (void)socket_asset_request_parse(data, size, 0, &request);
    (void)socket_asset_response_parse(data, size, 0, &response);
}

static void FUZZ_UNUSED fuzz_map(const uint8_t *data, size_t size) {
    int width = size > 0 ? data[0] % 32 + 1 : 1;
    int height = size > 1 ? data[1] % 32 + 1 : 1;
    size_t offset = MIN(size, (size_t)2);
    (void)map_protocol_validate(data, size, offset, width, height);
}

static void FUZZ_UNUSED fuzz_rendezvous(const uint8_t *data, size_t size) {
    if (size > 4096) {
        return;
    }
    char *message = malloc(size + 1);
    if (message == NULL) {
        return;
    }
    memcpy(message, data, size);
    message[size] = '\0';

    static const char ticket[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char host[65], parsed_ticket[65];
    uint16_t port;
    socket_direct_candidate_t candidate;
    (void)socket_rendezvous_client_candidate_parse(message, VS(host), &port, parsed_ticket);
    (void)socket_rendezvous_server_candidate_parse(message, ticket, &candidate);
    (void)socket_rendezvous_complete_parse(message, ticket);
    free(message);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
#if ATRINIK_FUZZ_packet
    fuzz_packet(data, size);
#elif ATRINIK_FUZZ_envelope
    fuzz_envelope(data, size);
#elif ATRINIK_FUZZ_dispatch
    fuzz_dispatch(data, size);
#elif ATRINIK_FUZZ_configuration
    fuzz_configuration(data, size);
#elif ATRINIK_FUZZ_asset
    fuzz_asset(data, size);
#elif ATRINIK_FUZZ_map
    fuzz_map(data, size);
#elif ATRINIK_FUZZ_rendezvous
    fuzz_rendezvous(data, size);
#endif
    return 0;
}

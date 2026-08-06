/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * Fork from Crossfire (Multiplayer game for X-windows).                 *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the Free Software           *
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.             *
 *                                                                       *
 * The author can be reached at admin@atrinik.org                        *
 ************************************************************************/

/**
 * @file
 * Socket server implementation.
 *
 * @author
 * Zoey Rose
 */

#include <global.h>
#include <server_main.h>
#include <initialization.h>
#include <toolkit/path.h>
#include <toolkit/string.h>
#include <toolkit/packet.h>
#include <server.h>
#include <player.h>
#include <object.h>
#include <ban.h>
#include <network_metrics.h>

TOOLKIT_API(DEPENDS(socket), IMPORTS(logger));

/**
 * @defgroup SOCKET_COMMAND_xxx Socket command flags
 * Flags for the socket commands.
 *@{*/
/**
 * The command may only be performed by clients that are logged in.
 */
#define SOCKET_COMMAND_PLAYER_ONLY 1
/*@}*/

/**
 * Maximum number of commands a player is able to issue in a single
 * iteration.
 */
#define SOCKET_SERVER_PLAYER_MAX_COMMANDS 15

/**
 * Structure to provide link linkage for client socket entries.
 */
typedef struct csocket_entry {
    struct csocket_entry *next; ///< Next entry.
    struct csocket_entry *prev; ///< Previous entry.
    socket_struct *cs; ///< Client's socket.
} csocket_entry_t;

/**
 * Structure that defines a single socket command type.
 */
typedef struct socket_command {
    /**
     * Handler function.
     */
    socket_command_func handle_func;

    /**
     * A combination of ::SOCKET_COMMAND_xxx.
     */
    int flags;
    const char *name;
} socket_command_t;

/**
 * File descriptors that have data available.
 */
static fd_set fds_read;
/** Direct UDP/QUIC listeners (IPv4 and IPv6), when enabled. */
static socket_t *quic_server_sockets[2];
static socket_direct_candidate_t quic_candidates[SOCKET_DIRECT_MAX_CANDIDATES];
static size_t quic_candidate_count;
static void socket_server_csocket_drop(csocket_entry_t *entry);
static char quic_public_host[MAX_BUF];
static uint16_t quic_public_port;
static char quic_certificate_sha256[65];
static uint64_t quic_punches_received;
static uint64_t quic_punches_echoed;
/**
 * List of client sockets that are not yet playing.
 */
static csocket_entry_t *client_sockets;
static size_t client_sockets_count;

#define SOCKET_PENDING_CONNECTIONS_MAX 128U

/**
 * Defines all the possible socket commands.
 */
static const socket_command_t socket_commands[] = {
#define ATRINIK_SERVER_COMMAND(_id, _name, _handler, _player_only)                  \
    [SERVER_CMD_##_id] = {.handle_func = (_handler),                                \
                          .flags = (_player_only) ? SOCKET_COMMAND_PLAYER_ONLY : 0, \
                          .name = (_name)},
#include <toolkit/socket_commands.def>
#undef ATRINIK_SERVER_COMMAND
};
CASSERT_ARRAY(socket_commands, SERVER_CMD_NROF);

/**
 * Initialize the socket server API.
 */
TOOLKIT_INIT_FUNC(socket_server) {
    /* Used to store the parsed network stack setting. */
    struct {
        /* Type of the network stack; some of these can be combined. */
        enum {
            STACK_IPV4,
            STACK_IPV6,
            STACK_DUAL,
        } type;

        /* IP addresses to bind to in non-dual-stack configurations. */
        struct sockaddr_storage v4;
        struct sockaddr_storage v6;
    } stack_setting;
    memset(&stack_setting, 0, sizeof(stack_setting));

    char word[MAX_BUF];
    size_t pos = 0;
    while (string_get_word(settings.network_stack, &pos, ',', VS(word), 0)) {
        string_whitespace_trim(word);

        char *cps[2];
        if (string_split(word, cps, arraysize(cps), '=') < 1) {
            LOG(ERROR, "Failed to split string: %s", word);
            exit(1);
        }

        if (strcasecmp(cps[0], "dual") == 0) {
            stack_setting.type = 0;
            BIT_SET(stack_setting.type, STACK_DUAL);
            break;
        }

        BIT_CLEAR(stack_setting.type, STACK_DUAL);

        struct sockaddr_storage *addr;
        if (strcasecmp(cps[0], "ipv4") == 0 || strcasecmp(cps[0], "v4") == 0) {
            BIT_SET(stack_setting.type, STACK_IPV4);
            addr = &stack_setting.v4;
            struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
            saddr->sin_family = AF_INET;
        } else if (strcasecmp(cps[0], "ipv6") == 0 || strcasecmp(cps[0], "v6") == 0) {
#ifdef HAVE_IPV6
            BIT_SET(stack_setting.type, STACK_IPV6);
            addr = &stack_setting.v6;
            struct sockaddr_in *saddr = (struct sockaddr_in *)addr;
            saddr->sin_family = AF_INET6;
#endif
        } else {
            LOG(ERROR, "Invalid value in network stack setting: %s", cps[0]);
            exit(1);
        }

        if (cps[1] != NULL && !socket_host2addr(cps[1], addr)) {
            LOG(ERROR, "Invalid IP address in network stack configuration: %s", cps[1]);
            exit(1);
        }
    }

    if (stack_setting.type == 0) {
        LOG(ERROR, "No network stack configuration selected");
        exit(1);
    }

    {
        if (settings.port_quic == 0) {
            LOG(ERROR, "No QUIC UDP port configured");
            exit(1);
        }
        char identity_path[HUGE_BUF];
        snprintf(VS(identity_path), "%s/quic-identity.pem", settings.datapath);
        bool dual = BIT_QUERY(stack_setting.type, STACK_DUAL);
        if (dual || BIT_QUERY(stack_setting.type, STACK_IPV4)) {
            quic_server_sockets[0] =
                socket_quic_server_create("0.0.0.0", settings.port_quic, false, identity_path);
        }
#ifdef HAVE_IPV6
        if (dual || BIT_QUERY(stack_setting.type, STACK_IPV6)) {
            quic_server_sockets[1] =
                socket_quic_server_create("::", settings.port_quic, false, identity_path);
        }
#endif
        socket_t *identity_socket =
            quic_server_sockets[0] != NULL ? quic_server_sockets[0] : quic_server_sockets[1];
        if (identity_socket == NULL ||
            !socket_certificate_sha256(identity_socket, quic_certificate_sha256)) {
            LOG(ERROR, "Failed to initialize the QUIC listener");
            exit(1);
        }
        LOG(SYSTEM, "QUIC certificate SHA-256: %s", quic_certificate_sha256);

        quic_candidate_count = 0;
        quic_public_host[0] = '\0';
        struct in_addr configured_address4;
#ifdef HAVE_IPV6
        struct in6_addr configured_address6;
#endif
        if ((inet_pton(AF_INET, settings.server_host, &configured_address4) == 1
#ifdef HAVE_IPV6
             || inet_pton(AF_INET6, settings.server_host, &configured_address6) == 1
#endif
             ) &&
            socket_host_is_global(settings.server_host)) {
            snprintf(VS(quic_public_host), "%s", settings.server_host);
        }
        quic_public_port = settings.port_quic;
        char mapped_host[65];
        uint16_t mapped_port;
        if (socket_port_mapping_init(settings.port_quic, VS(mapped_host), &mapped_port)) {
            if (socket_host_is_global(mapped_host)) {
                snprintf(VS(quic_public_host), "%s", mapped_host);
                quic_public_port = mapped_port;
            } else {
                LOG(INFO,
                    "Router mapping %s:%" PRIu16 " is not globally "
                    "routable; retaining it as an intermediate candidate",
                    mapped_host,
                    mapped_port);
            }
            snprintf(VS(quic_candidates[quic_candidate_count].host), "%s", mapped_host);
            quic_candidates[quic_candidate_count].port = mapped_port;
            quic_candidates[quic_candidate_count].kind = SOCKET_CANDIDATE_MAPPED;
            quic_candidate_count++;
        }

        char stun_host[65];
        uint16_t stun_port = settings.port_quic;
        if (*settings.stun_server != '\0' && strcmp(settings.stun_server, "off") != 0 &&
            quic_server_sockets[0] != NULL &&
            socket_stun_discover(quic_server_sockets[0],
                                 settings.stun_server,
                                 VS(stun_host),
                                 &stun_port)) {
            bool duplicate = quic_candidate_count != 0 && quic_candidates[0].port == stun_port &&
                             strcmp(quic_candidates[0].host, stun_host) == 0;
            if (!duplicate && quic_candidate_count < arraysize(quic_candidates)) {
                snprintf(VS(quic_candidates[quic_candidate_count].host), "%s", stun_host);
                quic_candidates[quic_candidate_count].port = stun_port;
                quic_candidates[quic_candidate_count].kind = SOCKET_CANDIDATE_SRFLX;
                quic_candidate_count++;
            }
            if (*quic_public_host == '\0' && socket_host_is_global(stun_host)) {
                snprintf(VS(quic_public_host), "%s", stun_host);
                quic_public_port = stun_port;
            }
        } else if (*quic_public_host == '\0' && quic_server_sockets[1] != NULL &&
                   *settings.stun_server != '\0' && strcmp(settings.stun_server, "off") != 0 &&
                   socket_stun_discover(quic_server_sockets[1],
                                        settings.stun_server,
                                        VS(stun_host),
                                        &stun_port)) {
            snprintf(VS(quic_public_host), "%s", stun_host);
            quic_public_port = stun_port;
            snprintf(VS(quic_candidates[quic_candidate_count].host), "%s", stun_host);
            quic_candidates[quic_candidate_count].port = stun_port;
            quic_candidates[quic_candidate_count].kind = SOCKET_CANDIDATE_IPV6;
            quic_candidate_count++;
        } else if (*quic_public_host == '\0') {
            if (strcmp(settings.port_mapping, "off") == 0 &&
                strcmp(settings.stun_server, "off") == 0) {
                LOG(INFO,
                    "Public UDP candidate discovery is disabled; only "
                    "LAN/IPv6 direct routes are available");
            } else {
                LOG(ERROR,
                    "No mapped or STUN public UDP candidate is available; "
                    "only LAN/IPv6 direct routes may work");
            }
        }

        quic_candidate_count +=
            socket_local_candidates(settings.port_quic,
                                    quic_candidates + quic_candidate_count,
                                    arraysize(quic_candidates) - quic_candidate_count);
#ifdef HAVE_IPV6
        if (*quic_public_host == '\0') {
            for (size_t i = 0; i < quic_candidate_count; i++) {
                struct in6_addr address6;
                if (quic_candidates[i].kind == SOCKET_CANDIDATE_IPV6 &&
                    inet_pton(AF_INET6, quic_candidates[i].host, &address6) == 1) {
                    size_t host_length =
                        strnlen(quic_candidates[i].host, sizeof(quic_candidates[i].host));
                    if (host_length == sizeof(quic_candidates[i].host)) {
                        continue;
                    }

                    memcpy(quic_public_host, quic_candidates[i].host, host_length + 1);
                    quic_public_port = quic_candidates[i].port;
                    break;
                }
            }
        }
#endif
        for (size_t i = 0; i < quic_candidate_count; i++) {
            LOG(INFO,
                "Direct %s candidate: %s:%" PRIu16,
                socket_candidate_kind_name(quic_candidates[i].kind),
                quic_candidates[i].host,
                quic_candidates[i].port);
        }
    }

    client_sockets = NULL;
    client_sockets_count = 0;
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket server API.
 */
TOOLKIT_DEINIT_FUNC(socket_server) {
    csocket_entry_t *entry, *tmp;
    DL_FOREACH_SAFE(client_sockets, entry, tmp) {
        socket_server_csocket_drop(entry);
    }
    socket_port_mapping_deinit();
    for (size_t i = 0; i < arraysize(quic_server_sockets); i++) {
        if (quic_server_sockets[i] != NULL) {
            socket_destroy(quic_server_sockets[i]);
            quic_server_sockets[i] = NULL;
        }
    }
}
TOOLKIT_DEINIT_FUNC_FINISH
bool socket_server_quic_info(char *host,
                             size_t host_size,
                             uint16_t *port,
                             char certificate_sha256[65]) {
    HARD_ASSERT(host != NULL);
    HARD_ASSERT(port != NULL);
    HARD_ASSERT(certificate_sha256 != NULL);

    if ((quic_server_sockets[0] == NULL && quic_server_sockets[1] == NULL) ||
        *quic_public_host == '\0') {
        return false;
    }

    snprintf(host, host_size, "%s", quic_public_host);
    *port = quic_public_port;
    memcpy(certificate_sha256, quic_certificate_sha256, sizeof(quic_certificate_sha256));

    return true;
}

size_t socket_server_quic_candidates(socket_direct_candidate_t *candidates, size_t capacity) {
    size_t count = MIN(quic_candidate_count, capacity);
    if (count != 0) {
        memcpy(candidates, quic_candidates, count * sizeof(*candidates));
    }
    return count;
}

bool socket_server_quic_punch(const char *host, uint16_t port) {
    size_t index = strchr(host, ':') != NULL ? 1 : 0;
    if (quic_server_sockets[index] == NULL) {
        return false;
    }

    return socket_udp_punch(quic_server_sockets[index], host, port);
}

static bool socket_server_quic_punch_receive(socket_t *server_socket) {
    char host[65];
    uint16_t port;
    if (!socket_udp_punch_receive(server_socket, VS(host), &port)) {
        return false;
    }

    quic_punches_received++;
    bool echoed = socket_udp_punch(server_socket, host, port);
    if (echoed) {
        quic_punches_echoed++;
    }
    LOG(DEBUG,
        "Received direct UDP punch from %s:%" PRIu16 "; echo %s "
        "(received=%" PRIu64 ", echoed=%" PRIu64 ")",
        host,
        port,
        echoed ? "sent" : "failed",
        quic_punches_received,
        quic_punches_echoed);
    return true;
}

/**
 * Attempt to handle a command from the client.
 *
 * @param cs
 * Client socket.
 * @param pl
 * Player associated with the client. Can be NULL if the client is not
 * playing yet.
 * @param data
 * Network data buffer containing the command to handle.
 * @param len
 * Length of the command.
 * @return
 * True if the command was handled, false otherwise.
 */
static bool socket_server_handle_command(socket_struct *cs, player *pl, uint8_t *data, size_t len) {
    size_t pos = 0;
    packet_reader_t reader;
    packet_reader_init_cursor(&reader, data, len, &pos);
    uint8_t type = packet_reader_read_uint8(&reader);

#ifndef DEBUG
    char *cp;

    LOG(DUMPRX, "Received packet with command type %d (%" PRIu64 " bytes):", type, (uint64_t)len);
    cp = xmalloc(sizeof(*cp) * (len * 3 + 1));
    string_tohex(data, len, cp, len * 3 + 1, true);
    LOG(DUMPRX, "  Hexadecimal: %s", cp);
    free(cp);
#endif

    if (packet_reader_error(&reader) != PACKET_ERROR_NONE) {
        LOG(DEVEL, "Malformed command envelope: %s", packet_error_string(reader.error));
        return true;
    }

    if (type >= SERVER_CMD_NROF || socket_commands[type].handle_func == NULL) {
        LOG(DEVEL, "Unknown command type: %" PRIu8, type);
        return true;
    }

    /* If the command is only for players and the client is not logged in yet,
     * do not handle the command. */
    if (socket_commands[type].flags & SOCKET_COMMAND_PLAYER_ONLY && pl == NULL) {
        return false;
    }

    packet_reader_scope_t scope;
    packet_reader_scope_begin(&scope);
    packet_reader_init_at(&reader, data + pos, len - pos, 0);
    socket_commands[type].handle_func(cs, pl, data + pos, len - pos, 0);
    packet_error_t error = packet_reader_scope_finish(&scope);
    if (error != PACKET_ERROR_NONE) {
        LOG(DEVEL,
            "Rejected malformed %s command: %s",
            socket_commands[type].name,
            packet_error_string(error));
    }

    return true;
}

static void socket_server_csocket_create(socket_t *server_socket) {
    socket_t *accepted = socket_accept(server_socket);
    if (accepted == NULL) {
        return;
    }
    if (client_sockets_count >= SOCKET_PENDING_CONNECTIONS_MAX) {
        LOG(ERROR,
            "Rejecting connection: pending login limit (%u) reached",
            SOCKET_PENDING_CONNECTIONS_MAX);
        server_metrics_connection_rejected(client_sockets_count);
        socket_destroy(accepted);
        return;
    }
    csocket_entry_t *entry = xcalloc(1, sizeof(*entry));
    entry->cs = xcalloc(1, sizeof(*entry->cs));
    entry->cs->sc = accepted;

    init_connection(entry->cs);
    DL_APPEND(client_sockets, entry);
    client_sockets_count++;
    server_metrics_connection_accepted(client_sockets_count);
}

/**
 * Frees the specified client socket entry.
 *
 * @param entry
 * Entry to free.
 */
static void socket_server_csocket_free(csocket_entry_t *entry) {
    HARD_ASSERT(entry != NULL);
    free_newsocket(entry->cs);
    DL_DELETE(client_sockets, entry);
    HARD_ASSERT(client_sockets_count != 0);
    client_sockets_count--;
    server_metrics_pending_changed(client_sockets_count);
    free(entry);
}

/**
 * Drops the specified client socket entry connection.
 *
 * Essentially the same as socket_server_csocket_free(), but logs a message.
 *
 * @param entry
 * Entry to drop.
 */
static void socket_server_csocket_drop(csocket_entry_t *entry) {
    HARD_ASSERT(entry != NULL);
    LOG(SYSTEM, "Connection %s: dropping connection", socket_get_id(entry->cs->sc));
    socket_server_csocket_free(entry);
}

static csocket_entry_t *socket_server_csocket_find(socket_struct *cs) {
    csocket_entry_t *entry;
    DL_FOREACH(client_sockets, entry) {
        if (entry->cs == cs) {
            return entry;
        }
    }
    return NULL;
}

static player *socket_server_player_find(socket_struct *cs) {
    player *pl;
    DL_FOREACH(first_player, pl) {
        if (pl->cs == cs) {
            return pl;
        }
    }
    return NULL;
}

static bool socket_server_quic_network_ready(socket_t *sc) {
    for (size_t i = 0; i < arraysize(quic_server_sockets); i++) {
        if (quic_server_sockets[i] != NULL && socket_fd(quic_server_sockets[i]) == socket_fd(sc) &&
            FD_ISSET(socket_fd(sc), &fds_read)) {
            return true;
        }
    }
    return false;
}

/**
 * Handle client commands.
 *
 * We only get here once there is input, and only do basic connection
 * checking.
 *
 * @param pl
 * Player to handle commands for.
 */
void socket_server_handle_client(player *pl) {
    HARD_ASSERT(pl != NULL);

    for (int num_cmds = 0; num_cmds < SOCKET_SERVER_PLAYER_MAX_COMMANDS; num_cmds++) {
        if (pl->cs->packet_recv_cmd->len == 0) {
            break;
        }

        /* Ensure the player is in a state capable of issue commands, and
         * has enough speed left to do so. */
        if (pl->cs->state == ST_ZOMBIE || pl->cs->state == ST_DEAD ||
            (pl->cs->state == ST_PLAYING && pl->ob != NULL && pl->ob->speed_left < 0)) {
            break;
        }

        size_t len = 2 + (pl->cs->packet_recv_cmd->data[0] << 8) + pl->cs->packet_recv_cmd->data[1];

        /* Reset idle counter. */
        if (pl->cs->state == ST_PLAYING) {
            pl->cs->login_count = 0;
            pl->cs->keepalive = 0;
        }

        socket_server_handle_command(pl->cs, pl, pl->cs->packet_recv_cmd->data + 2, len - 2);
        packet_delete(pl->cs->packet_recv_cmd, 0, len);
    }
}

/**
 * Removes the specified client socket from the server's managed list
 * of clients that haven't logged in yet. The client socket remains valid
 * afterwards.
 *
 * This is used from the login routine, because as soon as the client logs
 * in, they go to the player list, which is also walked through in the server
 * socket code, thus, it needs to be removed from the other list.
 *
 * @param cs
 * Client socket to remove.
 * @return
 * True on success, false on failure (no such client socket).
 */
bool socket_server_remove(socket_struct *cs) {
    csocket_entry_t *entry, *tmp;
    DL_FOREACH_SAFE(client_sockets, entry, tmp) {
        if (entry->cs == cs) {
            DL_DELETE(client_sockets, entry);
            HARD_ASSERT(client_sockets_count != 0);
            client_sockets_count--;
            server_metrics_pending_changed(client_sockets_count);
            free(entry);
            return true;
        }
    }

    return false;
}

/**
 * Checks if the specified client socket is in zombie state and takes care
 * of increasing the zombie tick counter until the socket is marked as dead.
 *
 * @param cs
 * Client socket.
 * @return
 * True if the client socket is in zombie state, false otherwise.
 */
static inline bool server_socket_csocket_is_zombie(socket_struct *cs) {
    HARD_ASSERT(cs != NULL);

    if (cs->state != ST_ZOMBIE) {
        return false;
    }

    if (cs->login_count++ >= MAX_TICKS_MULTIPLIER) {
        cs->state = ST_DEAD;
    }

    return true;
}

/**
 * Read data from the specified client socket and handle complete commands.
 *
 * @param cs
 * Client socket.
 */
static inline void socket_server_csocket_read(socket_struct *cs) {
    HARD_ASSERT(cs != NULL);

    if (cs->state == ST_DEAD) {
        return;
    }

    size_t amt;
    if (!socket_read(cs->sc,
                     (void *)(cs->packet_recv->data + cs->packet_recv->len),
                     cs->packet_recv->size - cs->packet_recv->len,
                     &amt)) {
        cs->state = ST_DEAD;
        return;
    }

    cs->packet_recv->len += amt;

    while (cs->packet_recv->len >= 2) {
        size_t size = 2 + (cs->packet_recv->data[0] << 8) + cs->packet_recv->data[1];
        if (size > cs->packet_recv->len) {
            break;
        }

        uint8_t *data = cs->packet_recv->data;
        size_t len = size;

        uint8_t *decrypted_data = data + 2;
        size_t decrypted_len = len - 2;

        /* Try to handle the command. */
        if (!socket_server_handle_command(cs, NULL, decrypted_data, decrypted_len)) {
            /* Couldn't handle it immediately, add it to the commands
             * packet. */
            packet_writer_write_uint16(cs->packet_recv_cmd, decrypted_len);
            packet_writer_write_bytes(cs->packet_recv_cmd, decrypted_data, decrypted_len);
            if (!packet_writer_finish(cs->packet_recv_cmd)) {
                LOG(ERROR,
                    "Connection %s exceeded the buffered command limit: %s",
                    socket_get_id(cs->sc),
                    packet_error_string(packet_writer_error(cs->packet_recv_cmd)));
                cs->state = ST_DEAD;
                return;
            }
        }

        packet_delete(cs->packet_recv, 0, size);
    }
}

/**
 * Accept incoming connections, read data from clients and write data to
 * clients.
 */
void socket_server_process(void) {
    static time_t heartbeat_last;
    time_t now = time(NULL);
    if (heartbeat_last == 0 || now - heartbeat_last >= 5) {
        char path[HUGE_BUF];
        char heartbeat[64];
        snprintf(VS(path), "%s/tmp/server-heartbeat", settings.datapath);
        int length = snprintf(VS(heartbeat), "%" PRIu64 "\n", (uint64_t)now);
        if (length > 0 && (size_t)length < sizeof(heartbeat) &&
            path_write_atomic(path, heartbeat, (size_t)length, 0600)) {
            heartbeat_last = now;
        }
    }
    socket_port_mapping_process();
    FD_ZERO(&fds_read);

    int nfds = 0;

    for (size_t i = 0; i < arraysize(quic_server_sockets); i++) {
        if (quic_server_sockets[i] == NULL) {
            continue;
        }
        int fd = socket_fd(quic_server_sockets[i]);
        if (nfds < fd) {
            nfds = fd;
        }
        FD_SET(fd, &fds_read);
    }

    csocket_entry_t *entry, *entry_tmp;
    DL_FOREACH_SAFE(client_sockets, entry, entry_tmp) {
        if (socket_prelogin_expired(entry->cs)) {
            LOG(SYSTEM,
                "Connection %s exceeded the pre-login deadline",
                socket_get_id(entry->cs->sc));
            entry->cs->state = ST_DEAD;
        }
        if (unlikely(!socket_is_fd_valid(entry->cs->sc))) {
            LOG(ERROR, "Invalid waiting socket: %s", socket_get_id(entry->cs->sc));
            entry->cs->state = ST_DEAD;
        }

        if (entry->cs->state == ST_DEAD) {
            socket_server_csocket_drop(entry);
            continue;
        }

        if (server_socket_csocket_is_zombie(entry->cs)) {
            continue;
        }

        /* Accepted OpenSSL QUIC connections share the listener's UDP handle.
         * Poll each SSL object explicitly after servicing that handle. */
    }

    player *pl, *pl_tmp;
    DL_FOREACH_SAFE(first_player, pl, pl_tmp) {
        if (pl->cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }

        if (unlikely(!socket_is_fd_valid(pl->cs->sc))) {
            LOG(ERROR, "Invalid waiting socket: %s", socket_get_id(pl->cs->sc));
            pl->cs->state = ST_DEAD;
        }

        if (pl->cs->keepalive++ >= SOCKET_KEEPALIVE_TIMEOUT) {
            LOG(SYSTEM,
                "Keepalive: disconnecting %s [%s]: %d",
                object_get_str(pl->ob),
                socket_get_id(pl->cs->sc),
                socket_fd(pl->cs->sc));
            pl->cs->state = ST_DEAD;
        }

        if (pl->cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }

        if (server_socket_csocket_is_zombie(pl->cs)) {
            continue;
        }
    }

    int ready;
#ifdef HAVE_PSELECT
    static struct timespec timeout;
    /* pselect does not change the timeout argument, so we're OK with a
     * static storage duration one. */
    ready = pselect(nfds + 1, &fds_read, NULL, NULL, &timeout, NULL);
#else
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ready = select(nfds + 1, &fds_read, NULL, NULL, &timeout);
#endif
    if (unlikely(ready == -1)) {
        LOG(ERROR, "pselect/select() returned an error: %s (%d)", strerror(errno), errno);
        return;
    }

    for (size_t i = 0; i < arraysize(quic_server_sockets); i++) {
        if (quic_server_sockets[i] != NULL &&
            FD_ISSET(socket_fd(quic_server_sockets[i]), &fds_read)) {
            if (socket_server_quic_punch_receive(quic_server_sockets[i])) {
                continue;
            }
            socket_server_csocket_create(quic_server_sockets[i]);
        }
    }

    DL_FOREACH_SAFE(client_sockets, entry, entry_tmp) {
        if (entry->cs->state == ST_ZOMBIE) {
            continue;
        }
        socket_struct *cs = entry->cs;
        bool network_ready = socket_server_quic_network_ready(cs->sc);
        server_metrics_quic_service(network_ready);
        if (!socket_quic_service(cs->sc, network_ready, cs->packets != NULL)) {
            continue;
        }
        socket_server_csocket_read(cs);
        entry = socket_server_csocket_find(cs);
        if (entry == NULL) {
            /* Login moved the live socket to first_player; the player pass
             * below will flush its queued response. */
            continue;
        }
        if (cs->state == ST_DEAD) {
            socket_server_csocket_drop(entry);
            continue;
        }
        socket_buffer_write(entry->cs);
    }

    DL_FOREACH_SAFE(first_player, pl, pl_tmp) {
        if (pl->cs->state == ST_ZOMBIE) {
            continue;
        }
        socket_struct *cs = pl->cs;
        bool network_ready = socket_server_quic_network_ready(cs->sc);
        server_metrics_quic_service(network_ready);
        if (!socket_quic_service(cs->sc, network_ready, cs->packets != NULL)) {
            continue;
        }
        socket_server_csocket_read(cs);
        pl = socket_server_player_find(cs);
        if (pl == NULL) {
            continue;
        }
        if (cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }
        socket_buffer_write(cs);
    }
}

/**
 * Update player socket-related data, render the map for them, etc.
 * Afterwards, attempt to write to the players' clients.
 */
void socket_server_post_process(void) {
    player *pl, *pl_tmp;
    DL_FOREACH_SAFE(first_player, pl, pl_tmp) {
        if (pl->cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }

        /* The removal of ext_title_flag is done in two steps because we might
         * be somewhere in the middle of the loop right now, which would mean
         * that the previous players in the list would not get the update. */
        if (pl->cs->ext_title_flag == 1) {
            generate_quick_name(pl);
            pl->cs->ext_title_flag = 2;
        } else if (pl->cs->ext_title_flag == 2) {
            pl->cs->ext_title_flag = 0;
        }

        esrv_update_stats(pl);
        party_update_who(pl);

        if (pl->ob->map != NULL) {
            draw_client_map(pl->ob);

            uint32_t update_tile = GET_MAP_UPDATE_COUNTER(pl->ob->map, pl->ob->x, pl->ob->y);
            if (update_tile != pl->cs->update_tile) {
                esrv_draw_look(pl->ob);
                pl->cs->update_tile = update_tile;
            }
        }

        socket_buffer_write(pl->cs);
    }
}

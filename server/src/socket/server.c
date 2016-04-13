/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <packet.h>
#include <server.h>
#include <player.h>
#include <object.h>
#include <ban.h>

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

typedef enum socket_server_id {
    SOCKET_SERVER_ID_CLASSIC_V4,
    SOCKET_SERVER_ID_SECURE_V4,
    SOCKET_SERVER_ID_CLASSIC_V6,
    SOCKET_SERVER_ID_SECURE_V6,

    SOCKET_SERVER_ID_NUM
} socket_server_id_t;

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
} socket_command_t;

/**
 * File descriptors that have data available.
 */
static fd_set fds_read;
/**
 * File descriptors that are ready to receive data.
 */
static fd_set fds_write;
/**
 * File descriptors with errors.
 */
static fd_set fds_error;
/**
 * The server's listening sockets.
 */
static socket_t *server_sockets[SOCKET_SERVER_ID_NUM];
/**
 * List of client sockets that are not yet playing.
 */
static csocket_entry_t *client_sockets;

/**
 * Defines all the possible socket commands.
 */
static const socket_command_t socket_commands[] = {
    {socket_command_control, 0},
    {socket_command_ask_face, 0},
    {socket_command_setup, 0},
    {socket_command_version, 0},
    {socket_command_security, 0},
    {socket_command_clear, 0},
    {socket_command_request_update, 0},
    {socket_command_keepalive, 0},
    {socket_command_account, 0},
    {socket_command_item_examine, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_item_apply, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_item_move, SOCKET_COMMAND_PLAYER_ONLY},
    {NULL, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_player_cmd, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_item_lock, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_item_mark, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_fire, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_quickslot, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_quest_list, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_move_path, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_combat, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_talk, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_move, SOCKET_COMMAND_PLAYER_ONLY},
    {socket_command_target, SOCKET_COMMAND_PLAYER_ONLY},
};
CASSERT_ARRAY(socket_commands, SERVER_CMD_NROF);

/**
 * Checks if the specified socket ID is for a secure connection.
 *
 * @param id
 * ID to check.
 * @return
 * Whether the ID is for a secure connection.
 */
static inline bool
server_socket_id_is_secure (socket_server_id_t id)
{
    if (id == SOCKET_SERVER_ID_SECURE_V4 || id == SOCKET_SERVER_ID_SECURE_V6) {
        return true;
    }

    return false;
}

/**
 * Checks if the specified socket ID is for an IPv6 connection.
 *
 * @param id
 * ID to check.
 * @return
 * Whether the ID is for an IPv6 connection.
 */
static inline bool
server_socket_id_is_v6 (socket_server_id_t id)
{
    if (id == SOCKET_SERVER_ID_CLASSIC_V6 ||
        id == SOCKET_SERVER_ID_SECURE_V6) {
        return true;
    }

    return false;
}

/**
 * Checks if the specified socket ID is for an IPv4 connection.
 *
 * @param id
 * ID to check.
 * @return
 * Whether the ID is for an IPv4 connection.
 */
static inline bool
server_socket_id_is_v4 (socket_server_id_t id)
{
    if (id == SOCKET_SERVER_ID_CLASSIC_V4 ||
        id == SOCKET_SERVER_ID_SECURE_V4) {
        return true;
    }

    return false;
}

/**
 * Initialize the socket server API.
 */
TOOLKIT_INIT_FUNC(socket_server)
{
    for (socket_server_id_t i = 0; i < SOCKET_SERVER_ID_NUM; i++) {
        uint16_t port;
        if (server_socket_id_is_secure(i)) {
            port = 14000; // TODO: config
        } else {
            port = settings.port;
        }

        // TODO: stack config (v6/v4/dual-stack)
        const char *host;
        if (server_socket_id_is_v6(i)) {
            host = "::";
        } else if (server_socket_id_is_v4(i)) {
            host = "0.0.0.0";
        } else {
            LOG(ERROR, "Reached impossible code branch");
            continue;
        }

        server_sockets[i] = socket_create(host, port);
        if (server_sockets[i] == NULL) {
            exit(1);
        }

        if (!socket_opt_linger(server_sockets[i], false, 0)) {
            exit(1);
        }

        if (!socket_opt_reuse_addr(server_sockets[i], true)) {
            exit(1);
        }

        if (!socket_bind(server_sockets[i])) {
            exit(1);
        }
    }

    client_sockets = NULL;
}
TOOLKIT_INIT_FUNC_FINISH

/**
 * Deinitialize the socket server API.
 */
TOOLKIT_DEINIT_FUNC(socket_server)
{
    for (int i = 0; i < SOCKET_SERVER_ID_NUM; i++) {
        if (server_sockets[i] == NULL) {
            continue;
        }

        socket_destroy(server_sockets[i]);
    }
}
TOOLKIT_DEINIT_FUNC_FINISH

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
static bool
socket_server_handle_command (socket_struct *cs,
                              player        *pl,
                              uint8_t       *data,
                              size_t         len)
{
    /* Skip the length data, because we already have it from the function
     * arguments. */
    size_t pos = 2;
    uint8_t type = packet_to_uint8(data, len, &pos);

#ifndef DEBUG
    char *cp;

    LOG(DUMPRX,
        "Received packet with command type %d (%" PRIu64 " bytes):",
        type, (uint64_t) len);
    cp = emalloc(sizeof(*cp) * (len * 3 + 1));
    string_tohex(data, len, cp, len * 3 + 1, true);
    LOG(DUMPRX, "  Hexadecimal: %s", cp);
    efree(cp);
#endif

    if (type >= SERVER_CMD_NROF || socket_commands[type].handle_func == NULL) {
        LOG(DEVEL, "Unknown command type: %" PRIu8, type);
        return true;
    }

    /* If the command is only for players and the client is not logged in yet,
     * do not handle the command. */
    if (socket_commands[type].flags & SOCKET_COMMAND_PLAYER_ONLY &&
        pl == NULL) {
        return false;
    }

    socket_commands[type].handle_func(cs, pl, data + pos, len - pos, 0);

    return true;
}

static void
socket_server_csocket_create (socket_t *server_socket)
{
    csocket_entry_t *entry = ecalloc(1, sizeof(*entry));
    entry->cs = ecalloc(1, sizeof(*entry->cs));
    entry->cs->sc = socket_accept(server_socket);
    if (entry->cs->sc == NULL) {
        efree(entry);
        return;
    }

    if (ban_check(entry->cs, NULL)) {
        LOG(SYSTEM, "Ban: Banned IP tried to connect: %s",
            socket_get_addr(entry->cs->sc));
        socket_destroy(entry->cs->sc);
        efree(entry);
        return;
    }

    init_connection(entry->cs);
    DL_APPEND(client_sockets, entry);
}

/**
 * Frees the specified client socket entry.
 *
 * @param entry
 * Entry to free.
 */
static void
socket_server_csocket_free (csocket_entry_t *entry)
{
    HARD_ASSERT(entry != NULL);
    free_newsocket(entry->cs);
    DL_DELETE(client_sockets, entry);
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
static void
socket_server_csocket_drop (csocket_entry_t *entry)
{
    HARD_ASSERT(entry != NULL);
    LOG(SYSTEM, "Connection: dropping connection: %s",
        socket_get_str(entry->cs->sc));
    socket_server_csocket_free(entry);
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
void
socket_server_handle_client (player *pl)
{
    HARD_ASSERT(pl != NULL);

    for (int num_cmds = 0;
         num_cmds < SOCKET_SERVER_PLAYER_MAX_COMMANDS;
         num_cmds++) {
        if (pl->cs->packet_recv_cmd->len == 0) {
            break;
        }

        /* Ensure the player is in a state capable of issue commands, and
         * has enough speed left to do so. */
        if (pl->cs->state == ST_ZOMBIE ||
            pl->cs->state == ST_DEAD ||
            (pl->cs->state == ST_PLAYING &&
             pl->ob != NULL &&
             pl->ob->speed_left < 0)) {
            break;
        }

        size_t len = 2 + (pl->cs->packet_recv_cmd->data[0] << 8) +
                     pl->cs->packet_recv_cmd->data[1];

        /* Reset idle counter. */
        if (pl->cs->state == ST_PLAYING) {
            pl->cs->login_count = 0;
            pl->cs->keepalive = 0;
        }

        socket_server_handle_command(pl->cs,
                                     pl,
                                     pl->cs->packet_recv_cmd->data,
                                     len);
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
bool
socket_server_remove (socket_struct *cs)
{
    csocket_entry_t *entry, *tmp;
    DL_FOREACH_SAFE(client_sockets, entry, tmp) {
        if (entry->cs == cs) {
            DL_DELETE(client_sockets, entry);
            efree(entry);
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
static inline bool
server_socket_csocket_is_zombie (socket_struct *cs)
{
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
static inline void
socket_server_csocket_read (socket_struct *cs)
{
    HARD_ASSERT(cs != NULL);

    if (cs->state == ST_DEAD) {
        return;
    }

    size_t amt;
    if (!socket_read(cs->sc,
                     (void *) (cs->packet_recv->data + cs->packet_recv->len),
                     cs->packet_recv->size - cs->packet_recv->len,
                     &amt)) {
        cs->state = ST_DEAD;
        return;
    }

    cs->packet_recv->len += amt;

    while (cs->packet_recv->len >= 2) {
        size_t size = 2 + (cs->packet_recv->data[0] << 8) +
                      cs->packet_recv->data[1];
        if (size > cs->packet_recv->len) {
            break;
        }

        /* Try to handle the command. */
        if (!socket_server_handle_command(cs,
                                          NULL,
                                          cs->packet_recv->data,
                                          size)) {
            /* Couldn't handle it immediately, add it to the commands
             * packet. */
            packet_append_data_len(cs->packet_recv_cmd,
                                   cs->packet_recv->data,
                                   size);
        }

        packet_delete(cs->packet_recv, 0, size);
    }
}

/**
 * Write out the packet queue to the specified client socket.
 *
 * @param cs
 * Client socket.
 */
static inline void
socket_server_csocket_write (socket_struct *cs)
{
    HARD_ASSERT(cs != NULL);

    while (cs->packets != NULL) {
        packet_struct *packet = cs->packets;

        if (packet->ndelay) {
            socket_opt_ndelay(cs->sc, true);
        }

        size_t amt;
        bool success = socket_write(cs->sc,
                                    (const void *) (packet->data + packet->pos),
                                    packet->len - packet->pos,
                                    &amt);

        if (packet->ndelay) {
            socket_opt_ndelay(cs->sc, false);
        }

        if (!success) {
            cs->state = ST_DEAD;
            break;
        }

        packet->pos += amt;

        if (packet->len - packet->pos == 0) {
            DL_DELETE(cs->packets, packet);
            packet_free(packet);
            continue;
        }

        /* Failed to send the entire packet; it's unlikely we can retry
         * immediately, so just stop here. */
        break;
    }
}

/**
 * Accept incoming connections, read data from clients and write data to
 * clients.
 */
void
socket_server_process (void)
{
    FD_ZERO(&fds_read);
    FD_ZERO(&fds_write);
    FD_ZERO(&fds_error);

    int nfds = 0;

    for (socket_server_id_t i = 0; i < SOCKET_SERVER_ID_NUM; i++) {
        if (server_sockets[i] == NULL) {
            continue;
        }

        int fd = socket_fd(server_sockets[i]);
        if (nfds < fd) {
            nfds = fd;
        }

        FD_SET(fd, &fds_read);
    }

    csocket_entry_t *entry, *entry_tmp;
    DL_FOREACH_SAFE(client_sockets, entry, entry_tmp) {
        if (unlikely(!socket_is_fd_valid(entry->cs->sc))) {
            LOG(ERROR, "Invalid waiting socket: %s",
                socket_get_str(entry->cs->sc));
            entry->cs->state = ST_DEAD;
        }

        if (entry->cs->state == ST_DEAD) {
            socket_server_csocket_drop(entry);
            continue;
        }

        if (server_socket_csocket_is_zombie(entry->cs)) {
            continue;
        }

        int fd = socket_fd(entry->cs->sc);
        if (nfds < fd) {
            nfds = fd;
        }

        FD_SET(fd, &fds_read);
        FD_SET(fd, &fds_write);
        FD_SET(fd, &fds_error);
    }

    player *pl, *pl_tmp;
    DL_FOREACH_SAFE(first_player, pl, pl_tmp) {
        if (pl->cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }

        if (unlikely(!socket_is_fd_valid(pl->cs->sc))) {
            LOG(ERROR, "Invalid waiting socket: %s",
                socket_get_str(pl->cs->sc));
            pl->cs->state = ST_DEAD;
        }

        if (pl->cs->keepalive++ >= SOCKET_KEEPALIVE_TIMEOUT) {
            LOG(SYSTEM, "Keepalive: disconnecting %s [%s]: %d",
                object_get_str(pl->ob),
                socket_get_str(pl->cs->sc),
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

        int fd = socket_fd(pl->cs->sc);
        if (nfds < fd) {
            nfds = fd;
        }

        FD_SET(fd, &fds_read);
        FD_SET(fd, &fds_write);
        FD_SET(fd, &fds_error);
    }

    int ready;
#ifdef HAVE_PSELECT
    static struct timespec timeout;
    /* pselect does not change the timeout argument, so we're OK with a
     * static storage duration one. */
    ready = pselect(nfds + 1,
                    &fds_read,
                    &fds_write,
                    &fds_error,
                    &timeout,
                    NULL);
#else
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    ready = select(nfds + 1,
                   &fds_read,
                   &fds_write,
                   &fds_error,
                   &timeout);
#endif
    if (unlikely(ready == -1)) {
        LOG(ERROR, "pselect/select() returned an error: %s (%d)",
            strerror(errno), errno);
        return;
    }

    /* No FDs that need processing. */
    if (ready == 0) {
        return;
    }

    for (socket_server_id_t i = 0; i < SOCKET_SERVER_ID_NUM; i++) {
        if (server_sockets[i] == NULL) {
            continue;
        }

        if (!FD_ISSET(socket_fd(server_sockets[i]), &fds_read)) {
            continue;
        }

        socket_server_csocket_create(server_sockets[i]);
    }

    DL_FOREACH_SAFE(client_sockets, entry, entry_tmp) {
        int fd = socket_fd(entry->cs->sc);

        if (FD_ISSET(fd, &fds_error)) {
            entry->cs->state = ST_DEAD;
            continue;
        } else if (FD_ISSET(fd, &fds_read)) {
            socket_server_csocket_read(entry->cs);
            continue;
        }

        if (entry->cs->state == ST_DEAD) {
            socket_server_csocket_drop(entry);
            continue;
        }

        if (FD_ISSET(fd, &fds_write)) {
            socket_server_csocket_write(entry->cs);
        }
    }

    DL_FOREACH_SAFE(first_player, pl, pl_tmp) {
        int fd = socket_fd(pl->cs->sc);

        if (FD_ISSET(fd, &fds_error)) {
            pl->cs->state = ST_DEAD;
            continue;
        } else if (FD_ISSET(fd, &fds_read)) {
            socket_server_csocket_read(pl->cs);
            continue;
        }

        if (pl->cs->state == ST_DEAD) {
            player_logout(pl);
            continue;
        }

        if (FD_ISSET(fd, &fds_write)) {
            socket_server_csocket_write(pl->cs);
        }
    }
}

/**
 * Update player socket-related data, render the map for them, etc.
 * Afterwards, attempt to write to the players' clients.
 */
void
socket_server_post_process (void)
{
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

            uint32_t update_tile = GET_MAP_UPDATE_COUNTER(pl->ob->map,
                                                          pl->ob->x,
                                                          pl->ob->y);
            if (update_tile != pl->cs->update_tile) {
                esrv_draw_look(pl->ob);
                pl->cs->update_tile = update_tile;
            }
        }

        if (FD_ISSET(socket_fd(pl->cs->sc), &fds_write)) {
            socket_server_csocket_write(pl->cs);
        }
    }
}

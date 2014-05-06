/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Mainly deals with initialization and higher level socket maintenance
 * (checking for lost connections and if data has arrived). */

#include <global.h>

static fd_set tmp_read, tmp_exceptions, tmp_write;

typedef struct socket_command_struct
{
    void (*handle_func)(socket_struct *, player *, uint8 *, size_t, size_t);

    int flags;
} socket_command_struct;

#define SOCKET_CMD_PLAYER_ONLY 1

static const socket_command_struct socket_commands[SERVER_CMD_NROF] =
{
    {socket_command_control, 0},
    {socket_command_ask_face, 0},
    {socket_command_setup, 0},
    {socket_command_version, 0},
    {socket_command_request_file, 0},
    {socket_command_clear, 0},
    {socket_command_request_update, 0},
    {socket_command_keepalive, 0},
    {socket_command_account, 0},
    {socket_command_item_examine, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_item_apply, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_item_move, SOCKET_CMD_PLAYER_ONLY},
    {NULL, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_player_cmd, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_item_lock, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_item_mark, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_fire, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_quickslot, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_quest_list, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_move_path, SOCKET_CMD_PLAYER_ONLY},
    {NULL, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_talk, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_move, SOCKET_CMD_PLAYER_ONLY},
    {socket_command_target, SOCKET_CMD_PLAYER_ONLY}
};

static int socket_command_check(socket_struct *ns, player *pl, uint8 *data, size_t len)
{
    size_t pos;
    uint8 type;

    pos = 2;
    type = packet_to_uint8(data, len, &pos);

    if (type >= SERVER_CMD_NROF || !socket_commands[type].handle_func) {
        return -1;
    }

    if (socket_commands[type].flags & SOCKET_CMD_PLAYER_ONLY && !pl) {
        return 0;
    }

    socket_commands[type].handle_func(ns, pl, data + pos, len - pos, 0);

    return 1;
}

/**
 * Fill a command buffer with data we read from socket.
 * @param ns Socket. */
static void fill_command_buffer(socket_struct *ns)
{
    size_t toread;

    do
    {
        toread = 0;

        if (ns->state == ST_DEAD) {
            break;
        }

        if (ns->packet_recv->len >= 2) {
            toread = 2 + (ns->packet_recv->data[0] << 8) + ns->packet_recv->data[1];

            if (toread <= ns->packet_recv->len) {
                if (socket_command_check(ns, NULL, ns->packet_recv->data, toread) == 0) {
                    packet_append_data_len(ns->packet_recv_cmd, ns->packet_recv->data, toread);
                }

                packet_delete(ns->packet_recv, 0, toread);
            }
        }
    }
    while (toread);
}

/**
 * Handle client commands.
 *
 * We only get here once there is input, and only do basic connection
 * checking.
 * @param ns Socket sending the command.
 * @param pl Player associated to the socket. If NULL, only commands in
 * client_commands will be checked. */
void handle_client(socket_struct *ns, player *pl)
{
    size_t len;
    int cmd_count = 0;

    while (1) {
        if (ns->packet_recv_cmd->len == 0) {
            break;
        }

        /* If it is a player, and they don't have any speed left, we
        * return, and will parse the data when they do have time. */
        if (ns->state == ST_ZOMBIE || ns->state == ST_DEAD || (pl && pl->socket.state == ST_PLAYING && pl->ob && pl->ob->speed_left < 0)) {
            break;
        }

        len = 2 + (ns->packet_recv_cmd->data[0] << 8) + ns->packet_recv_cmd->data[1];

        /* Reset idle counter. */
        if (pl && pl->socket.state == ST_PLAYING) {
            ns->login_count = 0;
            ns->keepalive = 0;
        }

        socket_command_check(ns, pl, ns->packet_recv_cmd->data, len);
        packet_delete(ns->packet_recv_cmd, 0, len);

        if (cmd_count++ <= 8) {
            continue;
        }

        break;
    }
}

/**
 * Remove a player from the game that has been disconnected by logging
 * out, the socket connection was interrupted, etc.
 * @param pl The player to remove. */
void remove_ns_dead_player(player *pl)
{
    if (pl == NULL || pl->ob->type == DEAD_OBJECT) {
        return;
    }

    /* Trigger the global LOGOUT event */
    trigger_global_event(GEVENT_LOGOUT, pl->ob, pl->socket.host);
    statistics_player_logout(pl);

    draw_info_format(COLOR_DK_ORANGE, NULL, "%s left the game.", pl->ob->name);

    /* If this player is in a party, leave the party */
    if (pl->party) {
        command_party(pl->ob, "party", "leave");
    }

    snprintf(pl->killer, sizeof(pl->killer), "left");
    hiscore_check(pl->ob, 1);

    /* Be sure we have closed container when we leave */
    container_close(pl->ob, NULL);

    player_save(pl->ob);
    account_logout_char(&pl->socket, pl);
    leave_map(pl->ob);

    logger_print(LOG(INFO), "Logout %s from IP %s", pl->ob->name, pl->socket.host);

    /* To avoid problems with inventory window */
    pl->ob->type = DEAD_OBJECT;
    free_player(pl);
}

/**
 * Checks if file descriptor is valid.
 * @param fd File descriptor to check.
 * @return 1 if fd is valid, 0 else. */
static int is_fd_valid(int fd)
{
#ifndef WIN32
    return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
#else
    return 1;
#endif
}

/**
 * Common way to free a socket. Frees the socket from ::init_sockets,
 * sets its status to Ns_Avail and decrements number of connections. */
#define FREE_SOCKET(i) \
    free_newsocket(&init_sockets[(i)]); \
    init_sockets[(i)].state = ST_AVAILABLE; \
    socket_info.nconns--;

/**
 * This checks the sockets for input and exceptions, does the right
 * thing.
 *
 * There are 2 lists we need to look through - init_sockets is a list */
void doeric_server(void)
{
    int i, pollret, rr;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    player *pl, *next;

    FD_ZERO(&tmp_read);
    FD_ZERO(&tmp_write);
    FD_ZERO(&tmp_exceptions);

    for (i = 0; i < socket_info.allocated_sockets; i++) {
        if (init_sockets[i].state == ST_LOGIN && !is_fd_valid(init_sockets[i].fd)) {
            logger_print(LOG(DEBUG), "Invalid waiting fd %d", i);
            init_sockets[i].state = ST_DEAD;
        }

        if (init_sockets[i].state == ST_DEAD) {
            FREE_SOCKET(i);
        }
        else if (init_sockets[i].state == ST_ZOMBIE) {
            if (init_sockets[i].login_count++ >= 1000000 / MAX_TIME) {
                init_sockets[i].state = ST_DEAD;
            }
        }
        else if (init_sockets[i].state != ST_AVAILABLE) {
            if (init_sockets[i].state > ST_WAITING) {
                if (init_sockets[i].keepalive++ >= (uint32) SOCKET_KEEPALIVE_TIMEOUT * (1000000 / max_time)) {
                    logger_print(LOG(INFO), "Keepalive: disconnecting %s: %d", init_sockets[i].host ? init_sockets[i].host : "(unknown ip?)", init_sockets[i].fd);
                    FREE_SOCKET(i);
                    continue;
                }
            }

            FD_SET((uint32) init_sockets[i].fd, &tmp_read);
            FD_SET((uint32) init_sockets[i].fd, &tmp_write);
            FD_SET((uint32) init_sockets[i].fd, &tmp_exceptions);
        }
    }

    /* Go through the players. Let the loop set the next pl value, since
     * we may remove some. */
    for (pl = first_player; pl != NULL; ) {
        if (pl->socket.state != ST_DEAD && !is_fd_valid(pl->socket.fd)) {
            logger_print(LOG(DEBUG), "Invalid file descriptor for player %s [%s]: %d", (pl->ob && pl->ob->name) ? pl->ob->name : "(unnamed player?)", (pl->socket.host) ? pl->socket.host : "(unknown ip?)", pl->socket.fd);
            pl->socket.state = ST_DEAD;
        }

        if (pl->socket.state != ST_DEAD && pl->socket.keepalive++ >= (uint32) SOCKET_KEEPALIVE_TIMEOUT * (1000000 / max_time)) {
            logger_print(LOG(INFO), "Keepalive: disconnecting %s [%s]: %d", (pl->ob && pl->ob->name) ? pl->ob->name : "(unnamed player?)", (pl->socket.host) ? pl->socket.host : "(unknown ip?)", pl->socket.fd);
            pl->socket.state = ST_DEAD;
        }

        if (pl->socket.state == ST_DEAD) {
            player *npl = pl->next;

            remove_ns_dead_player(pl);
            pl = npl;
        }
        else if (pl->socket.state == ST_ZOMBIE) {
            if (pl->socket.login_count++ >= 1000000 / MAX_TIME) {
                pl->socket.state = ST_DEAD;
            }
        }
        else {
            FD_SET((uint32) pl->socket.fd, &tmp_read);
            FD_SET((uint32) pl->socket.fd, &tmp_write);
            FD_SET((uint32) pl->socket.fd, &tmp_exceptions);
            pl = pl->next;
        }
    }

    socket_info.timeout.tv_sec = 0;
    socket_info.timeout.tv_usec = 0;

    pollret = select(socket_info.max_filedescriptor, &tmp_read, &tmp_write, &tmp_exceptions, &socket_info.timeout);

    if (pollret == -1) {
        logger_print(LOG(DEBUG), "select failed: %s", strerror(errno));
        return;
    }

    /* Following adds a new connection */
    if (pollret && FD_ISSET(init_sockets[0].fd, &tmp_read)) {
        int newsocknum = 0;

        /* If this is the case, all sockets are currently in use */
        if (socket_info.allocated_sockets <= socket_info.nconns) {
            init_sockets = erealloc(init_sockets, sizeof(socket_struct) * (socket_info.nconns + 1));
            newsocknum = socket_info.allocated_sockets;
            socket_info.allocated_sockets++;
            init_sockets[newsocknum].state = ST_AVAILABLE;
        }
        else {
            int j;

            for (j = 1; j < socket_info.allocated_sockets; j++) {
                if (init_sockets[j].state == ST_AVAILABLE) {
                    newsocknum = j;
                    break;
                }
            }
        }

        init_sockets[newsocknum].fd = accept(init_sockets[0].fd, (struct sockaddr *) &addr, &addrlen);

        if (init_sockets[newsocknum].fd == -1) {
            logger_print(LOG(DEBUG), "accept failed: %s", strerror(errno));
        }
        else {
            char buf[MAX_BUF];
            long ip = ntohl(addr.sin_addr.s_addr);

            snprintf(buf, sizeof(buf), "%ld.%ld.%ld.%ld", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);

            if (checkbanned(NULL, buf)) {
                logger_print(LOG(SYSTEM), "Ban: Banned IP tried to connect: %s", buf);
#ifndef WIN32
                close(init_sockets[newsocknum].fd);
#else
                shutdown(init_sockets[newsocknum].fd, SD_BOTH);
                closesocket(init_sockets[newsocknum].fd);
#endif
                init_sockets[newsocknum].fd = -1;
            }
            else {
                init_connection(&init_sockets[newsocknum], buf);
                socket_info.nconns++;
            }
        }
    }

    /* Check for any exceptions/input on the sockets */
    if (pollret) {
        for (i = 1; i < socket_info.allocated_sockets; i++) {
            if (init_sockets[i].state == ST_AVAILABLE) {
                continue;
            }

            if (FD_ISSET(init_sockets[i].fd, &tmp_exceptions)) {
                FREE_SOCKET(i);
                continue;
            }

            if (FD_ISSET(init_sockets[i].fd, &tmp_read)) {
                rr = socket_recv(&init_sockets[i]);

                if (rr < 0) {
                    logger_print(LOG(INFO), "Drop connection: %s", STRING_SAFE(init_sockets[i].host));
                    init_sockets[i].state = ST_DEAD;
                }
                else {
                    fill_command_buffer(&init_sockets[i]);
                }
            }

            if (init_sockets[i].state == ST_AVAILABLE) {
                continue;
            }

            if (init_sockets[i].state == ST_DEAD) {
                FREE_SOCKET(i);
                continue;
            }

            if (FD_ISSET(init_sockets[i].fd, &tmp_write)) {
                socket_buffer_write(&init_sockets[i]);
            }

            if (init_sockets[i].state == ST_DEAD) {
                FREE_SOCKET(i);
            }
        }
    }

    /* This does roughly the same thing, but for the players now */
    for (pl = first_player; pl; pl = next) {
        next = pl->next;

        /* Kill players if we have problems */
        if (pl->socket.state == ST_DEAD || FD_ISSET(pl->socket.fd, &tmp_exceptions)) {
            remove_ns_dead_player(pl);
            continue;
        }

        if (FD_ISSET(pl->socket.fd, &tmp_read)) {
            rr = socket_recv(&pl->socket);

            if (rr < 0) {
                logger_print(LOG(INFO), "Drop connection: %s (%s)", STRING_OBJ_NAME(pl->ob), STRING_SAFE(pl->socket.host));
                pl->socket.state = ST_DEAD;
            }
            else {
                fill_command_buffer(&pl->socket);
            }
        }

        /* Perhaps something was bad in handle_client(), or player has
         * left the game. */
        if (pl->socket.state == ST_DEAD) {
            remove_ns_dead_player(pl);
            continue;
        }

        if (FD_ISSET(pl->socket.fd, &tmp_write)) {
            socket_buffer_write(&pl->socket);
        }
    }
}

/**
 * Write to players' sockets. */
void doeric_server_write(void)
{
    player *pl, *next;
    uint32 update_below;

    /* This does roughly the same thing, but for the players now */
    for (pl = first_player; pl; pl = next) {
        next = pl->next;

        if (pl->socket.state == ST_DEAD || FD_ISSET(pl->socket.fd, &tmp_exceptions)) {
            remove_ns_dead_player(pl);
            continue;
        }

        esrv_update_stats(pl);
        party_update_who(pl);

        draw_client_map(pl->ob);

        if (pl->ob->map && (update_below = GET_MAP_UPDATE_COUNTER(pl->ob->map, pl->ob->x, pl->ob->y)) != pl->socket.update_tile) {
            esrv_draw_look(pl->ob);
            pl->socket.update_tile = update_below;
        }

        if (FD_ISSET(pl->socket.fd, &tmp_write)) {
            socket_buffer_write(&pl->socket);
        }
    }
}

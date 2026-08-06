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
 * Client interface main routine.
 *
 * This file sets up a few global variables, connects to the server,
 * tells it what kind of pictures it wants, adds the client and enters
 * the main event loop (event_loop()) checks the game socket for input and
 * then polls for x events. This should be fixed since you can just block
 * on both filedescriptors.
 *
 * The DoClient function receives a message (an ArgList), unpacks it, and
 * in a slow for loop dispatches the command to the right function
 * through the commands table. ArgLists are essentially like RPC things,
 * only they don't require going through RPCgen, and it's easy to get
 * variable length lists. They are just lists of longs, strings,
 * characters, and byte arrays that can be converted to a machine
 * independent format.
 */

#include <global.h>
#include <resources.h>
#include <toolkit/datetime.h>
#include <toolkit/packet.h>

/** Maximum time spent draining server commands before yielding to rendering. */
#define CLIENT_COMMAND_BUDGET_US UINT64_C(4000)

/** Client player structure with things like stats, damage, etc */
Client_Player cpl;

/** Structure of all the socket commands */
static socket_command_struct commands[CLIENT_CMD_NROF] = {
#define ATRINIK_CLIENT_COMMAND(_id, _name, _handler) \
    [CLIENT_CMD_##_id] = {.handle_func = (_handler), .name = (_name)},
#include <toolkit/socket_commands.def>
#undef ATRINIK_CLIENT_COMMAND
};
CASSERT_ARRAY(commands, CLIENT_CMD_NROF);

/**
 * Do client. The main loop for commands. From this, the data and
 * commands from server are received.
 */
void DoClient(void) {
    command_buffer *cmd;
    uint64_t commands_started = datetime_monotonic_us();

    /* Handle all enqueued commands */
    while ((cmd = get_next_input_command()) != NULL) {
        uint8_t *data = cmd->data;
        size_t len = cmd->len;

        size_t pos = 0;
        packet_reader_t reader;
        packet_reader_init_cursor(&reader, data, len, &pos);
        uint8_t type = packet_reader_read_uint8(&reader);

        if (packet_reader_error(&reader) != PACKET_ERROR_NONE) {
            LOG(ERROR, "Rejected command envelope: %s", packet_error_string(reader.error));
        } else if (type >= CLIENT_CMD_NROF || commands[type].handle_func == NULL) {
            LOG(ERROR, "Bad command from server (%d)", type);
        } else {
            packet_reader_scope_t scope;
            packet_reader_scope_begin(&scope);
            packet_reader_init_at(&reader, data, len, pos);
            commands[type].handle_func(data, len, pos);
            packet_error_t error = packet_reader_scope_finish(&scope);
            if (error != PACKET_ERROR_NONE) {
                LOG(ERROR,
                    "Rejected malformed %s command: %s",
                    commands[type].name,
                    packet_error_string(error));
            }
        }

        command_buffer_free(cmd);

        /* A sustained stream of multi-level map updates must not starve the
         * main loop's render/present phases. Packet order is retained; the
         * remaining queue resumes on the next frame. Always finish at least
         * the command already dequeued, even when it exceeds this budget. */
        if (datetime_monotonic_us() - commands_started >= CLIENT_COMMAND_BUDGET_US) {
            break;
        }
    }
}

/**
 * Check animation status.
 * @param anum
 * Animation ID.
 */
bool check_animation_status(int anum) {
    if (anum < 0 || (size_t)anum >= animations_num || animations == NULL || anim_table == NULL) {
        LOG(ERROR,
            "Ignoring invalid animation ID %d (count: %" PRIu64 ")",
            anum,
            (uint64_t)animations_num);
        return false;
    }

    if (animations[anum].loaded) {
        return true;
    }

    if (anim_table[anum].anim_cmd == NULL || anim_table[anum].len < 4) {
        LOG(ERROR, "Animation %d has no valid command data", anum);
        return false;
    }

    /* Same as server sends it. */
    socket_command_anim(anim_table[anum].anim_cmd, anim_table[anum].len, 0);

    return animations[anum].loaded;
}

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
 * Client interface main routine.
 *
 * This file sets up a few global variables, connects to the server,
 * tells it what kind of pictures it wants, adds the client and enters
 * the main event loop (event_loop()) checks the tcp socket for input and
 * then polls for x events. This should be fixed since you can just block
 * on both filedescriptors.
 *
 * The DoClient function receives a message (an ArgList), unpacks it, and
 * in a slow for loop dispatches the command to the right function
 * through the commands table. ArgLists are essentially like RPC things,
 * only they don't require going through RPCgen, and it's easy to get
 * variable length lists. They are just lists of longs, strings,
 * characters, and byte arrays that can be converted to a machine
 * independent format. */

#include <global.h>

/** Client player structure with things like stats, damage, etc */
Client_Player cpl;

/** Client socket. */
ClientSocket csocket;

/** Structure of all the socket commands */
static socket_command_struct commands[CLIENT_CMD_NROF] = {
    {socket_command_map},
    {socket_command_drawinfo},
    {socket_command_file_update},
    {socket_command_item},
    {socket_command_sound},
    {socket_command_target},
    {socket_command_item_update},
    {socket_command_item_delete},
    {socket_command_stats},
    {socket_command_image},
    {socket_command_anim},
    {NULL},
    {socket_command_player},
    {socket_command_mapstats},
    {NULL},
    {socket_command_version},
    {socket_command_setup},
    {socket_command_control},
    {NULL},
    {socket_command_characters},
    {socket_command_book},
    {socket_command_party},
    {socket_command_quickslots},
    {socket_command_compressed},
    {socket_command_region_map},
    {socket_command_sound_ambient},
    {socket_command_interface},
    {socket_command_notification}
};

/**
 * Do client. The main loop for commands. From this, the data and
 * commands from server are received. */
void DoClient(void)
{
    command_buffer *cmd;

    /* Handle all enqueued commands */
    while ((cmd = get_next_input_command())) {
        if (cmd->data[0] >= CLIENT_CMD_NROF || !commands[cmd->data[0]].handle_func) {
            logger_print(LOG(BUG), "Bad command from server (%d)", cmd->data[0]);
        } else {
            commands[cmd->data[0]].handle_func(cmd->data + 1, cmd->len - 1, 0);
        }

        command_buffer_free(cmd);
    }
}

/**
 * Check animation status.
 * @param anum Animation ID. */
void check_animation_status(int anum)
{
    /* Check if it has been loaded. */
    if (animations[anum].loaded) {
        return;
    }

    /* Mark this animation as loaded. */
    animations[anum].loaded = 1;

    /* Same as server sends it */
    socket_command_anim(anim_table[anum].anim_cmd, anim_table[anum].len, 0);
}

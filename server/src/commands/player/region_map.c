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
 * Implements the /region_map command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_region_map(object *op, const char *command, char *params)
{
    uint8 params_check;
    region *r;
    packet_struct *packet;

    if (!op->map) {
        return;
    }

    /* Server has not configured client maps URL. */
    if (*settings.client_maps_url == '\0') {
        draw_info(COLOR_WHITE, op, "This server does not support that command.");
        return;
    }

    /* Check if params were given and whether the player is allowed to
     * see map of any region they want. */
    params_check = params && commands_check_permission(CONTR(op), command);

    if (params_check) {
        /* Search for the region the player wants. */
        for (r = first_region; r; r = r->next) {
            if (!strcasecmp(r->name, params)) {
                break;
            }
        }

        /* Not found, try partial region names. */
        if (!r) {
            size_t params_len = strlen(params);

            for (r = first_region; r; r = r->next) {
                if (!strncasecmp(r->name, params, params_len)) {
                    break;
                }
            }
        }

        if (!r) {
            draw_info(COLOR_WHITE, op, "No such region.");
            return;
        }
    }
    else {
        r = op->map->region;
    }

    /* Try to find a region that should have had a client map
     * generated. */
    for (; r; r = r->parent) {
        if (r->map_first) {
            break;
        }
    }

    if (!r && params_check) {
        draw_info(COLOR_WHITE, op, "That region doesn't have a map.");
        return;
    }

    if (!r || (r->map_quest && !params_check && !player_has_region_map(CONTR(op), r))) {
        draw_info(COLOR_WHITE, op, "You don't have a map of this region.");
        return;
    }

    packet = packet_new(CLIENT_CMD_REGION_MAP, 256, 256);
    packet_append_string_terminated(packet, op->map->path);
    packet_append_uint16(packet, op->x);
    packet_append_uint16(packet, op->y);
    packet_append_string_terminated(packet, r->name);
    packet_append_string_terminated(packet, settings.client_maps_url);

    if (CONTR(op)->socket.socket_version >= 1058) {
        packet_append_string_terminated(packet, r->longname);
        packet_append_uint8(packet, op->direction);
    }

    socket_send_packet(&CONTR(op)->socket, packet);
}

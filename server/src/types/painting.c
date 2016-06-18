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
 * Handles code for @ref PAINTING "paintings".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object_methods.h>
#include <object.h>
#include <resources.h>

#include "player.h"

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    if (op->slaying == NULL) {
        LOG(ERROR, "Painting object %s has no slaying attribute",
            object_get_str(op));
        return OBJECT_METHOD_ERROR;
    }

    resource_t *resource = resources_find(op->slaying);
    if (resource == NULL) {
        LOG(ERROR, "Painting object %s has an invalid slaying attribute: '%s'",
            object_get_str(op),
            op->slaying);
        return OBJECT_METHOD_ERROR;
    }

    resources_send(resource, CONTR(applier)->cs);

    packet_struct *packet = packet_new(CLIENT_CMD_PAINTING, 256, 0);
    packet_append_string_terminated(packet, op->slaying);
    packet_append_string_terminated(packet, op->name);

    if (op->msg != NULL) {
        packet_append_string_terminated(packet, op->msg);
    }

    socket_send_packet(CONTR(applier)->cs, packet);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the pants type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(painting)
{
    OBJECT_METHODS(PAINTING)->apply_func = apply_func;
}

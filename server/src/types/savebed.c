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
 * Handles code used for @ref SAVEBED "savebeds". */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    (void) op;
    (void) aflags;

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    /* Update respawn position. */
    snprintf(VS(CONTR(applier)->savebed_map), "%s", applier->map->path);
    CONTR(applier)->bed_x = applier->x;
    CONTR(applier)->bed_y = applier->y;

    draw_info(COLOR_WHITE, applier, "You save and your save bed location is updated.");
    hiscore_check(applier, 0);
    player_save(applier);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the savebed type object methods. */
void object_type_init_savebed(void)
{
    object_type_methods[SAVEBED].apply_func = apply_func;
}

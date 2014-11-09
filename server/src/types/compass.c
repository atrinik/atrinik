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
 * Handles code related to @ref COMPASS "compasses".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    const char *direction_names[] = {"north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest"};

    (void) op;
    (void) aflags;

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_OK;
    }

    draw_info_format(COLOR_WHITE, applier, "You are facing %s.", direction_names[absdir(applier->direction) - 1]);
    return OBJECT_METHOD_OK;
}

/**
 * Initialize the compass type object methods. */
void object_type_init_compass(void)
{
    object_type_methods[COMPASS].apply_func = apply_func;
}

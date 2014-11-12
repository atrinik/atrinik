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
 * Implements the /resetmap command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_resetmap(object *op, const char *command, char *params)
{
    mapstruct *m, *newmap;

    if (!params) {
        m = op->map;
    }
    else {
        shstr *mapfile_sh;

        mapfile_sh = add_string(params);
        m = has_been_loaded_sh(mapfile_sh);
        free_string_shared(mapfile_sh);
    }

    if (!m) {
        draw_info(COLOR_WHITE, op, "No such map.");
        return;
    }

    newmap = map_force_reset(m);

    if (!newmap) {
        draw_info_format(COLOR_WHITE, op, "Could not reset map: %s", m->path);
        return;
    }

    draw_info_format(COLOR_WHITE, op, "Successfully reset map: %s", newmap->path);
}

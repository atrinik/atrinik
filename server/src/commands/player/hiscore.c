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
 * Implements the /hiscore command.
 *
 * @author Alex Tokar */

#include <global.h>
#include <toolkit_string.h>

/** @copydoc command_func */
void command_hiscore(object *op, const char *command, char *params)
{
    int results;

    results = 0;

    if (params) {
        if (string_isdigit(params)) {
            results = atoi(params);
        } else {
            if (strlen(params) < settings.limits[ALLOWED_CHARS_CHARNAME][0]) {
                draw_info_format(COLOR_WHITE, op, "Your search term must be at least %"PRIu64 " characters long.", (uint64_t) settings.limits[ALLOWED_CHARS_CHARNAME][0]);
                return;
            }
        }
    }

    results = MAX(25, MIN(50, results));
    hiscore_display(op, results, params);
}

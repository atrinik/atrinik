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
 * Implements the /config command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <clioptions.h>

/** @copydoc command_func */
void
command_config (object *op, const char *command, char *params)
{
    if (params == NULL) {
        draw_info(COLOR_WHITE, op,
                  "Usage: /config <name>, /config <name> = <value>");
        return;
    }

    if (strchr(params, '=') == NULL) {
        const char *value = clioptions_get(params);
        if (value == NULL) {
            draw_info_format(COLOR_WHITE, op,
                             "No such option: %s",
                             params);
        } else {
            draw_info_format(COLOR_WHITE, op,
                             "Configuration: %s = %s",
                             params, value);
        }
    } else {
        char *errmsg;
        if (clioptions_load_str(params, &errmsg)) {
            draw_info(COLOR_WHITE, op, "Configuration successful.");
        } else {
            draw_info_format(COLOR_WHITE, op,
                             "Configuration failed: %s",
                             errmsg != NULL ? errmsg : "<no error message>");
            if (errmsg != NULL) {
                efree(errmsg);
            }
        }
    }
}

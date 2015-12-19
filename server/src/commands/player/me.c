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
 * Implements the /me command.
 *
 * @author Alex Tokar
 */

#include <global.h>

/** @copydoc command_func */
void command_me(object *op, const char *command, char *params)
{
    char buf[HUGE_BUF];

    params = player_sanitize_input(params);

    if (!params) {
        return;
    }

    LOG(CHAT, "[ME] [%s] %s", op->name, params);
    snprintf(buf, sizeof(buf), "[a=#charname]%s[/a] %s", op->name, params);
    draw_info_map(CHAT_TYPE_LOCAL, NULL, COLOR_YELLOW, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, NULL, buf);
}

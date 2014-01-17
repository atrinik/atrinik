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
 * Implements the /tpto command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_tpto(object *op, const char *command, char *params)
{
    char path[MAX_BUF], word[MAX_BUF];
    size_t pos;
    mapstruct *m;
    int x, y;

    params = player_sanitize_input(params);
    pos = 0;

    if (!params || !string_get_word(params, &pos, ' ', path, sizeof(path), 0)) {
        return;
    }

    x = y = -1;

    if (string_get_word(params, &pos, ' ', word, sizeof(word), 0) && string_isdigit(word)) {
        x = atoi(word);
    }

    if (string_get_word(params, &pos, ' ', word, sizeof(word), 0) && string_isdigit(word)) {
        y = atoi(word);
    }

    m = ready_map_name(path, 0);

    if (!m) {
        draw_info_format(COLOR_WHITE, op, "No such map: %s", path);
        return;
    }

    object_enter_map(op, NULL, m, x, y, 0);
}

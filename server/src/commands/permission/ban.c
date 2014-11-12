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
 * Implements the /ban command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_ban(object *op, const char *command, char *params)
{
    size_t pos;
    char word[MAX_BUF];

    pos = 0;

    if (!string_get_word(params, &pos, ' ', word, sizeof(word), '"')) {
        return;
    }

    params = player_sanitize_input(params + pos);

    if (strcmp(word, "add") == 0) {
        if (!params) {
            return;
        }

        if (add_ban(params)) {
            draw_info(COLOR_GREEN, op, "Added new ban successfully.");
        }
        else {
            draw_info(COLOR_RED, op, "Failed to add new ban!");
        }
    }
    else if (strcmp(word, "remove") == 0) {
        if (!params) {
            return;
        }

        if (remove_ban(params)) {
            draw_info(COLOR_GREEN, op, "Removed ban successfully.");
        }
        else {
            draw_info(COLOR_RED, op, "Failed to remove ban!");
        }
    }
    else if (strcmp(word, "list") == 0) {
        list_bans(op);
    }
}

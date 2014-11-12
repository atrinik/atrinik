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
 * Implements the /freeze command.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc command_func */
void command_freeze(object *op, const char *command, char *params)
{
    char word[MAX_BUF];
    size_t pos;
    player *pl;
    int ticks;

    pos = 0;

    if (!string_get_word(params, &pos, ' ', word, sizeof(word), '"')) {
        draw_info(COLOR_WHITE, op, "Usage: /freeze <player> &lsqb;ticks&rsqb;");
        return;
    }

    pl = find_player(word);

    if (!pl) {
        draw_info(COLOR_WHITE, op, "No such player.");
        return;
    }

    if (string_get_word(params, &pos, ' ', word, sizeof(word), 0) && string_isdigit(word)) {
        ticks = atoi(word);
    }
    else {
        ticks = 100;
    }

    draw_info(COLOR_RED, pl->ob, "You feel a mystical power binding you in place...");
    draw_info_format(COLOR_WHITE, op, "You freeze %s for %d ticks.", pl->ob->name, ticks);

    pl->ob->speed_left = -(pl->ob->speed * ticks);
}

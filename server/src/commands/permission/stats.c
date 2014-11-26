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
 * Implements the /stats command.
 *
 * @author Alex Tokar
 */

#include <global.h>

/** @copydoc command_func */
void command_stats(object *op, const char *command, char *params)
{
    size_t pos;
    char type[MAX_BUF], buf[HUGE_BUF];

    pos = 0;

    if (!string_get_word(params, &pos, ' ', type, sizeof(type), 0)) {
        snprintf(VS(buf), "Usage: /stats <stats type>");
    } else if (strcmp(type, "mempool") == 0) {
        char pool[MAX_BUF];

        if (!string_get_word(params, &pos, ' ', pool, sizeof(pool), 0)) {
            snprintf(VS(buf), "Usage: /stats mempool <pool name>");
        } else if (strcmp(pool, "object") == 0) {
            mempool_stats(pool_object, VS(buf));
        } else if (strcmp(pool, "packet") == 0) {
            mempool_stats(pool_packet, VS(buf));
        } else {
            snprintf(VS(buf), "Unknown memory pool name: %s", pool);
        }
    } else if (strcmp(type, "shstr") == 0) {
        shstr_stats(VS(buf));
    } else if (strcmp(type, "metaserver") == 0) {
        metaserver_stats(VS(buf));
    } else {
        snprintf(VS(buf), "Unknown stats type: %s", type);
    }

    draw_info(COLOR_WHITE, op, buf);
}

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
 * Implements the /statistics command.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <toolkit_string.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <exp.h>

/** @copydoc command_func */
void command_statistics(object *op, const char *command, char *params)
{
    size_t i;

    draw_info_format(COLOR_WHITE, op, "Experience: %s", string_format_number_comma(op->stats.exp));

    if (op->level < MAXLEVEL) {
        char *cp;

        cp = estrdup(string_format_number_comma(level_exp(op->level + 1, 1.0)));
        draw_info_format(COLOR_WHITE, op, "Next Level:  %s (%s)", cp, string_format_number_comma(level_exp(op->level + 1, 1.0) - op->stats.exp));
        efree(cp);
    }

    draw_info(COLOR_WHITE, op, "\nStat: Natural (Real)");

    for (i = 0; i < NUM_STATS; i++) {
        draw_info_format(COLOR_WHITE, op, "[green]%s:[/green] %d (%d)", short_stat_name[i], get_attr_value(&op->arch->clone.stats, i), get_attr_value(&op->stats, i));
    }

    draw_info_format(COLOR_WHITE, op, "\nYour equipped item power is %d out of %d.", CONTR(op)->item_power, op->level);
}

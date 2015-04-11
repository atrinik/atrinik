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
 * Handles code for @ref POISONING "poisoning" objects.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    if (!op->env || !IS_LIVE(op->env) || op->env->stats.hp < 0) {
        object_remove(op, 0);
        object_destroy(op);
        return;
    }

    /* If we successfully do damage to the player, the poison effects
     * worsen... */
    if (hit_player(op->env, op->stats.dam, op, AT_INTERNAL) && op->env->type == PLAYER) {
        int i;

        /* Pick some stats to 'deplete'. */
        for (i = 0; i < NUM_STATS; i++) {
            if (!(RANDOM() % 2) && get_attr_value(&op->stats, i) > -(MAX_STAT / 2)) {
                /* Now deplete the stat. Relatively small chance that the
                 * depletion
                 * will be worse than usual. */
                change_attr_value(&op->stats, i, !(RANDOM() % 6) ? -2 : -1);
                draw_info(COLOR_GRAY, op->env, lose_msg[i]);
            }
        }

        living_update(op->env);
    }
}

/**
 * Initialize the poisoning type object methods. */
void object_type_init_poisoning(void)
{
    object_type_methods[POISONING].process_func = process_func;
}

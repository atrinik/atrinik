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
 * Handles code for @ref SHOP_FLOOR "shop floor".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object_methods.h>
#include <object.h>

/** @copydoc object_methods_t::auto_apply_func */
static void
auto_apply_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->randomitems == NULL) {
        return;
    }

    int a_chance;
    /* If damned shop floor, force 0 artifact chance. */
    if (QUERY_FLAG(op, FLAG_DAMNED)) {
        a_chance = 0;
    } else {
        a_chance = op->randomitems->artifact_chance;
    }

    int level;
    if (op->stats.exp != 0) {
        level = op->stats.exp;
    } else {
        level = get_environment_level(op);
    }

    object *tmp = NULL;
    do {
        /* Give it ten tries. */
        int i = 10;
        while (tmp == NULL && i != 0) {
            tmp = treasure_generate_single(op->randomitems, level, a_chance);
            i--;
        }

        if (tmp == NULL) {
            return;
        }

        if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED)) {
            object_destroy(tmp);
            tmp = NULL;
        }
    } while (tmp == NULL);

    tmp->x = op->x;
    tmp->y = op->y;
    SET_FLAG(tmp, FLAG_UNPAID);

    /* If this shop floor doesn't have FLAG_CURSED, generate shop-clone
     * items. */
    if (!QUERY_FLAG(op, FLAG_CURSED)) {
        SET_FLAG(tmp, FLAG_NO_PICK);
    }

    tmp = object_insert_map(tmp, op->map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
    SOFT_ASSERT(tmp != NULL,
                "Failed to insert treasure generated from %s",
                object_get_str(op));
    identify(tmp);
}

/**
 * Initialize the shop floor type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(shop_floor)
{
    OBJECT_METHODS(SHOP_FLOOR)->auto_apply_func = auto_apply_func;
}

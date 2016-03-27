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
 * Handles code related to @ref BULLET "bullet".
 */

#include <global.h>
#include <object.h>
#include <player.h>
#include <object_methods.h>

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    /* Custom handling for the magic missile spell bullets, so that they
     * follow the target. */
    if (op->stats.sp == SP_MAGIC_MISSILE) {
        rv_vector rv;

        if (!OBJECT_VALID(op->enemy, op->enemy_count) ||
            !get_rangevector(op, op->enemy, &rv, 0)) {
            object_remove(op, 0);
            object_destroy(op);
            return;
        }

        op->direction = rv.direction;
        SET_ANIMATION_STATE(op);
    }

    common_object_projectile_process(op);
}

/** @copydoc object_methods_t::projectile_hit_func */
static int
projectile_hit_func (object *op, object *victim)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    /* Handle probe. */
    if (op->stats.sp == SP_PROBE && IS_LIVE(victim)) {
        object *owner = object_owner(op);
        if (owner != NULL) {
            draw_info_format(COLOR_WHITE, owner,
                             "Your probe analyzes %s.",
                             victim->name);
            examine(owner, victim, NULL);
        }

        return OBJECT_METHOD_OK;
    }

    return common_object_projectile_hit(op, victim);
}

/**
 * Initialize the bullet type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(bullet)
{
    OBJECT_METHODS(BULLET)->process_func =
        process_func;
    OBJECT_METHODS(BULLET)->projectile_move_func =
        common_object_projectile_move;
    OBJECT_METHODS(BULLET)->projectile_stop_func =
        common_object_projectile_stop_spell;
    OBJECT_METHODS(BULLET)->projectile_hit_func =
        projectile_hit_func;
    OBJECT_METHODS(BULLET)->move_on_func =
        common_object_projectile_move_on;
}

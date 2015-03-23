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
 * Handles code related to @ref BULLET "bullet". */

#include <global.h>

/**
 * Attempt to reflect a bullet object by an object with @ref FLAG_REFL_SPELL
 * on the specified square.
 * @param op The bullet object.
 * @param m Map.
 * @param x X position.
 * @param y Y position.
 * @return 1 if the bullet was reflected, 0 otherwise. */
int bullet_reflect(object *op, mapstruct *m, int x, int y)
{
    int sub_layer;
    object *tmp;

    m = get_map_from_coord(m, &x, &y);

    if (!m) {
        return 0;
    }

    for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_LIVING, sub_layer); tmp && tmp->layer == LAYER_LIVING && tmp->sub_layer == sub_layer; tmp = tmp->above) {
            /* Try to reflect the bullet. */
            if (QUERY_FLAG(HEAD(tmp), FLAG_REFL_SPELL) && rndm(0, 99) < 90 - (op->level / 10)) {
                op->direction = absdir(op->direction + 4);
                return 1;
            }
        }
    }

    return 0;
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    if (op->stats.sp == SP_MAGIC_MISSILE) {
        rv_vector rv;

        if (!OBJECT_VALID(op->enemy, op->enemy_count) || !get_rangevector(op, op->enemy, &rv, 0)) {
            object_remove(op, 0);
            object_destroy(op);
            return;
        }

        op->direction = rv.direction;
        SET_ANIMATION_STATE(op);
    }

    common_object_projectile_process(op);
}

/** @copydoc object_methods::projectile_hit_func */
static int projectile_hit_func(object *op, object *victim)
{
    /* Handle probe. */
    if (op->stats.sp == SP_PROBE && IS_LIVE(victim)) {
        object *owner;

        owner = get_owner(op);

        if (owner) {
            draw_info_format(COLOR_WHITE, owner, "Your probe analyzes %s.", victim->name);
            examine(owner, victim, NULL);
        }

        return OBJECT_METHOD_OK;
    }

    return common_object_projectile_hit(op, victim);
}

/**
 * Initialize the bullet type object methods. */
void object_type_init_bullet(void)
{
    object_type_methods[BULLET].process_func = process_func;

    object_type_methods[BULLET].projectile_move_func = common_object_projectile_move;
    object_type_methods[BULLET].projectile_stop_func = common_object_projectile_stop_spell;
    object_type_methods[BULLET].projectile_hit_func = projectile_hit_func;
    object_type_methods[BULLET].move_on_func = common_object_projectile_move_on;
}

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
 * Handles code for @ref TREASURE "treasure".
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

    int level;
    if (op->stats.exp != 0) {
        level = op->stats.exp;
    } else {
        level = get_environment_level(op);
    }

    create_treasure(op->randomitems,
                    op,
                    op->map != NULL ? GT_ENVIRONMENT : 0,
                    level,
                    T_STYLE_UNSET,
                    ART_CHANCE_UNSET,
                    0,
                    NULL);

    /* If we generated on object and put it in this object inventory,
     * move it to the parent object as the current object is about
     * to disappear. An example of this item is the random_* stuff
     * that is put inside other objects. */
    if (op->env != NULL) {
        FOR_INV_PREPARE(op, tmp) {
            object_remove(tmp, 0);
            object_insert_into(tmp, op->env, 0);
        } FOR_INV_FINISH();
    }

    object_remove(op, 0);
    object_destroy(op);
}

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    FOR_INV_PREPARE(op, tmp) {
        object_remove(tmp, 0);

        if (op->env != NULL) {
            object_insert_into(tmp, op->env, 0);
        } else if (op->map != NULL) {
            tmp->x = op->x;
            tmp->y = op->y;
            object_insert_map(tmp, op->map, applier, 0);
        } else {
            object_destroy(tmp);
        }
    } FOR_INV_FINISH();

    if (op->msg != NULL) {
        draw_info(COLOR_WHITE, applier, op->msg);
    }

    object_remove(op, 0);
    object_destroy(op);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the treasure type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(treasure)
{
    OBJECT_METHODS(TREASURE)->auto_apply_func = auto_apply_func;
    OBJECT_METHODS(TREASURE)->apply_func = apply_func;
}

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
 * @ref DOOR "Door" related code.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Open the specified door.
 * @param op The door to open.
 * @param opener Who is opening the door.
 * @param nearby Whether this door was opened by opening a nearby door.
 */
static void door_open(object *op, object *opener, uint8 nearby)
{
    object *tmp;

    /* Already open, nothing to do. */
    if (op->last_eat == 1) {
        return;
    }

    /* Spring any traps in the door's inventory. */
    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if (tmp->type == RUNE && tmp->level != 0) {
            rune_spring(tmp, opener);
        }
    }

    /* Mark this door as opened. */
    op->last_eat = 1;
    /* Put it on the active list, so it will close automatically. */
    op->speed = 0.1f;
    update_ob_speed(op);
    op->speed_left = -0.2f;
    op->state = 1;
    /* Initialize counter that controls how long to allow the door to
     * stay open. */
    op->last_sp = op->stats.sp;

    CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
    CLEAR_FLAG(op, FLAG_DOOR_CLOSED);

    /* Update animation state. */
    if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE)) {
        SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) *
                op->direction + op->state);
    }

    if (op->sub_type == ST1_DOOR_NORMAL && !nearby) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, "door.ogg",
                op->x, op->y, 0, 0);
    }

    update_object(op, UP_OBJ_FLAGS);
}

/**
 * Open a door, including all nearby doors.
 * @param op Door object to open.
 * @param opener Object opening the door.
 */
static void doors_open(object *op, object *opener)
{
    int i, x, y;
    mapstruct *m;
    object *tmp;

    door_open(op, opener, 0);

    /* Try to open nearby doors. */
    for (i = 1; i <= SIZEOFFREE1; i += 2) {
        x = op->x + freearr_x[i];
        y = op->y + freearr_y[i];
        m = get_map_from_coord(op->map, &x, &y);

        if (m == NULL) {
            continue;
        }

        /* If there's no closed door, no need to check the objects on
         * this square. */
        if (!(GET_MAP_FLAGS(m, x, y) & P_DOOR_CLOSED)) {
            continue;
        }

        FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_WALL, -1, tmp)
        {
            if (tmp->type == DOOR && tmp->slaying == op->slaying) {
                door_open(tmp, opener, 1);
            }
        }
        FOR_MAP_LAYER_END
    }
}

/**
 * Open a door (or check whether it can be opened).
 * @param op Object which will open the door.
 * @param m Map where the door is.
 * @param x X position of the door.
 * @param y Y position of the door.
 * @param test If 1, only check whether the door can be opened, but do
 * not actually open the door.
 * @return 1 if door was opened (or can be), 0 if not and is not possible
 * to open.
 */
int door_try_open(object *op, mapstruct *m, int x, int y, int test)
{
    object *tmp, *key;

    /* Don't bother opening doors when the player has collision disabled. */
    if (op->type == PLAYER && CONTR(op)->tcl) {
        return 0;
    }

    /* Cannot open doors. */
    if (!(op->behavior & BEHAVIOR_OPEN_DOORS)) {
        return 0;
    }

    /* There isn't a door on this tile. */
    if (!(GET_MAP_FLAGS(m, x, y) & P_DOOR_CLOSED)) {
        return 0;
    }

    /* Pass through is set on the tile, and the object can pass through, so
     * don't open doors. */
    if (GET_MAP_FLAGS(m, x, y) & P_PASS_THRU &&
            QUERY_FLAG(op, FLAG_CAN_PASS_THRU)) {
        return 0;
    }

    FOR_MAP_LAYER_BEGIN(m, x, y, LAYER_WALL, -1, tmp)
    {
        if (tmp->type != DOOR) {
            continue;
        }

        if (tmp->slaying != NULL) {
            key = find_key(op, tmp);

            if (key == NULL) {
                if (!test) {
                    draw_info(COLOR_NAVY, op, tmp->msg);
                }

                return 0;
            } else if (!test) {
                if (key->type == KEY) {
                    draw_info_format(COLOR_WHITE, op, "You open the %s with "
                            "the %s.", tmp->name, query_short_name(key, NULL));
                } else if (key->type == FORCE) {
                    draw_info_format(COLOR_WHITE, op, "The %s is opened for "
                            "you.", tmp->name);
                }
            }
        }

        /* If we are here, the door can be opened. */
        if (!test) {
            doors_open(tmp, op);
        }

        return 1;
    }
    FOR_MAP_LAYER_END

    return 0;
}

/**
 * Search object for the needed key to open a door/container.
 * @param op Object to search in.
 * @param door The object to find the key for.
 * @return The key pointer if found, NULL otherwise.
 */
object *find_key(object *op, object *door)
{
    object *tmp, *key;

    /* First, let's try to find a key in the top level inventory. */
    for (tmp = op->inv; tmp != NULL; tmp = tmp->below) {
        if ((tmp->type == KEY || tmp->type == FORCE) &&
                tmp->slaying == door->slaying) {
            return tmp;
        }

        /* Go through containers. */
        if (tmp->type == CONTAINER && tmp->inv != NULL) {
            key = find_key(tmp, door);

            if (key != NULL) {
                return key;
            }
        }
    }

    return NULL;
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    /* Check to see if the door should still remain open. */
    if (op->last_sp-- > 0) {
        return;
    }

    /* If there's something blocking the door from closing, reset the
     * counter. */
    if (blocked(NULL, op->map, op->x, op->y, TERRAIN_ALL) & (P_NO_PASS | P_IS_MONSTER | P_IS_PLAYER)) {
        op->last_sp = op->stats.sp;
        return;
    }

    /* The door is no longer open. */
    op->last_eat = 0;

    /* Remove from active list. */
    op->speed = 0.0f;
    op->speed_left = 0.0f;
    update_ob_speed(op);

    op->state = 0;

    if (QUERY_FLAG(&op->arch->clone, FLAG_BLOCKSVIEW)) {
        SET_FLAG(op, FLAG_BLOCKSVIEW);
    }

    SET_FLAG(op, FLAG_DOOR_CLOSED);

    /* Update animation state. */
    if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE)) {
        SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) *
                op->direction + op->state);
    }

    if (op->sub_type == ST1_DOOR_NORMAL) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, "door_close.ogg",
                op->x, op->y, 0, 0);
    }

    update_object(op, UP_OBJ_FLAGS);
}

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    (void) op;
    (void) applier;
    (void) aflags;

    return OBJECT_METHOD_UNHANDLED;
}

/**
 * Initialize the door type object methods. */
void object_type_init_door(void)
{
    object_type_methods[DOOR].process_func = process_func;
    object_type_methods[DOOR].apply_func = apply_func;
}

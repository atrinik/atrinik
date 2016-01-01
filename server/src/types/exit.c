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
 * Handles code related to @ref EXIT "exits".
 *
 * @todo Somehow allow multi-part objects to pass through exits. The issue is
 * that usually portals lead to a space next to a return portal. This means that
 * when the multi-part object enters a portal, chances are that either its head
 * or its tail will be on top of the return portal, causing it to teleport back
 * to the portal it wanted to enter in the first place...
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <plugin.h>
#include <object.h>
#include <object_methods.h>
#include <exit.h>

/**
 * Find an automatically connected exit.
 *
 * @param op
 * Exit to find a connected exit for.
 * @param do_load
 * If true, will load maps if necessary.
 * @return
 * Connected exit if found, NULL otherwise.
 */
static object *
exit_find (object *op, bool do_load)
{
    HARD_ASSERT(op != NULL);

    object *altern[20];
    int nrofalt = 0;

    /* Find all other teleporters within range. This range should really
     * be settable by some object attribute instead of using hard coded
     * values. */
    for (int i = -5; i < 6; i++) {
        for (int j = -5; j < 6; j++) {
            if (i == 0 && j == 0) {
                /* Skip our own tile */
                continue;
            }

            int x = op->x + i;
            int y = op->y + j;
            mapstruct *m;
            if (do_load) {
                m = get_map_from_coord(op->map, &x, &y);
            } else {
                m = get_map_from_coord2(op->map, &x, &y);
            }

            if (m == NULL) {
                continue;
            }

            FOR_MAP_PREPARE(m, x, y, tmp) {
                if (tmp->type == op->type && tmp->sub_type == op->sub_type) {
                    /* Assumes altern can hold at least one element */
                    altern[nrofalt++] = tmp;

                    /* Reached the maximum, no point in going on. */
                    if (nrofalt == arraysize(altern)) {
                        goto loop_exit;
                    }
                }
            } FOR_MAP_FINISH();
        }
    }

loop_exit:

    if (nrofalt == 0) {
        return NULL;
    }

    return altern[rndm(0, nrofalt - 1)];
}

/**
 * Activate the exit, teleporting the person who applied it to the appropriate
 * destination.
 *
 * @param op
 * The exit.
 * @param applier
 * Who applied the exit.
 * @return
 * True on success, false on failure.
 */
static bool
exit_activate (object *op, object *applier)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (EXIT_PATH(op) != NULL) {
        return object_enter_map(applier, op, NULL, 0, 0, 0);
    }

    int x, y;
    mapstruct *m = exit_get_destination(op, &x, &y, true);
    if (m == NULL) {
        return false;
    }

    int i = find_free_spot(applier->arch, applier, m, x, y, 1, SIZEOFFREE1);
    if (i == -1) {
        return false;
    }

    applier->direction = i;
    SET_ANIMATION_STATE(applier);

    object_remove(applier, 0);
    applier->x = x + freearr_x[i];
    applier->y = y + freearr_y[i];
    insert_ob_in_map(applier, m, NULL, 0);

    return true;
}

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (op->map == NULL) {
        return OBJECT_METHOD_OK;
    }

    /* Do not allow multi-part objects to use exits. */
    if (op->more != NULL) {
        return OBJECT_METHOD_OK;
    }

    if (QUERY_FLAG(applier, FLAG_NO_TELEPORT)) {
        return OBJECT_METHOD_OK;
    }

    bool is_shop = false;
    for (int sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        object *tmp = GET_MAP_OB_LAYER(op->map,
                                       op->x,
                                       op->y,
                                       LAYER_FLOOR,
                                       sub_layer);
        if (tmp != NULL && tmp->type == SHOP_FLOOR) {
            is_shop = true;
            break;
        }
    }

    /* It's a shop exit, don't let the player out until they've paid for
     * all the items they want to buy (if any). */
    if (is_shop && applier->type == PLAYER && !shop_pay_items(applier)) {
        int i = find_free_spot(applier->arch,
                               NULL,
                               applier->map,
                               applier->x,
                               applier->y,
                               1,
                               SIZEOFFREE1);
        if (i != -1) {
            object_remove(applier, 0);
            applier->x += freearr_x[i];
            applier->y += freearr_y[i];
            insert_ob_in_map(applier, applier->map, op, 0);
        }

        return OBJECT_METHOD_OK;
    }

    /* Don't display messages for random maps. */
    if (op->msg != NULL && (EXIT_PATH(op) == NULL ||
                            (strncmp(EXIT_PATH(op), "/!", 2) != 0 &&
                             strncmp(EXIT_PATH(op), "/random/", 8) != 0))) {
        draw_info(COLOR_NAVY, applier, op->msg);
    } else if (is_shop) {
        draw_info(COLOR_WHITE, applier, "Thank you for visiting our shop.");
    }

    if (op->race != NULL) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, op->race, op->x, op->y, 0, 0);
    }

    if (!exit_activate(op, applier)) {
        if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
            char *name = object_get_name_s(op, applier);
            draw_info_format(COLOR_WHITE, applier, "The %s is closed.", name);
            efree(name);
        }

        log_error("Exit %s leads nowhere, applier: %s",
                  object_get_str(op),
                  object_get_str(applier));
        return OBJECT_METHOD_OK;
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::move_on_func */
static int
move_on_func (object *op, object *victim, object *originator, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(victim != NULL);

    return apply_func(op, victim, 0);
}

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->map == NULL) {
        return;
    }

    FOR_MAP_PREPARE(op->map, op->x, op->y, tmp) {
        if (tmp == op || QUERY_FLAG(tmp, FLAG_NO_TELEPORT)) {
            continue;
        }

        if (HAS_EVENT(op, EVENT_TRIGGER)) {
            int ret = trigger_event(EVENT_TRIGGER,
                                    tmp,
                                    op,
                                    NULL,
                                    NULL,
                                    0,
                                    0,
                                    0,
                                    0);
            if (ret == 1) {
                return;
            } else if (ret == 2) {
                continue;
            }
        }

        object_apply(op, tmp, 0);
    } FOR_MAP_FINISH();
}

/** @copydoc object_methods_t::trigger_func */
static int
trigger_func (object *op, object *cause, int state)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(cause != NULL);

    process_func(op);
    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::insert_map_func */
static void
insert_map_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (EXIT_PATH(op) != NULL) {
        /* Exit has a path, ensure it's absolute and take unique maps
         * into account. */
        bool is_unique = MAP_UNIQUE(op->map) && !map_path_isabs(EXIT_PATH(op));
        char *path = map_get_path(op->map,
                                  EXIT_PATH(op),
                                  is_unique,
                                  NULL);
        FREE_AND_COPY_HASH(EXIT_PATH(op), path);
        efree(path);
    } else if (op->last_heal > 0 && op->last_heal <= TILED_NUM &&
               op->map->tile_path[op->last_heal - 1] != NULL) {
        FREE_AND_ADD_REF_HASH(EXIT_PATH(op),
                op->map->tile_path[op->last_heal - 1]);

        EXIT_X(op) = op->x;
        EXIT_Y(op) = op->y;

        if (QUERY_FLAG(op, FLAG_XRAYS)) {
            int dir;
            if (op->last_heal - 1 == TILED_UP) {
                dir = absdir(op->direction + 4);
            } else {
                dir = op->direction;
            }

            EXIT_X(op) += freearr_x[dir];
            EXIT_Y(op) += freearr_y[dir];
        }
    } else if (EXIT_X(op) != -1 && EXIT_Y(op) != -1) {
        /* Exit with no path but has X/Y coordinates, use the map's path. */
        FREE_AND_ADD_REF_HASH(EXIT_PATH(op), op->map->path);
    }

    /* If the exit has a usable path, add it to the map's list of exits. */
    if (EXIT_PATH(op) != NULL) {
        map_exit_t *exit = ecalloc(1, sizeof(*exit));
        exit->obj = op;
        DL_APPEND(op->map->exits, exit);
    }
}

/** @copydoc object_methods_t::remove_map_func */
static void
remove_map_func (object *op)
{
    HARD_ASSERT(op != NULL);

    map_exit_t *exit, *tmp;
    DL_FOREACH_SAFE(op->map->exits, exit, tmp) {
        if (exit->obj == op) {
            DL_DELETE(op->map->exits, exit);
            efree(exit);
            break;
        }
    }
}

/**
 * Initialize the exit type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(exit)
{
    OBJECT_METHODS(EXIT)->apply_func = apply_func;
    OBJECT_METHODS(EXIT)->move_on_func = move_on_func;
    OBJECT_METHODS(EXIT)->process_func = process_func;
    OBJECT_METHODS(EXIT)->trigger_func = trigger_func;
    OBJECT_METHODS(EXIT)->insert_map_func = insert_map_func;
    OBJECT_METHODS(EXIT)->remove_map_func = remove_map_func;
}

/**
 * Acquires the specified exit's destination (map and coordinates).
 *
 * If this function returns NULL, the contents of 'x' and 'y' are undefined.
 *
 * @param op
 * Exit.
 * @param[out] x
 * Will contain the destination X coordinate. Can be NULL.
 * @param[out] y
 * Will contain the destination Y coordinate. Can be NULL.
 * @param do_load
 * Whether to load maps if necessary.
 * @return
 * Destination map. Can be NULL.
 */
mapstruct *
exit_get_destination (object *op, int *x, int *y, bool do_load)
{
    HARD_ASSERT(op != NULL);

    if (EXIT_PATH(op) != NULL) {
        mapstruct *m;
        if (do_load) {
            m = ready_map_name(EXIT_PATH(op), NULL, 0);
        } else {
            m = has_been_loaded_sh(EXIT_PATH(op));
        }

        if (m == NULL) {
            return NULL;
        }

        int xt = EXIT_X(op);
        int yt = EXIT_Y(op);
        m = get_map_from_coord(m, &xt, &yt);

        if (x != NULL) {
            *x = xt;
        }

        if (y != NULL) {
            *y = yt;
        }

        return m;
    } else if (op->sub_type != 0) {
        object *other_exit = exit_find(op, do_load);
        if (other_exit == NULL) {
            return NULL;
        }

        if (x != NULL) {
            *x = other_exit->x;
        }

        if (y != NULL) {
            *y = other_exit->y;
        }

        return other_exit->map;
    }

    return NULL;
}

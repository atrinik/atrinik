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
 * @author Alex Tokar */

#include <global.h>
#include <plugin.h>

static object *exit_find(object *op, int do_load)
{
    /* Better use c/malloc here in the future */
    object *altern[120];
    int i, j, nrofalt = 0, xt, yt;
    object *tmp;
    mapstruct *m;

    HARD_ASSERT(op != NULL);

    /* Find all other teleporters within range. This range should really
     * be settable by some object attribute instead of using hard coded
     * values. */
    for (i = -5; i < 6; i++) {
        for (j = -5; j < 6; j++) {
            if (i == 0 && j == 0) {
                continue;
            }

            xt = op->x + i;
            yt = op->y + j;

            if (do_load) {
                m = get_map_from_coord(op->map, &xt, &yt);
            } else {
                m = get_map_from_coord2(op->map, &xt, &yt);
            }

            if (m == NULL) {
                continue;
            }

            for (tmp = GET_MAP_OB(m, xt, yt); tmp != NULL; tmp = tmp->above) {
                if (tmp->type == op->type && tmp->sub_type == op->sub_type) {
                    break;
                }
            }

            if (tmp != NULL) {
                altern[nrofalt++] = tmp;
            }
        }
    }

    if (nrofalt == 0) {
        return NULL;
    }

    return altern[rndm(0, nrofalt - 1)];
}

/**
 * Activate the exit, teleporting the person who applied it to the appropriate
 * destination.
 * @param op The exit.
 * @param applier Who applied the exit.
 * @return True on success, false on failure.
 */
static bool exit_activate(object *op, object *applier)
{
    mapstruct *m;
    int x, y, i;

    if (EXIT_PATH(op) != NULL && EXIT_X(op) != -1 && EXIT_Y(op) != -1) {
        return object_enter_map(applier, op, NULL, 0, 0, 0);
    }

    m = exit_get_destination(op, &x, &y, 1);

    if (m == NULL) {
        return false;
    }

    i = find_free_spot(applier->arch, applier, m, x, y, 1, 9);

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

/**
 * Acquires the specified exit's destination (map and coordinates).
 *
 * If this function returns NULL, the contents of 'x' and 'y' are undefined.
 * @param op Exit.
 * @param[out] x Will contain the destination X coordinate. Can be NULL.
 * @param[out] y Will contain the destination Y coordinate. Can be NULL.
 * @param do_load Whether to load maps if necessary.
 * @return Destination map. Can be NULL.
 */
mapstruct *exit_get_destination(object *op, int *x, int *y, int do_load)
{
    mapstruct *m;

    HARD_ASSERT(op != NULL);

    if (EXIT_PATH(op) != NULL && EXIT_X(op) != -1 && EXIT_Y(op) != -1) {
        if (do_load) {
            m = ready_map_name(EXIT_PATH(op), NULL, 0);
        } else {
            m = has_been_loaded_sh(EXIT_PATH(op));
        }

        if (m == NULL) {
            return NULL;
        }

        if (x != NULL) {
            *x = EXIT_X(op);
        }

        if (y != NULL) {
            *y = EXIT_Y(op);
        }

        return m;
    } else if (op->sub_type != 0) {
        object *other_exit;

        other_exit = exit_find(op, do_load);

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

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    bool is_shop;
    int sub_layer, i;
    object *tmp;

    (void) aflags;

    if (op->map == NULL) {
        return OBJECT_METHOD_OK;
    }

    /* Do not allow multi-part objects to use exits. */
    if (op->more != NULL) {
        return OBJECT_METHOD_OK;
    }

    is_shop = false;

    for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++) {
        tmp = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, sub_layer);

        if (tmp != NULL && tmp->type == SHOP_FLOOR) {
            is_shop = true;
            break;
        }
    }

    if (is_shop && applier->type == PLAYER && !get_payment(applier,
            applier->inv)) {
        i = find_free_spot(applier->arch, NULL, applier->map, applier->x,
                applier->y, 1, SIZEOFFREE1 + 1);

        if (i != -1) {
            object_remove(applier, 0);
            applier->x += freearr_x[i];
            applier->y += freearr_y[i];
            insert_ob_in_map(applier, applier->map, op, 0);
        }

        return OBJECT_METHOD_OK;
    }

    /* Don't display messages for random maps. */
    if (op->msg != NULL && (EXIT_PATH(op) == NULL || (strncmp(EXIT_PATH(op),
            "/!", 2) != 0 && strncmp(EXIT_PATH(op), "/random/", 8) != 0))) {
        draw_info(COLOR_NAVY, applier, op->msg);
    } else if (is_shop) {
        draw_info(COLOR_WHITE, applier, "Thank you for visiting our shop.");
    }

    if (op->race != NULL) {
        play_sound_map(op->map, CMD_SOUND_EFFECT, op->race, op->x, op->y, 0, 0);
    }

    if (!exit_activate(op, applier)) {
        if (!QUERY_FLAG(op, FLAG_SYS_OBJECT)) {
            draw_info_format(COLOR_WHITE, applier, "The %s is closed.",
                    query_name(op, NULL));
        }

        LOG(BUG, "Exit '%s' on map %s at %d,%d leads nowhere.", op->name,
                op->map->path, op->x, op->y);
        return OBJECT_METHOD_OK;
    }

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator,
        int state)
{
    (void) originator;
    (void) state;

    return apply_func(op, victim, 0);
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    object *tmp, *next;

    if (op->map == NULL) {
        return;
    }

    for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp != NULL; tmp = next) {
        next = tmp->above;

        if (tmp == op || QUERY_FLAG(tmp, FLAG_NO_TELEPORT)) {
            continue;
        }

        if (HAS_EVENT(op, EVENT_TRIGGER)) {
            int ret;

            ret = trigger_event(EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0,
                    SCRIPT_FIX_NOTHING);

            if (ret == 1) {
                return;
            } else if (ret == 2) {
                continue;
            }
        }

        object_apply(op, tmp, 0);
    }
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
    process_func(op);

    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::insert_map_func */
static void insert_map_func(object *op)
{
    if (EXIT_PATH(op) != NULL) {
        char *path;

        path = map_get_path(op->map, EXIT_PATH(op), MAP_UNIQUE(op->map) &&
                !map_path_isabs(EXIT_PATH(op)), NULL);
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
        FREE_AND_ADD_REF_HASH(EXIT_PATH(op), op->map->path);
    }

    if (EXIT_PATH(op) != NULL && EXIT_X(op) != -1 && EXIT_Y(op) != -1) {
        map_exit_t *exit;

        exit = ecalloc(1, sizeof(*exit));
        exit->obj = op;
        DL_APPEND(op->map->exits, exit);
    }
}

/** @copydoc object_methods::remove_map_func */
static void remove_map_func(object *op)
{
    map_exit_t *exit, *tmp;

    DL_FOREACH_SAFE(op->map->exits, exit, tmp)
    {
        if (exit->obj == op) {
            DL_DELETE(op->map->exits, exit);
            efree(exit);
            break;
        }
    }
}

/**
 * Initialize the exit type object methods. */
void object_type_init_exit(void)
{
    object_type_methods[EXIT].apply_func = apply_func;
    object_type_methods[EXIT].move_on_func = move_on_func;
    object_type_methods[EXIT].process_func = process_func;
    object_type_methods[EXIT].trigger_func = trigger_func;
    object_type_methods[EXIT].insert_map_func = insert_map_func;
    object_type_methods[EXIT].remove_map_func = remove_map_func;
}

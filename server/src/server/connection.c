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
 * Connection system handling.
 */

#include <global.h>
#include <plugin.h>

/**
 * Creates a new connection.
 * @param op Object to connect.
 * @param map Map to create the connection on.
 * @param connected Connection ID of the object.
 */
void connection_object_add(object *op, mapstruct *map, int connected)
{
    objectlink *ol2, *ol;

    if (!op || !map) {
        return;
    }

    /* Remove previous connection if any. */
    if (QUERY_FLAG(op, FLAG_IS_LINKED)) {
        connection_object_remove(op);
    }

    SET_FLAG(op, FLAG_IS_LINKED);
    op->path_attuned = connected;

    for (ol = map->buttons; ol; ol = ol->next) {
        if (ol->value == connected) {
            break;
        }
    }

    ol2 = get_objectlink();
    ol2->objlink.ob = op;
    ol2->id = op->count;

    /* Link to the existing one. */
    if (ol) {
        ol2->next = ol->objlink.link;
        ol->objlink.link = ol2;
    } else {
        /* Create new. */

        ol = get_objectlink();
        ol->value = connected;

        ol->next = map->buttons;
        map->buttons = ol;
        ol->objlink.link = ol2;
    }
}

/**
 * Remove a connection.
 * @param op Object to remove. Must be on a map, and connected.
 */
void connection_object_remove(object *op)
{
    objectlink *ol, **ol2, *tmp;

    if (!op->map) {
        return;
    }

    if (!QUERY_FLAG(op, FLAG_IS_LINKED)) {
        return;
    }

    for (ol = op->map->buttons; ol; ol = ol->next) {
        for (ol2 = &ol->objlink.link; (tmp = *ol2); ol2 = &tmp->next) {
            if (tmp->objlink.ob == op) {
                *ol2 = tmp->next;
                free_objectlink_simple(ol);
                return;
            }
        }
    }

    CLEAR_FLAG(op, FLAG_IS_LINKED);
}

/**
 * Acquire the connection ID of the specified object.
 * @param op Object to get the connection ID of.
 * @return Connection ID, or 0 if not connected.
 */
int connection_object_get_value(object *op)
{
    if (!op || !op->map || !QUERY_FLAG(op, FLAG_IS_LINKED)) {
        return 0;
    }

    return op->path_attuned;
}

/**
 * Return the first objectlink in the objects linked to this one.
 * @param op Object to get the link for.
 * @param map Map to look at.
 * @return ::objectlink for this object, or NULL.
 */
static objectlink *connection_object_links(object *op, mapstruct *map)
{
    objectlink *ol, *ol2;

    HARD_ASSERT(op != NULL);
    HARD_ASSERT(map != NULL);

    for (ol = map->buttons; ol != NULL; ol = ol->next) {
        for (ol2 = ol->objlink.link; ol2 != NULL; ol2 = ol2->next) {
            if (OBJECT_VALID(ol2->objlink.ob, ol2->id) &&
                    ol2->objlink.ob->path_attuned == op->path_attuned) {
                return ol->objlink.link;
            }
        }
    }

    return NULL;
}

/**
 * Actually does the logic behind triggering a connection.
 * @param op The object.
 * @param state Trigger state.
 * @param button If true, we're triggering a button (called from
 * connection_trigger_button())
 * @return If @p button is true, returns new state of the button, otherwise 0
 * is returned.
 */
static int64_t connection_trigger_do(object *op, int state, bool button)
{
    if (op->map == NULL) {
        return 0;
    }

    int64_t down = 0;

    for (int i = -1; i < TILED_NUM; i++) {
        if (i != -1 && (op->map->tile_path[i] == NULL ||
                !(op->path_repelled & (1 << i)))) {
            continue;
        }

        mapstruct *map = op->map;

        if (i != -1) {
            map = ready_map_name(map->tile_path[i], NULL, MAP_NAME_SHARED);

            if (map == NULL) {
                LOG(ERROR, "Could not load map: %s",
                        op->map->tile_path[i]);
                continue;
            }
        }

        for (objectlink *ol = connection_object_links(op, map); ol != NULL;
                ol = ol->next) {
            object *tmp = ol->objlink.ob;

            if (button) {
                if (object_trigger_button(tmp, op, state) == OBJECT_METHOD_OK) {
                    if (tmp->value) {
                        down = 1;

                        if (QUERY_FLAG(tmp, FLAG_CONNECT_RESET)) {
                            tmp->value = 0;
                        }
                    }
                }
            } else {
                /* If the criteria isn't appropriate, don't do anything. */
                if (state && QUERY_FLAG(tmp, FLAG_CONNECT_NO_PUSH)) {
                    continue;
                }

                if (!state && QUERY_FLAG(tmp, FLAG_CONNECT_NO_RELEASE)) {
                    continue;
                }

                if (HAS_EVENT(tmp, EVENT_TRIGGER) && trigger_event(
                        EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0,
                        SCRIPT_FIX_NOTHING)) {
                    continue;
                }

                object_trigger(tmp, op, state);
            }
        }
    }

    return down;
}

/**
 * Trigger an object.
 * @param op The object.
 * @param state The trigger state.
 */
void connection_trigger(object *op, int state)
{
    HARD_ASSERT(op != NULL);

    connection_trigger_do(op, state, false);
}

/**
 * Trigger a button-like object.
 * @param op The button-like object.
 * @param state The trigger state.
 */
void connection_trigger_button(object *op, int state)
{
    int64_t old_state, down;

    HARD_ASSERT(op != NULL);

    old_state = op->value;
    down = connection_trigger_do(op, state, true);

    if (down) {
        op->value = down;
    }

    if (op->value != old_state) {
        connection_trigger(op, op->value);
    }

    if (op->value && QUERY_FLAG(op, FLAG_CONNECT_RESET)) {
        op->value = 0;
    }
}

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
 * Handles code for handling @ref CONTAINER "containers".
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Check if both objects are magical containers.
 * @param op Object being put into the container.
 * @param container The container.
 * @return 1 if both op and container are magical containers, 0 otherwise.
 */
int check_magical_container(object *op, object *container)
{
    if (op->type == CONTAINER && container->type == CONTAINER &&
            op->weapon_speed != 1.0f && container->weapon_speed != 1.0f) {
        return 1;
    }

    return 0;
}

/**
 * Actually open a container, springing traps/monsters, and doing the
 * linked list linking.
 * @param applier Player that is opening the container.
 * @param op The container.
 */
static void container_open(object *applier, object *op)
{
    player *pl;

    /* Safety. */
    if (applier->type != PLAYER) {
        return;
    }

    pl = CONTR(applier);

    /* Safety. */
    if (op->attacked_by && op->attacked_by->type != PLAYER) {
        op->attacked_by = NULL;
    }

    /* Check for quest containers. */
    if (HAS_EVENT(op, EVENT_QUEST)) {

        FOR_INV_PREPARE(op, inv)
        {
            if (inv->type == QUEST_CONTAINER) {
                quest_handle(applier, inv);
            }
        }
        FOR_INV_FINISH();
    }

    pl->container = op;
    pl->container_count = op->count;
    pl->container_above = op->attacked_by;

    /* Already opened. */
    if (op->attacked_by) {
        CONTR(op->attacked_by)->container_below = applier;
    } else {
        /* Not open yet. */

        SET_FLAG(op, FLAG_APPLIED);

        if (op->other_arch) {
            op->face = op->other_arch->clone.face;
            op->animation_id = op->other_arch->clone.animation_id;
            SET_ANIMATION_STATE(op);
            esrv_update_item(UPD_FACE | UPD_ANIM | UPD_FLAGS, op);
        } else {
            esrv_update_item(UPD_FLAGS, op);
        }

        update_object(op, UP_OBJ_FACE);

        FOR_INV_PREPARE(op, inv)
        {
            if (inv->type == RUNE) {
                rune_spring(inv, applier);
            } else if (inv->type == MONSTER) {
                int i;

                object_remove(inv, 0);
                inv->x = op->x;
                inv->y = op->y;
                i = find_free_spot(inv->arch, inv, applier->map, inv->x,
                        inv->y, 0, SIZEOFFREE1 + 1);

                if (i != -1) {
                    inv->x += freearr_x[i];
                    inv->y += freearr_y[i];
                }

                inv = insert_ob_in_map(inv, applier->map, inv, 0);

                if (inv) {
                    fix_monster(inv);
                    draw_info_format(
                            COLOR_WHITE, applier, "A %s jumps out of the %s.",
                            query_name(inv, applier),
                            query_base_name(op, applier)
                            );
                }
            }
        }
        FOR_INV_FINISH();
    }

    esrv_send_inventory(applier, op);
    pl->container_below = NULL;
    op->attacked_by = applier;
    op->attacked_by_count = applier->count;
}

/**
 * Close a container and remove player from the container's linked list.
 *
 * @param applier The player. If NULL, we will unlink all players from
 * the container 'op'.
 * @param op The container object. If NULL, unlink the applier's current
 * container.
 * @return 1 if the container was closed and has no players left looking
 * into the container, 0 otherwise.
 */
int container_close(object *applier, object *op)
{
    if (!applier && !op) {
        return 0;
    }

    if (applier && applier->type == PLAYER) {
        player *pl;

        pl = CONTR(applier);

        /* No container, nothing to do. */
        if (!pl->container) {
            return 0;
        }

        /* Make sure the object is valid. */
        if (!OBJECT_VALID(pl->container, pl->container_count)) {
            pl->container = NULL;
            pl->container_count = 0;
            return 0;
        }

        /* Only applier left, go ahead and close the container. */
        if (!pl->container_below && !pl->container_above) {
            return container_close(NULL, pl->container);
        }

        /* The applier is at the beginning of the linked list. */
        if (!pl->container_below) {
            pl->container->attacked_by = pl->container_above;
            pl->container->attacked_by_count = pl->container_above->count;
            CONTR(pl->container_above)->container_below = NULL;
        } else {
            /* Elsewhere in the list. */

            CONTR(pl->container_below)->container_above = pl->container_above;

            if (pl->container_above) {
                CONTR(pl->container_above)->container_below =
                        pl->container_below;
            }
        }

        pl->container_above = NULL;
        pl->container_below = NULL;
        pl->container = NULL;
        pl->container_count = 0;
        esrv_close_container(applier);
    } else if (op) {
        object *tmp, *next;

        CLEAR_FLAG(op, FLAG_APPLIED);

        if (op->other_arch) {
            op->face = op->arch->clone.face;
            op->animation_id = op->arch->clone.animation_id;
            SET_ANIMATION_STATE(op);
            esrv_update_item(UPD_FACE | UPD_ANIM | UPD_FLAGS, op);
        } else {
            esrv_update_item(UPD_FLAGS, op);
        }

        update_object(op, UP_OBJ_FACE);

        for (tmp = op->attacked_by; tmp; tmp = next) {
            next = CONTR(tmp)->container_above;

            CONTR(tmp)->container = NULL;
            CONTR(tmp)->container_count = 0;
            CONTR(tmp)->container_below = NULL;
            CONTR(tmp)->container_above = NULL;
            esrv_close_container(tmp);
        }

        op->attacked_by = NULL;
        op->attacked_by_count = 0;

        if (op->env != NULL && op->env->type == PLAYER && OBJECT_IS_AMMO(op)) {
            fix_player(op->env);
        }

        return 1;
    }

    return 0;
}

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    object *container, *tmp;

    (void) aflags;

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_UNHANDLED;
    }

    container = CONTR(applier)->container;

    if (op == NULL || op->type != CONTAINER ||
            (container && container->type != CONTAINER)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    /* Already opened container, so close it, even if the player wants to
     * open another container. */
    if (container) {
        /* Trigger the CLOSE event. */
        if (trigger_event(EVENT_CLOSE, applier, container, NULL, NULL, 0, 0, 0,
                SCRIPT_FIX_ALL)) {
            return OBJECT_METHOD_OK;
        }

        if (container_close(applier, container)) {
            draw_info_format(COLOR_WHITE, applier, "You close %s.",
                    query_base_name(container, applier));
        } else {
            draw_info_format(COLOR_WHITE, applier, "You leave %s.",
                    query_base_name(container, applier));
        }

        /* Applied the one we just closed, no need to go on. */
        if (container == op) {
            return OBJECT_METHOD_OK;
        }
    }

    /* If the player is trying to open it (which he must be doing if we
     * got here), and it is locked, check to see if player has the means
     * to open it. */
    if (op->slaying || op->stats.maxhp) {
        /* Locked container. */
        if (op->sub_type == ST1_CONTAINER_NORMAL) {
            tmp = find_key(applier, op);

            if (tmp) {
                if (tmp->type == KEY) {
                    draw_info_format(
                            COLOR_WHITE, applier, "You unlock %s with %s.",
                            query_base_name(op, applier), query_name(tmp, applier)
                            );
                } else if (tmp->type == FORCE) {
                    draw_info_format(
                            COLOR_WHITE, applier, "The %s is unlocked for you.",
                            query_base_name(op, applier)
                            );
                }
            } else {
                draw_info_format(
                        COLOR_WHITE, applier, "You don't have the key to unlock "
                        "%s.", query_base_name(op, applier)
                        );
                return OBJECT_METHOD_OK;
            }
        } else {
            /* Personalized container. */

            /* Party corpse. */
            if (op->sub_type == ST1_CONTAINER_CORPSE_party &&
                    !party_can_open_corpse(applier, op)) {
                return OBJECT_METHOD_OK;
            } else if (op->sub_type == ST1_CONTAINER_CORPSE_player &&
                    op->slaying != applier->name) {
                /* Normal player-only corpse. */
                draw_info(COLOR_WHITE, applier, "It's not your bounty.");
                return OBJECT_METHOD_OK;
            }
        }
    }

    /* The container is not in the applier's inventory. */
    if (op->env != applier) {
        /* If in inventory of some other object other than the applier,
         * can't open it. */
        if (op->env) {
            draw_info_format(COLOR_WHITE, applier, "You can't open %s.",
                    query_base_name(op, applier));
            return OBJECT_METHOD_OK;
        }

        draw_info_format(COLOR_WHITE, applier, "You open %s.",
                query_base_name(op, applier));
        container_open(applier, op);

        /* Handle party corpses. */
        if (op->slaying && op->sub_type == ST1_CONTAINER_CORPSE_party) {
            party_handle_corpse(applier, op);
        }
    } else {
        /* Container is in player's inventory. */

        /* If it's readied, open it. */
        if (QUERY_FLAG(op, FLAG_APPLIED)) {
            draw_info_format(COLOR_WHITE, applier, "You open %s.",
                    query_base_name(op, applier));
            container_open(applier, op);
        } else {
            /* Otherwise ready it. */
            if (OBJECT_IS_AMMO(op)) {
                object_apply_item(op, applier, aflags);
            } else {
                draw_info_format(COLOR_WHITE, applier, "You ready %s.",
                        query_base_name(op, applier));
                SET_FLAG(op, FLAG_APPLIED);

                update_object(op, UP_OBJ_FACE);
                esrv_update_item(UPD_FLAGS, op);
            }
        }
    }

    /* If it's a corpse and it has not been searched before, add to
     * player's statistics. */
    if ((op->sub_type == ST1_CONTAINER_CORPSE_party ||
            op->sub_type == ST1_CONTAINER_CORPSE_player) &&
            !QUERY_FLAG(op, FLAG_BEEN_APPLIED)) {
        CONTR(applier)->stat_corpses_searched++;
    }

    /* Only after actually readying/opening the container we know more
     * about it. */
    SET_FLAG(op, FLAG_BEEN_APPLIED);

    return 1;
}

/**
 * Initialize the container type object methods.
 */
void object_type_init_container(void)
{
    object_type_methods[CONTAINER].apply_func = apply_func;
}

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Common object processing functions.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Process a changing object.
 * @param op The object to process. */
static void common_object_process_changing(object *op)
{
    object *tmp, *env;

    if (op->stats.food-- > 0) {
        return;
    }

    /* Handle applyable lights that should burn out. */
    if (op->type == LIGHT_APPLY) {
        CLEAR_FLAG(op, FLAG_CHANGING);

        /* Inform the player, if the object is inside player's inventory. */
        if (op->env && op->env->type == PLAYER) {
            draw_info_format(COLOR_WHITE, op->env, "The %s burnt out.", query_name(op, NULL));
        }

        /* If other_arch is not set, it means the light can be refilled
         * (such as a lamp) and doesn't need to be exchanged with another
         * object (such as torch becoming a burned out torch). */
        if (!op->other_arch) {
            if (op->other_arch && op->other_arch->clone.sub_type & 1) {
                op->animation_id = op->other_arch->clone.animation_id;
                SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
            }
            else {
                CLEAR_FLAG(op, FLAG_ANIMATE);
                op->face = op->arch->clone.face;
            }

            /* Is it in inventory? */
            if (op->env) {
                op->glow_radius = 0;
                esrv_send_item(op);

                /* Inside player? */
                if (op->env->type == PLAYER) {
                    /* Will take care about adjusting light masks. */
                    fix_player(op->env);
                }
            }
            /* Object is on map. */
            else {
                /* Remove light mask from map. */
                adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
                update_object(op, UP_OBJ_FACE);
                op->glow_radius = 0;
            }

            return;
        }
    }

    if (!op->other_arch) {
        logger_print(LOG(BUG), "%s is missing other_arch.", op->name);
        return;
    }

    env = op->env;
    object_remove(op, 0);

    tmp = arch_to_object(op->other_arch);

    if (env) {
        tmp = insert_ob_in_ob(tmp, env);
    }
    else {
        tmp->x = op->x;
        tmp->y = op->y;
        insert_ob_in_map(tmp, op->map, op, 0);
    }
}

/**
 * Pre-processing for object_process() function.
 * @param op Object to pre-process.
 * @return 1 if the object was processed and should not continue
 * processing it normally, 0 otherwise. */
int common_object_process_pre(object *op)
{
    if (QUERY_FLAG(op, FLAG_CHANGING) && !op->state) {
        common_object_process_changing(op);
        return 1;
    }

    if (QUERY_FLAG(op, FLAG_IS_USED_UP) && --op->stats.food <= 0) {
        /* Handle corpses specially. */
        if (op->type == CONTAINER && (op->sub_type & 1) == ST1_CONTAINER_CORPSE) {
            /* If the corpse is currently open by someone, delay the
             * corpse removal for a bit longer. */
            if (op->attacked_by) {
                /* Give him a bit time back */
                op->stats.food += 3;
                return 1;
            }

            /* If the corpse was locked, remove the lock, making it
             * available available for all players, and reset the decay
             * counter. */
            if (op->slaying || op->stats.maxhp) {
                if (op->slaying) {
                    FREE_AND_CLEAR_HASH2(op->slaying);
                }

                op->stats.maxhp = 0;
                op->stats.food = op->arch->clone.stats.food;
                return 1;
            }
        }
        /* If it's a force or such in player's inventory, unapply it. */
        else if (op->env && op->env->type == PLAYER && QUERY_FLAG(op, FLAG_APPLIED)) {
            CLEAR_FLAG(op, FLAG_APPLIED);
            change_abil(op->env, op);
            fix_player(op->env);
        }

        object_remove(op, 0);
        object_destroy(op);

        return 1;
    }

    return 0;
}

/** @copydoc object_methods::process_func */
void common_object_process(object *op)
{
    if (OBJECT_IS_PROJECTILE(op)) {
        common_object_projectile_process(op);
    }
}

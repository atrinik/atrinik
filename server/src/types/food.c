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
 * Handles code used for @ref FOOD "food", @ref DRINK "drinks" and
 * @ref FLESH "flesh".
 */

#include <global.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>

/** Maximum allowed food value. */
#define FOOD_MAX 999

/**
 * Create a food force to include buff/debuff effects of stats and
 * protections to the player.
 *
 * @param who
 * The player object.
 * @param food
 * The food.
 * @param force
 * The force object.
 */
static void
food_create_force (object *who, object *food, object *force)
{
    HARD_ASSERT(who != NULL);
    HARD_ASSERT(food != NULL);
    HARD_ASSERT(force != NULL);

    for (int i = 0; i < NUM_STATS; i++) {
        set_attr_value(&force->stats, i, get_attr_value(&food->stats, i));
    }

    memcpy(force->protection, food->protection, sizeof(force->protection));

    /* Negate all stats for cursed food */
    if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
        int mult = QUERY_FLAG(food, FLAG_CURSED) ? 2 : 3;

        for (int i = 0; i < NUM_STATS; i++) {
            int val = get_attr_value(&force->stats, i);
            if (val > 0) {
                val = -val;
            }

            val *= mult;
            set_attr_value(&force->stats, i, val);
        }

        for (int i = 0; i < NROFATTACKS; i++) {
            if (force->protection[i] > 0) {
                force->protection[i] = -force->protection[i];
            }

            force->protection[i] *= mult;
        }
    }

    if (!DBL_EQUAL(food->speed_left, 0.0)) {
        force->speed = food->speed_left;
    }

    SET_FLAG(force, FLAG_APPLIED);
    SET_FLAG(force, FLAG_IS_USED_UP);

    force = object_insert_into(force, who, 0);
    SOFT_ASSERT(force != NULL,
                "Failed to insert force from food %s into %s.",
                object_get_str(food),
                object_get_str(who));
}

/**
 * The food gives specials, like +/- hp or sp, protections and stats.
 *
 * Food can be good or bad (good effect or bad effect), and cursed or
 * not. If food is "good" (for example, Str +1 and Dex +1), then it puts
 * those effects as force in the player for some time.
 *
 * If good food is cursed, all positive values are turned to negative
 * values.
 *
 * If bad food (Str -1, Dex -1) is uncursed, it gives just those values.
 *
 * If bad food is cursed, all negative values are doubled.
 *
 * Food effects can stack. For really powerful food, a high food value
 * should be set, so the player can't eat a lot of such food, as his
 * stomach will be full.
 *
 * @param who
 * Object eating the food.
 * @param food
 * The food object.
 */
static void
food_eat_special (object *who, object *food)
{
    HARD_ASSERT(who != NULL);
    HARD_ASSERT(food != NULL);

    /* Check if we need to create a special force to hold stat
     * modifications. */
    bool create_force = false;
    for (int i = 0; i < NUM_STATS && !create_force; i++) {
        if (get_attr_value(&food->stats, i) != 0) {
            create_force = true;
        }
    }

    for (int i = 0; i < NROFATTACKS && !create_force; i++) {
        if (food->protection[i] != 0) {
            create_force = true;
        }
    }

    if (create_force) {
        food_create_force(who, food, arch_get("force"));
    }

    /* Check for HP change */
    if (food->stats.hp) {
        if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
            int tmp = food->stats.hp;
            if (tmp > 0) {
                tmp = -tmp;
            }

            snprintf(VS(CONTR(who)->killer), "%s", food->name);

            if (QUERY_FLAG(food, FLAG_CURSED)) {
                who->stats.hp += tmp * 2;
            } else {
                who->stats.hp += tmp * 3;
            }

            draw_info(COLOR_WHITE, who, "Eck!... that was rotten food!");
        } else {
            draw_info(COLOR_WHITE, who, "You begin to feel better.");
            who->stats.hp += food->stats.hp;
            if (who->stats.hp > who->stats.maxhp) {
                who->stats.hp = who->stats.maxhp;
            }
        }
    }

    /* Check for SP change */
    if (food->stats.sp) {
        if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
            int tmp = food->stats.sp;
            if (tmp > 0) {
                tmp = -tmp;
            }

            draw_info(COLOR_WHITE, who, "Your mana is drained!");

            if (QUERY_FLAG(food, FLAG_CURSED)) {
                who->stats.sp += tmp * 2;
            } else {
                who->stats.sp += tmp * 3;
            }

            if (who->stats.sp < 0) {
                who->stats.sp = 0;
            }
        } else {
            draw_info(COLOR_WHITE, who, "You feel a rush of magical energy!");
            who->stats.sp += food->stats.sp;
            if (who->stats.sp > who->stats.maxsp) {
                who->stats.sp = who->stats.maxsp;
            }
        }
    }
}

/** @copydoc object_methods_t::apply_func */
static int
apply_func (object *op, object *applier, int aflags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_UNHANDLED;
    }

    if (applier->stats.food + op->stats.food > FOOD_MAX) {
        if ((applier->stats.food + op->stats.food) - FOOD_MAX >
            op->stats.food / 5) {
            draw_info_format(COLOR_WHITE, applier,
                             "You are too full to %s this right now!",
                             op->type == DRINK ? "drink" : "eat");
            return OBJECT_METHOD_OK;
        }

        if (op->type == FOOD || op->type == FLESH) {
            draw_info(COLOR_WHITE, applier,
                      "You feel full, but what a waste of food!");
        } else {
            draw_info(COLOR_WHITE, applier,
                      "Most of the drink goes down your face not your throat!");
        }
    }

    if (!QUERY_FLAG(op, FLAG_CURSED) && !QUERY_FLAG(op, FLAG_DAMNED)) {
        int capacity_remaining = FOOD_MAX - applier->stats.food;
        if (op->type == DRINK) {
            draw_info_format(COLOR_WHITE, applier,
                             "Ahhh... that %s tasted good.",
                             op->name);
        } else {
            draw_info_format(COLOR_WHITE, applier,
                             "The %s tasted %s",
                             op->name,
                             op->type == FLESH ? "terrible!" : "good.");
        }

        applier->stats.food =
            MAX(0, MIN(FOOD_MAX, applier->stats.food + ABS(op->stats.food)));
        CONTR(applier)->stat_food_consumed += op->stats.food;

        /* Heal for a bit */
        applier->stats.hp += MIN(capacity_remaining, op->stats.food) / 50;
        if (applier->stats.hp > applier->stats.maxhp) {
            applier->stats.hp = applier->stats.maxhp;
        }
    } else {
        draw_info_format(COLOR_WHITE, applier,
                         "The %s tasted terrible!",
                         op->name);
        applier->stats.food =
            MAX(0, MIN(FOOD_MAX, applier->stats.food - ABS(op->stats.food)));
    }

    CONTR(applier)->stat_food_num_consumed++;

    if (op->title != NULL ||
        QUERY_FLAG(op, FLAG_CURSED) ||
        QUERY_FLAG(op, FLAG_DAMNED)) {
        food_eat_special(applier, op);
    }

    decrease_ob(op);
    return OBJECT_METHOD_OK;
}

/**
 * Initialize the food type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(food)
{
    OBJECT_METHODS(FOOD)->apply_func = apply_func;
}

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
 * Handles code used for @ref FOOD "food", @ref DRINK "drinks" and
 * @ref FLESH "flesh". */

#include <global.h>

/** Maximum allowed food value. */
#define FOOD_MAX 999

/**
 * Create a food force to include buff/debuff effects of stats and
 * protections to the player.
 * @param who The player object.
 * @param food The food.
 * @param force The force object. */
static void create_food_force(object* who, object *food, object *force)
{
    int i;

    force->stats.Str = food->stats.Str;
    force->stats.Pow = food->stats.Pow;
    force->stats.Dex = food->stats.Dex;
    force->stats.Con = food->stats.Con;
    force->stats.Int = food->stats.Int;
    force->stats.Wis = food->stats.Wis;
    force->stats.Cha = food->stats.Cha;

    for (i = 0; i < NROFATTACKS; i++) {
        force->protection[i] = food->protection[i];
    }

    /* if damned, set all negative if not and double or triple them */
    if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
        int stat_multiplier = QUERY_FLAG(food, FLAG_CURSED) ? 2 : 3;

        if (force->stats.Str > 0) {
            force->stats.Str =-force->stats.Str;
        }

        force->stats.Str *= stat_multiplier;

        if (force->stats.Dex > 0) {
            force->stats.Dex =-force->stats.Dex;
        }

        force->stats.Dex *= stat_multiplier;

        if (force->stats.Con > 0) {
            force->stats.Con =-force->stats.Con;
        }

        force->stats.Con *= stat_multiplier;

        if (force->stats.Int > 0) {
            force->stats.Int =-force->stats.Int;
        }

        force->stats.Int *= stat_multiplier;

        if (force->stats.Wis > 0) {
            force->stats.Wis =-force->stats.Wis;
        }

        force->stats.Wis *= stat_multiplier;

        if (force->stats.Pow > 0) {
            force->stats.Pow =-force->stats.Pow;
        }

        force->stats.Pow *= stat_multiplier;

        if (force->stats.Cha > 0) {
            force->stats.Cha =-force->stats.Cha;
        }

        force->stats.Cha *= stat_multiplier;

        for (i = 0; i < NROFATTACKS; i++) {
            if (force->protection[i] > 0) {
                force->protection[i] =-force->protection[i];
            }

            force->protection[i] *= stat_multiplier;
        }
    }

    if (food->speed_left) {
        force->speed = food->speed_left;
    }

    SET_FLAG(force, FLAG_APPLIED);

    force = insert_ob_in_ob(force, who);
    /* Mostly to display any messages */
    change_abil(who, force);
    /* This takes care of some stuff that change_abil() */
    fix_player(who);
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
 * @param who Object eating the food.
 * @param food The food object. */
static void eat_special_food(object *who, object *food)
{
    /* if there is any stat or protection value - create force for the object!
     * */
    if (food->stats.Pow || food->stats.Str || food->stats.Dex || food->stats.Con || food->stats.Int || food->stats.Wis || food->stats.Cha) {
        create_food_force(who, food, get_archetype("force"));
    }
    else {
        int i;

        for (i = 0; i < NROFATTACKS; i++) {
            if (food->protection[i] > 0) {
                create_food_force(who, food, get_archetype("force"));
                break;
            }
        }
    }

    /* Check for hp, sp change */
    if (food->stats.hp) {
        if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
            int tmp = food->stats.hp;

            if (tmp > 0) {
                tmp = -tmp;
            }

            strcpy(CONTR(who)->killer, food->name);

            if (QUERY_FLAG(food, FLAG_CURSED)) {
                who->stats.hp += tmp * 2;
            }
            else {
                who->stats.hp += tmp * 3;
            }

            draw_info(COLOR_WHITE, who, "Eck!... that was rotten food!");
        }
        else {
            draw_info(COLOR_WHITE, who, "You begin to feel better.");
            who->stats.hp += food->stats.hp;

            if (who->stats.hp > who->stats.maxhp) {
                who->stats.hp = who->stats.maxhp;
            }
        }
    }

    if (food->stats.sp) {
        if (QUERY_FLAG(food, FLAG_CURSED) || QUERY_FLAG(food, FLAG_DAMNED)) {
            int tmp = food->stats.sp;

            if (tmp > 0) {
                tmp = -tmp;
            }

            draw_info(COLOR_WHITE, who, "Your mana is drained!");

            if (QUERY_FLAG(food, FLAG_CURSED)) {
                who->stats.sp += tmp * 2;
            }
            else {
                who->stats.sp += tmp * 3;
            }

            if (who->stats.sp < 0) {
                who->stats.sp = 0;
            }
        }
        else {
            draw_info(COLOR_WHITE, who, "You feel a rush of magical energy!");
            who->stats.sp += food->stats.sp;

            if (who->stats.sp > who->stats.maxsp) {
                who->stats.sp = who->stats.maxsp;
            }
        }
    }
}

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    (void) aflags;

    if (applier->type != PLAYER) {
        return OBJECT_METHOD_UNHANDLED;
    }

    if (applier->stats.food + op->stats.food > FOOD_MAX) {
        if ((applier->stats.food + op->stats.food) - FOOD_MAX > op->stats.food / 5) {
            draw_info_format(COLOR_WHITE, applier, "You are too full to %s this right now!", op->type == DRINK ? "drink" : "eat");
            return OBJECT_METHOD_OK;
        }

        if (op->type == FOOD || op->type == FLESH) {
            draw_info(COLOR_WHITE, applier, "You feel full, but what a waste of food!");
        }
        else {
            draw_info(COLOR_WHITE, applier, "Most of the drink goes down your face not your throat!");
        }
    }

    if (!QUERY_FLAG(op, FLAG_CURSED) && !QUERY_FLAG(op, FLAG_DAMNED)) {
        int capacity_remaining;

        capacity_remaining = FOOD_MAX - applier->stats.food;

        if (op->type == DRINK) {
            draw_info_format(COLOR_WHITE, applier, "Ahhh... that %s tasted good.", op->name);
        }
        else {
            draw_info_format(COLOR_WHITE, applier, "The %s tasted %s", op->name, op->type == FLESH ? "terrible!" : "good.");
        }

        applier->stats.food = MAX(0, MIN(FOOD_MAX, applier->stats.food + ABS(op->stats.food)));
        CONTR(applier)->stat_food_consumed += op->stats.food;

        applier->stats.hp += MIN(capacity_remaining, op->stats.food) / 50;

        if (applier->stats.hp > applier->stats.maxhp) {
            applier->stats.hp = applier->stats.maxhp;
        }
    }
    else {
        draw_info_format(COLOR_WHITE, applier, "The %s tasted terrible!", op->name);
        applier->stats.food = MAX(0, MIN(FOOD_MAX, applier->stats.food - ABS(op->stats.food)));
    }

    CONTR(applier)->stat_food_num_consumed++;

    if (op->title || QUERY_FLAG(op, FLAG_CURSED)|| QUERY_FLAG(op, FLAG_DAMNED)) {
        eat_special_food(applier, op);
    }

    decrease_ob(op);
    return OBJECT_METHOD_OK;
}

/**
 * Initialize the food type object methods. */
void object_type_init_food(void)
{
    object_type_methods[FOOD].apply_func = apply_func;
}

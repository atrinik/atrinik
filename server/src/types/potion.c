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
 * Handles potion related code.
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <arch.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>
#include <artifact.h>

#include "common/process_treasure.h"

/**
 * Chance for non-spell potions to become cursed - 1/x.
 */
#define POTION_CHANCE_CURSED 3
/**
 * Chance for non-spell potions to become damned instead of just cursed - 1/x.
 */
#define POTION_CHANCE_DAMNED 2

/**
 * Apply a spell potion.
 *
 * @param op
 * Potion.
 * @param applier
 * Who is applying the potion.
 */
static void
potion_apply_spell (object *op, object *applier)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    SOFT_ASSERT(op->stats.sp >= 0 && op->stats.sp < NROFREALSPELLS,
                "Potion with invalid spell ID %d: %s, applier: %s",
                op->stats.sp,
                object_get_str(op),
                object_get_str(applier));

    /* If it's a cursed potion of remove depletion, drain stats instead. */
    if (op->stats.sp == SP_REMOVE_DEPLETION &&
        (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED))) {
        /* Drain 2 stats if the potion is cursed, 4 if it's damned. */
        for (int i = QUERY_FLAG(op, FLAG_DAMNED) ? 0 : 2; i < 4; i++) {
            drain_stat(applier);
        }

        insert_spell_effect("meffect_purple",
                            applier->map,
                            applier->x,
                            applier->y);
        play_sound_map(applier->map,
                       CMD_SOUND_EFFECT,
                       "poison.ogg",
                       applier->x,
                       applier->y,
                       0,
                       0);
        return;
    }

    /* Fire in the player's facing direction, unless the spell is
     * something like healing or cure disease. */
    int direction = 0;
    if (!(spells[op->stats.sp].flags & SPELL_DESC_SELF)) {
        direction = applier->direction;
    }

    cast_spell(applier,
               op,
               direction,
               op->stats.sp,
               1,
               CAST_POTION,
               NULL);
}

/**
 * Apply a potion which copies stat effects to the applier.
 *
 * @param op
 * Potion.
 * @param applier
 * Who is applying the potion.
 */
static void
potion_apply_effects (object *op, object *applier)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(applier != NULL);

    /* Create a force and copy the effects in. */
    object *force = arch_get("force");
    force->type = POTION_EFFECT;
    /* Copy the amount of time the effect should last. */
    force->stats.food = op->stats.food;
    SET_FLAG(force, FLAG_IS_USED_UP);

    /* Make sure the effect lasts for at least a little while. */
    if (force->stats.food <= 0) {
        force->stats.food = 1;
    }

    if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
        int protection;

        /* The potion is cursed/damned, so the negative effects stay
         * around longer. */
        force->stats.food *= 3;

        /* Copy over protection values to the force from the potion, but
         * negate them first. Attacks are ignored, as it's not actually
         * possible to store negative attack values in objects. */
        for (int i = 0; i < NROFATTACKS; i++) {
            protection = -ABS(op->protection[i]);

            /* If the potion is damned, the effects worsen... */
            if (QUERY_FLAG(op, FLAG_DAMNED)) {
                protection *= 2;
            }

            /* Actually set the protection value, but make sure it's in a
             * valid range. */
            force->protection[i] = MIN(100, MAX(-100, protection));
        }

        insert_spell_effect("meffect_purple",
                            applier->map,
                            applier->x,
                            applier->y);
        play_sound_map(applier->map,
                       CMD_SOUND_EFFECT,
                       "poison.ogg",
                       applier->x,
                       applier->y,
                       0,
                       0);
    } else {
        memcpy(force->protection, op->protection, sizeof(op->protection));
        memcpy(force->attack, op->attack, sizeof(op->attack));

        insert_spell_effect("meffect_green",
                            applier->map,
                            applier->x,
                            applier->y);
        play_sound_map(applier->map,
                       CMD_SOUND_EFFECT,
                       "magic_default.ogg",
                       applier->x,
                       applier->y,
                       0,
                       0);
    }

    /* Copy over stat values. */
    for (int i = 0; i < NUM_STATS; i++) {
        /* Get the stat value from the potion. */
        int val = get_attr_value(&op->stats, i);
        /* No value, nothing to do. */
        if (val == 0) {
            continue;
        }

        /* If the potion is cursed/damned and the stat increase is
         * positive, reverse it. */
        if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
            val = -ABS(val);

            /* The potion is damned, so the negative effect is worse. */
            if (QUERY_FLAG(op, FLAG_DAMNED)) {
                val *= 2;
            }
        }

        /* Now set the stat value of the force to the one calculate
         * above, but make sure it doesn't overflow an int8_t. */
        change_attr_value(&force->stats,
                          i,
                          MIN(INT8_MAX, MAX(INT8_MIN, val)));
    }

    /* Insert the force into the player and apply it. */
    force->speed_left = -1;
    force = object_insert_into(force, applier, 0);
    SOFT_ASSERT(force != NULL,
                "Failed to insert potion effect force into %s",
                object_get_str(applier));
    SET_FLAG(force, FLAG_APPLIED);

    if (!living_update(applier)) {
        draw_info(COLOR_WHITE, applier, "Nothing happened.");
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

    /* Use magic devices skill for using potions. */
    if (!change_skill(applier, SK_MAGIC_DEVICES)) {
        return OBJECT_METHOD_ERROR;
    }

    /* We are using it, so we now know what it does. */
    if (!QUERY_FLAG(op, FLAG_IDENTIFIED)) {
        identify(op);
    }

    CONTR(applier)->stat_potions_used++;

    /* REMOVEME:
     * Old potions of remove depletion used a custom flag instead of using
     * the remove depletion spell, so update them and remove the flag. */
    if (op->last_eat == 1) {
        op->stats.sp = SP_REMOVE_DEPLETION;
        op->last_eat = 0;
    }

    if (op->stats.sp != SP_NO_SPELL) {
        potion_apply_spell(op, applier);
    } else {
        potion_apply_effects(op, applier);
    }

    decrease_ob(op);
    return OBJECT_METHOD_OK;
}

/** @copydoc object_methods_t::process_treasure_func */
static int
process_treasure_func (object  *op,
                       object **ret,
                       int      difficulty,
                       int      affinity,
                       int      flags)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(difficulty > 0);

    /* Avoid processing if the item is already special. */
    if (process_treasure_is_special(op)) {
        return OBJECT_METHOD_UNHANDLED;
    }

    bool generated_art = false;
    switch (op->sub_type) {
    case POTION_NORMAL:
        generated_art = artifact_generate(op, difficulty, affinity);
        break;

    case POTION_BALM:
        op->stats.sp = spell_get_random(difficulty, SPELL_USE_BALM);
        break;

    case POTION_DUST:
        op->stats.sp = spell_get_random(difficulty, SPELL_USE_DUST);
        break;

    default:
        break;
    }

    if (op->stats.sp == SP_NO_SPELL && !generated_art) {
        log_error("Failed to generate a spell/artifact for potion: %s",
                  object_get_str(op));
        object_remove(op, 0);
        object_destroy(op);
        return OBJECT_METHOD_ERROR;
    }

    SET_FLAG(op, FLAG_IS_MAGICAL);

    /* Calculate the level. Essentially -20 below the difficulty at worst, or
     * +15 above the difficulty at best. */
    int level = difficulty - 20 + rndm(0, 35);
    level = MIN(MAX(level, 1), MAXLEVEL);
    op->level = level;

    /* Chance to make special potions damned/cursed, unless we're only
     * generating good treasures. */
    if (op->stats.sp == SP_NO_SPELL &&
        !(flags & GT_ONLY_GOOD) &&
        rndm_chance(POTION_CHANCE_CURSED)) {
        if (rndm_chance(POTION_CHANCE_DAMNED)) {
            SET_FLAG(op, FLAG_DAMNED);
        } else {
            SET_FLAG(op, FLAG_CURSED);
        }
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the potion type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(potion)
{
    OBJECT_METHODS(POTION)->apply_func = apply_func;
    OBJECT_METHODS(POTION)->process_treasure_func = process_treasure_func;
    OBJECT_METHODS(POTION)->override_treasure_processing = true;
}

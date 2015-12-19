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

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
    (void) aflags;

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

    /* Potions with temporary effects. */
    if (op->last_eat == -1) {
        object *force;
        int i, val;

        /* Create a force and copy the effects in. */
        force = arch_get("force");
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
             * longer. */
            force->stats.food *= 3;

            /* Copy over protection values to the force from the potion, but
             * reverse them first. Attacks are ignored, as it's not actually
             * possible to store negative attack values in objects. */
            for (i = 0; i < NROFATTACKS; i++) {
                protection = op->protection[i] > 0 ? -op->protection[i] : op->protection[i];

                /* If the potion is damned, the effects worsen... */
                if (QUERY_FLAG(op, FLAG_DAMNED)) {
                    protection *= 2;
                }

                /* Actually set the protection value, but make sure it's in a
                 * valid range. */
                force->protection[i] = MIN(100, MAX(-100, protection));
            }

            insert_spell_effect("meffect_purple", applier->map, applier->x, applier->y);
            play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
        } else {
            memcpy(force->protection, op->protection, sizeof(op->protection));
            memcpy(force->attack, op->attack, sizeof(op->attack));

            insert_spell_effect("meffect_green", applier->map, applier->x, applier->y);
            play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
        }

        /* Copy over stat values. */
        for (i = 0; i < NUM_STATS; i++) {
            /* Get the stat value from the potion. */
            val = get_attr_value(&op->stats, i);

            /* No value, nothing to do. */
            if (!val) {
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

            /* Now set the stat value of the force to the one calculcated
             * above, but make sure it doesn't overflow sint8. */
            change_attr_value(&force->stats, i, MIN(INT8_MAX, MAX(INT8_MIN, val)));
        }

        /* Insert the force into the player and apply it. */
        force->speed_left = -1;
        force = insert_ob_in_ob(force, applier);
        SET_FLAG(force, FLAG_APPLIED);

        if (!living_update(applier)) {
            draw_info(COLOR_WHITE, applier, "Nothing happened.");
        }
    } else if (op->last_eat == 1) {
        int i;

        /* Potion of minor restoration (removes depletion). */

        /* Cursed potion of minor restoration; reverse effects (stats are
         * depleted). */
        if (QUERY_FLAG(op, FLAG_CURSED) || QUERY_FLAG(op, FLAG_DAMNED)) {
            /* Drain 2 stats if the potion is cursed, 4 if it's damned. */
            for (i = QUERY_FLAG(op, FLAG_DAMNED) ? 0 : 2; i < 4; i++) {
                drain_stat(applier);
            }

            insert_spell_effect("meffect_purple", applier->map, applier->x, applier->y);
            play_sound_map(applier->map, CMD_SOUND_EFFECT, "poison.ogg", applier->x, applier->y, 0, 0);
        } else {
            archetype_t *at;
            object *depletion;

            at = arch_find("depletion");

            depletion = present_arch_in_ob(at, applier);

            if (depletion) {
                for (i = 0; i < NUM_STATS; i++) {
                    if (get_attr_value(&depletion->stats, i)) {
                        draw_info(COLOR_WHITE, applier, restore_msg[i]);
                    }
                }

                SET_FLAG(applier, FLAG_NO_FIX_PLAYER);
                object_remove(depletion, 0);
                object_destroy(depletion);
                CLEAR_FLAG(applier, FLAG_NO_FIX_PLAYER);
                living_update_player(applier);
            } else {
                draw_info(COLOR_WHITE, applier, "You are not depleted.");
            }

            insert_spell_effect("meffect_green", applier->map, applier->x, applier->y);
            play_sound_map(applier->map, CMD_SOUND_EFFECT, "magic_default.ogg", applier->x, applier->y, 0, 0);
        }
    } else if (op->stats.sp != SP_NO_SPELL) {
        /* Spell potion. */

        /* Fire in the player's facing direction, unless the spell is
         * something like healing or cure disease. */
        cast_spell(applier, op, spells[op->stats.sp].flags & SPELL_DESC_SELF ? 0 : applier->direction, op->stats.sp, 1, CAST_POTION, NULL);
    } else {
        draw_info(COLOR_WHITE, applier, "Nothing happens as you apply it.");
    }

    decrease_ob(op);

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the potion type object methods.
 */
void object_type_init_potion(void)
{
    object_type_methods[POTION].apply_func = apply_func;
}

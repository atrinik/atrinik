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
 * Handles code for @ref ROD "rods".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <player.h>
#include <object.h>
#include <object_methods.h>

#include "common/process_treasure.h"

/** @copydoc object_methods_t::process_func */
static void
process_func (object *op)
{
    HARD_ASSERT(op != NULL);

    if (op->stats.hp < op->stats.maxhp) {
        op->stats.hp++;
    }
}

/** @copydoc object_methods_t::ranged_fire_func */
static int
ranged_fire_func (object *op, object *shooter, int dir, double *delay)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(shooter != NULL);

    if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS) {
        draw_info_format(COLOR_WHITE, shooter,
                         "The %s is broken.",
                         op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    /* If the device level is higher than player's magic skill, don't allow
     * using the device. */
    if (shooter->type == PLAYER &&
        op->level > (CONTR(shooter)->skill_ptr[SK_WIZARDRY_SPELLS]->level +
                     settings.magic_devices_level)) {
        draw_info_format(COLOR_WHITE, shooter,
                         "The %s is impossible to handle for you.",
                         op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    if (op->stats.maxhp && op->stats.hp <= 0) {
        play_sound_player_only(CONTR(shooter),
                               CMD_SOUND_EFFECT,
                               "rod.ogg",
                               0,
                               0,
                               0,
                               0);
        draw_info_format(COLOR_WHITE, shooter,
                         "The %s whines for a while, but nothing happens.",
                         op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    if (cast_spell(shooter, op, dir, op->stats.sp, 0, CAST_ROD, NULL)) {
        SET_FLAG(op, FLAG_BEEN_APPLIED);

        if (op->stats.maxhp != 0) {
            op->stats.hp--;
        }
    }

    if (delay != NULL) {
        *delay = op->last_grace;
    }

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

    op->stats.sp = spell_get_random(difficulty, SPELL_USE_ROD);
    if (op->stats.sp == SP_NO_SPELL) {
        log_error("Failed to generate a spell for rod: %s",
                  object_get_str(op));
        object_remove(op, 0);
        object_destroy(op);
        return OBJECT_METHOD_ERROR;
    }

    SET_FLAG(op, FLAG_IS_MAGICAL);

    if (op->stats.maxhp != 0) {
        op->stats.maxhp += rndm(0, op->stats.maxhp);
    }

    op->stats.hp = op->stats.maxhp;

    /* Calculate the level. Essentially -20 below the difficulty at worst, or
     * +15 above the difficulty at best. */
    int level = difficulty - 20 + rndm(0, 15);
    level = MIN(MAX(level, 1), MAXLEVEL);
    op->level = level;

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the rod type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(rod)
{
    OBJECT_METHODS(ROD)->apply_func = object_apply_item;
    OBJECT_METHODS(ROD)->process_func = process_func;
    OBJECT_METHODS(ROD)->ranged_fire_func = ranged_fire_func;
    OBJECT_METHODS(ROD)->process_treasure_func = process_treasure_func;
}

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

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
    if (op->stats.hp < op->stats.maxhp) {
        op->stats.hp++;
    }
}

/** @copydoc object_methods::ranged_fire_func */
static int ranged_fire_func(object *op, object *shooter, int dir, double *delay)
{
    if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS) {
        draw_info_format(COLOR_WHITE, shooter, "The %s is broken.", op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    /* If the device level is higher than player's magic skill,
     * don't allow using the device. */
    if (shooter->type == PLAYER && op->level > CONTR(shooter)->skill_ptr[SK_WIZARDRY_SPELLS]->level + settings.magic_devices_level) {
        draw_info_format(COLOR_WHITE, shooter, "The %s is impossible to handle for you.", op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    if (op->stats.maxhp && op->stats.hp <= 0) {
        play_sound_player_only(CONTR(shooter), CMD_SOUND_EFFECT, "rod.ogg", 0, 0, 0, 0);
        draw_info_format(COLOR_WHITE, shooter, "The %s whines for a while, but nothing happens.", op->name);
        return OBJECT_METHOD_UNHANDLED;
    }

    if (cast_spell(shooter, op, dir, op->stats.sp, 0, CAST_ROD, NULL)) {
        SET_FLAG(op, FLAG_BEEN_APPLIED);

        if (op->stats.maxhp) {
            op->stats.hp--;
        }
    }

    if (delay) {
        *delay = op->last_grace;
    }

    return OBJECT_METHOD_OK;
}

/**
 * Initialize the rod type object methods.
 */
void object_type_init_rod(void)
{
    object_type_methods[ROD].apply_func = object_apply_item;
    object_type_methods[ROD].process_func = process_func;
    object_type_methods[ROD].ranged_fire_func = ranged_fire_func;
}

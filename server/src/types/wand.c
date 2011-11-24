/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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
 * Handles code for @ref WAND "wands".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::ranged_fire_func */
static int ranged_fire_func(object *op, object *shooter, int dir)
{
	if (!op)
	{
		return OBJECT_METHOD_UNHANDLED;
	}

	if (op->stats.sp < 0 || op->stats.sp >= NROFREALSPELLS)
	{
		draw_info_format(COLOR_WHITE, shooter, "The %s is broken.", op->name);
		return OBJECT_METHOD_UNHANDLED;
	}

	if (op->stats.food <= 0)
	{
		play_sound_player_only(CONTR(shooter), CMD_SOUND_EFFECT, "rod.ogg", 0, 0, 0, 0);
		draw_info_format(COLOR_WHITE, shooter, "The %s says poof.", op->name);
		return OBJECT_METHOD_UNHANDLED;
	}

	if (cast_spell(shooter, op, dir, op->stats.sp, 0, CAST_WAND, NULL))
	{
		SET_FLAG(op, FLAG_BEEN_APPLIED);
		op->stats.food--;
	}

	if (shooter->chosen_skill)
	{
		shooter->chosen_skill->stats.maxsp = op->last_grace;
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the wand type object methods. */
void object_type_init_wand(void)
{
	object_type_methods[WAND].apply_func = object_apply_item;
	object_type_methods[WAND].ranged_fire_func = ranged_fire_func;
}

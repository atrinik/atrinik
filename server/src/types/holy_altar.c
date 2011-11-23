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
 * Handles code related to @ref HOLY_ALTAR "holy altars".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	object *god;

	(void) aflags;

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_OK;
	}

	draw_info_format(COLOR_WHITE, applier, "You touch the %s.", op->name);

	if (!change_skill(applier, SK_PRAYING))
	{
		draw_info(COLOR_WHITE, applier, "Nothing happens. It seems you miss the right skill.");
		return OBJECT_METHOD_OK;
	}

	/* Non-consecrated altar, not much to do. */
	if (!op->other_arch)
	{
		return OBJECT_METHOD_OK;
	}

	god = find_god(determine_god(applier));

	/* No god yet. */
	if (!god)
	{
		become_follower(applier, &op->other_arch->clone);
	}
	/* Pray at your god's altar. */
	else if (god->name == op->other_arch->clone.name)
	{
		int bonus;

		draw_info_format(COLOR_WHITE, applier, "You feel the powers of your deity %s.", god->name);

		bonus = ((applier->stats.Wis / 10) + (SK_level(applier) / 10));

		applier->stats.grace = MIN(applier->stats.maxgrace, applier->stats.grace + bonus);

		if (rndm_chance(MAX(1, 100 - bonus)))
		{
			god_intervention(applier, god);
		}
	}
	/* Praying to another god! */
	else
	{
		if (god->other_arch && (op->other_arch->name == god->other_arch->name))
		{
			draw_info_format(COLOR_NAVY, applier, "Foolish heretic! %s is livid!", god->name);
		}
		else
		{
			draw_info_format(COLOR_NAVY, applier, "Heretic! %s is angered!", god->name);
		}

		/* May switch gods, but it's random chance based on our current
		 * level. Note it gets harder to swap gods the higher we get. */
		if (QUERY_FLAG(op, FLAG_XRAYS) || rndm_chance(applier->chosen_skill->exp_obj->level))
		{
			become_follower(applier, &op->other_arch->clone);
		}
		/* Toss this player off the altar. He can try again. */
		else
		{
			draw_info_format(COLOR_NAVY, applier, "A divine force pushes you off the altar.");
			move_object(applier, absdir(applier->facing + 4));
		}
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the holy altar type object methods. */
void object_type_init_holy_altar(void)
{
	object_type_methods[HOLY_ALTAR].apply_func = apply_func;
}

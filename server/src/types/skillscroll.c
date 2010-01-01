/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Handles code used by @ref SKILLSCROLL "skillscrolls". */

#include <global.h>
#include <sproto.h>

/**
 * Apply a skillscroll.
 * @param op The player applying the skillscroll.
 * @param tmp The skillscroll. */
void apply_skillscroll(object *op, object *tmp)
{
	/* Must be applied by a player. */
	if (!op->type == PLAYER)
	{
		return;
	}

	switch (learn_skill(op, tmp, NULL, 0, 1))
	{
		case 0:
			new_draw_info(NDI_UNIQUE, 0, op, "You already possess the knowledge ");
			new_draw_info_format(NDI_UNIQUE, 0, op, "held within the %s.\n", query_name(tmp, NULL));
			return;

		case 1:
			new_draw_info_format(NDI_UNIQUE, 0, op, "You succeed in learning %s", skills[tmp->stats.sp].name);
			/* to immediately link new skill to exp object */
			fix_player(op);
			decrease_ob(tmp);
			return;

		default:
			new_draw_info_format(NDI_UNIQUE, 0, op, "You fail to learn the knowledge of the %s.\n", query_name(tmp, NULL));
			decrease_ob(tmp);
			return;
	}
}

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
 * Handles code related to @ref SIGN "signs". */

#include <global.h>

/**
 * Apply a sign or trigger a magic mouth.
 *
 * Signs and magic mouths can have a "counter" value, which will make it
 * possible to read/trigger it only so many times.
 * @param op Object applying the sign
 * @param sign The sign or magic mouth object. */
void apply_sign(object *op, object *sign)
{
	if (sign->msg == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Nothing is written on it.");
		return;
	}

	if (sign->stats.food)
	{
		if (sign->last_eat >= sign->stats.food)
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
			{
				new_draw_info(NDI_UNIQUE, op, "You cannot read it anymore.");
			}

			return;
		}

		sign->last_eat++;
	}

	/* Sign or magic mouth?  Do we need to see it, or does it talk to us?
	 * No way to know for sure.
	 *
	 * This check fails for signs with FLAG_WALK_ON/FLAG_FLY_ON.  Checking
	 * for FLAG_INVISIBLE instead of FLAG_WALK_ON/FLAG_FLY_ON would fail
	 * for magic mouths that have been made visible. */
	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ) && !QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to read while blind.");
		return;
	}

	new_draw_info(NDI_UNIQUE | NDI_NAVY, op, sign->msg);
}

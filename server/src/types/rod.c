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
 * @ref ROD "Rod" related code. */

#include <global.h>
#include <sproto.h>

/**
 * Regenerate rod speed needed to fire the rod again.
 * @param rod The rod object to regenerate. */
void regenerate_rod(object *rod)
{
	if (++rod->stats.food > rod->stats.hp / 10 || rod->type == HORN)
	{
		rod->stats.food = 0;

		if (rod->stats.hp < rod->stats.maxhp)
		{
			rod->stats.hp += 1 + rod->stats.maxhp / 10;

			if (rod->stats.hp > rod->stats.maxhp)
			{
				rod->stats.hp = rod->stats.maxhp;
			}

			fix_rod_speed(rod);
		}
	}
}

/**
 * Drain charges from a rod.
 * @param rod Rod to drain. */
void drain_rod_charge(object *rod)
{
	rod->stats.hp -= spells[rod->stats.sp].sp;

	if (QUERY_FLAG(rod, FLAG_ANIMATE))
	{
		fix_rod_speed(rod);
	}
}

/**
 * Fix the speed of the rod, based on its hp.
 * @param rod Rod to fix. */
void fix_rod_speed(object *rod)
{
	rod->speed = (FABS(rod->arch->clone.speed) * rod->stats.hp) / (float) rod->stats.maxhp;

	if (rod->speed < 0.02f)
	{
		rod->speed = 0.02f;
	}

	update_ob_speed(rod);
}

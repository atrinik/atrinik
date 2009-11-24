/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * @ref DEEP_SWAMP "Deep swamp" related functions. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/**
 * Walk on deep swamp.
 * @param op The swamp object
 * @param victim Object walking on this swamp */
void walk_on_deep_swamp(object *op, object *victim)
{
	if (victim->type == PLAYER && !QUERY_FLAG(victim, FLAG_FLYING) && victim->stats.hp >= 0)
	{
		new_draw_info(NDI_UNIQUE, 0, victim, "You are down to your knees in the swamp.");
		op->stats.food = 1;
		victim->speed_left -= (float) (SLOW_PENALTY(op));
	}
}

/**
 * Process deep swamp object.
 * @param op The deep swamp object. */
void move_deep_swamp(object *op)
{
	object *above = op->above, *nabove;

	while (above)
	{
		nabove = above->above;
		if (above->type == PLAYER && !QUERY_FLAG(above, FLAG_FLYING) && above->stats.hp >= 0)
		{
			if (op->stats.food < 1)
			{
				LOG(llevDebug, "move_deep_swamp(): player is here, but state is %d\n", op->stats.food);
				op->stats.food = 1;
			}

			switch (op->stats.food)
			{
				case 1:
					if (rndm(0, 2) == 0)
					{
						new_draw_info(NDI_UNIQUE, 0, above, "You are down to your waist in the wet swamp.");
						op->stats.food = 2;
						above->speed_left -= (float) (SLOW_PENALTY(op));
					}

					break;

				case 2:
					if (rndm(0, 2) == 0)
					{
						new_draw_info(NDI_UNIQUE, 0, above, "You are down to your NECK in the dangerous swamp.");
						op->stats.food = 3;
						strcpy(CONTR(above)->killer, "drowning in a swamp");
						above->stats.hp--;
						above->speed_left -= (float) (SLOW_PENALTY(op));
					}

					break;

				case 3:
					if (rndm(0, 4) == 0)
					{
						/* player is ready to drown */
						if (rndm(0, 4) == 0)
						{
							op->stats.food = 0;
							new_draw_info_format(NDI_UNIQUE | NDI_ALL, 1, NULL, "%s disappeared into a swamp.", above->name);
							strcpy(CONTR(above)->killer, "drowning in a swamp");

							above->stats.hp = -1;
							/* player dies in the swamp */
							kill_player(above);
						}
					}

					break;
			}
		}
		else if (!IS_LIVE(above))
		{
			if (rndm(0, 2) == 0)
			{
				decrease_ob(above);
			}
		}

		above = nabove;
	}
}

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
 * Handles code used for @ref PLAYERMOVER "player movers". */

#include <global.h>
#include <sproto.h>

/**
 * A player mover finds any players that are sitting on it. It moves them
 * based on the mover's direction.
 * @param op The player mover. */
void move_player_mover(object *op)
{
	object *victim, *nextmover;
	mapstruct *mt;
	int xt, yt, dir = op->direction;

	if (!(blocked(NULL, op->map, op->x, op->y, TERRAIN_NOTHING) & (P_IS_ALIVE | P_IS_PLAYER)))
	{
		return;
	}

	/* Determine direction now for random movers so we do the right thing */
	if (!dir)
	{
		dir = get_random_dir();
	}

	for (victim = GET_BOTTOM_MAP_OB(op); victim != NULL; victim = victim->above)
	{
		if (IS_LIVE(victim) && (!(QUERY_FLAG(victim, FLAG_FLYING)) || op->stats.maxhp))
		{
			if (QUERY_FLAG(op, FLAG_LIFESAVE) && op->stats.hp-- < 0)
			{
				destruct_ob(op);
				return;
			}

			xt = op->x + freearr_x[dir];
			yt = op->y + freearr_y[dir];

			if (!(mt = get_map_from_coord(op->map, &xt, &yt)))
			{
				return;
			}

			for (nextmover = get_map_ob(mt, xt, yt); nextmover != NULL; nextmover = nextmover->above)
			{
				if (nextmover->type == PLAYERMOVER)
				{
					nextmover->speed_left = -0.99f;
				}

				/* wait until the next thing gets out of the way */
				if (IS_LIVE(nextmover))
				{
					op->speed_left = -1.1f;
				}
			}

			if (victim->type == PLAYER)
			{
				/* only level >=1 movers move people */
				if (op->level)
				{
					/* Following is a bit of hack.  We need to make sure it
					 * is cleared, otherwise the player will get stuck in
					 * place.  This can happen if the player used a spell to
					 * get to this space. */
					CONTR(victim)->fire_on = 0;
					victim->speed_left = -FABS(victim->speed);
					move_player(victim, dir);
				}
				else
				{
					return;
				}
			}
			else
			{
				move_object(victim, dir);
			}

			if (!op->stats.maxsp && op->stats.sp)
			{
				op->stats.maxsp = 2;
			}

			/* flag to paralyze the player */
			if (op->stats.sp)
			{
				victim->speed_left = -FABS(op->stats.maxsp * victim->speed / op->speed);
			}
		}
	}
}

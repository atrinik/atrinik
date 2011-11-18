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
 * Handles code used for @ref PLAYER_MOVER "player movers".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
	object *victim, *victim_next;
	mapstruct *mt;
	int xt, yt, dir = op->direction;

	op->value = pticks;

	if (!(blocked(NULL, op->map, op->x, op->y, TERRAIN_NOTHING) & (P_IS_MONSTER | P_IS_PLAYER)))
	{
		return;
	}

	/* Determine direction now for random movers so we do the right thing. */
	if (!dir)
	{
		dir = get_random_dir();
	}

	for (victim = GET_BOTTOM_MAP_OB(op); victim; victim = victim_next)
	{
		victim_next = victim->above;

		if (IS_LIVE(victim) && (!(QUERY_FLAG(victim, FLAG_FLYING)) || op->stats.maxhp))
		{
			if (QUERY_FLAG(op, FLAG_LIFESAVE) && op->stats.hp-- < 0)
			{
				destruct_ob(op);
				return;
			}

			/* No direction, this means 'xrays 1' was set; so use the
			 * victim's direction instead. */
			if (!op->direction && QUERY_FLAG(op, FLAG_XRAYS))
			{
				dir = victim->direction;
			}

			xt = op->x + freearr_x[dir];
			yt = op->y + freearr_y[dir];

			if (!(mt = get_map_from_coord(op->map, &xt, &yt)))
			{
				return;
			}

			/* Flag to stop moving if there's a wall. */
			if (QUERY_FLAG(op, FLAG_STAND_STILL) && blocked(victim, mt, xt, yt, victim->terrain_flag))
			{
				continue;
			}

			/* Unless there is an alive object or a player on the square
			 * this object is being moved onto, disable the mover on that
			 * square, if any. This is done so the object doesn't rocket
			 * across a bunch of movers. */
			if (!(blocked(NULL, mt, xt, yt, TERRAIN_NOTHING) & (P_IS_MONSTER | P_IS_PLAYER)))
			{
				object *nextmover;

				for (nextmover = GET_MAP_OB(mt, xt, yt); nextmover; nextmover = nextmover->above)
				{
					/* Only disable movers that didn't go this tick yet;
					 * otherwise they wouldn't trigger on the next tick to
					 * move objects they may have on top of them. */
					if (nextmover->type == PLAYER_MOVER && nextmover->value != op->value)
					{
						nextmover->speed_left--;
					}
				}
			}

			if (victim->type == PLAYER)
			{
				/* only level >=1 movers move people */
				if (op->level)
				{
					/* Following is a bit of hack.  We need to make sure it
					 * is cleared, otherwise the player will get stuck in
					 * place. This can happen if the player used a spell to
					 * get to this space. */
					CONTR(victim)->fire_on = 0;
					victim->speed_left = -FABS(victim->speed);
					move_player(victim, dir);
					/* Clear player's path; they probably can't move there
					 * any more after being pushed, or might not want to. */
					player_path_clear(CONTR(victim));
				}
				else
				{
					continue;
				}
			}
			else if (op->stats.grace)
			{
				move_object(victim, dir);
			}
			else
			{
				continue;
			}

			if (!op->stats.maxsp && op->stats.sp)
			{
				op->stats.maxsp = 2;
			}

			/* flag to paralyze the player */
			if (op->stats.sp)
			{
				victim->speed_left = -(op->stats.maxsp * FABS(victim->speed));
			}
		}
	}
}

/**
 * Initialize the player mover type object methods. */
void object_type_init_player_mover(void)
{
	object_type_methods[PLAYER_MOVER].process_func = process_func;
}

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
 * Code related to @ref GATE "gate" and @ref TIMED_GATE "timed gate"
 * object types. */

#include <global.h>
#include <sproto.h>

/**
 * Move a gate up/down.
 * @param op Gate object
 * @todo I have not included damage to mobs/player on reverse up going gates!
 * Look in the code! */
void move_gate(object *op)
{
	object *tmp;
	/* default update is only face */
	int update = UP_OBJ_FACE;

	if (op->stats.wc < 0 || (int) op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)))
	{
		LOG(llevBug, "BUG: move_gate(): Gate animation was %d, max=%d\n", op->stats.wc, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)));
		op->stats.wc = 0;
	}

	/* We're going down (or reverse up) */
	if (op->value)
	{
		/* Reached bottom, let's stop */
		if (--op->stats.wc <= 0)
		{
			op->stats.wc = 0;

			if (op->arch->clone.speed)
			{
				op->value = 0;
			}
			else
			{
				op->speed = 0;
				update_ob_speed(op);
			}
		}

		if ((int) op->stats.wc < ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) / 2 + 1))
		{
			/* We do the QUERY_FLAG() here to check we must rebuild the tile flags or not,
			 * if we don't change the object settings here, just change the face but
			 * don't rebuild the flag tiles.
			 * If != 0, we have a reversed timed gate, which starts open */
			if (op->last_heal)
			{
				if (!QUERY_FLAG(op, FLAG_NO_PASS))
				{
					update = UP_OBJ_FLAGFACE;
				}

				/* The coast is clear, block the way */
				SET_FLAG(op, FLAG_NO_PASS);

				if (!op->arch->clone.stats.ac)
				{
					if (!QUERY_FLAG(op, FLAG_BLOCKSVIEW))
					{
						update = UP_OBJ_FLAGFACE;
					}

					SET_FLAG(op, FLAG_BLOCKSVIEW);
				}
			}
			else
			{
				if (QUERY_FLAG(op, FLAG_NO_PASS))
				{
					update = UP_OBJ_FLAGFACE;
				}

				CLEAR_FLAG(op, FLAG_NO_PASS);

				if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
				{
					update = UP_OBJ_FLAGFACE;
				}

				CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, update);

		return;
	}

	/* First, lets see if we are already at the top */
	if ((unsigned char) op->stats.wc == ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1))
	{
		/* Check to make sure that only non pickable and non rollable
		 * objects are above the gate. If so, we finish closing the gate,
		 * otherwise, we fall through to the code below which should lower
		 * the gate slightly. */
		for (tmp = op->above; tmp != NULL; tmp = tmp->above)
		{
			if (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || IS_LIVE(tmp))
			{
				break;
			}
		}

		if (tmp == NULL)
		{
			if (op->arch->clone.speed)
			{
				op->value = 1;
			}
			else
			{
				op->speed = 0;

				/* Reached top, let's stop */
				update_ob_speed(op);
			}

			return;
		}
	}

	/* The gate is going temporarily down */
	if (op->stats.food)
	{
		/* Gone all the way down? */
		if (--op->stats.wc <= 0)
		{
			/* Then let's try again */
			op->stats.food = 0;
			op->stats.wc = 0;
		}
	}
	/* The gate is still going up */
	else
	{
		op->stats.wc++;

		if ((int) op->stats.wc >= ((NUM_ANIMATIONS(op) / NUM_FACINGS(op))))
		{
			op->stats.wc = (signed char) (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1;
		}

		/* If there is something on top of the gate, we try to roll it off.
		 * If a player/monster, we don't roll, we just hit them with damage */
		if ((int) op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) / 2)
		{
			/* Halfway or further, check blocks
			 * First, get the top object on the square. */
			for (tmp = op->above; tmp != NULL && tmp->above != NULL; tmp = tmp->above)
			{
			}

			if (tmp != NULL)
			{
				if (IS_LIVE(tmp))
				{
					hit_player(tmp, 4, op, AT_PHYSICAL);

					if (tmp->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, tmp, "You are crushed by the %s!", op->name);
					}
				}

				/* If the object is not alive, and the object either can
				 * be picked up or the object rolls, move the object
				 * off the gate. */
				if (IS_LIVE(tmp) || (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL)))
				{
					/* If it has speed, it should move itself, otherwise: */
					int i = find_free_spot(tmp->arch, NULL, op->map, op->x, op->y, 1, 9);

					/* If there is a free spot, move the object someplace */
					if (i != -1)
					{
						remove_ob(tmp);
						check_walk_off(tmp, NULL, MOVE_APPLY_VANISHED);
						tmp->x += freearr_x[i], tmp->y += freearr_y[i];
						insert_ob_in_map(tmp, op->map, op, 0);
					}
				}
			}

			/* See if there is still anything blocking the gate */
			for (tmp = op->above; tmp != NULL; tmp = tmp->above)
			{
				if (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || IS_LIVE(tmp))
				{
					break;
				}
			}

			/* If there is, start putting the gate down */
			if (tmp)
			{
				op->stats.food = 1;
			}
			else
			{
				/* If != 0, we have a reversed timed gate, which starts open */
				if (op->last_heal)
				{
					if (QUERY_FLAG(op, FLAG_NO_PASS))
						update = UP_OBJ_FLAGFACE;

					CLEAR_FLAG(op, FLAG_NO_PASS);

					if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
						update = UP_OBJ_FLAGFACE;

					CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
				}
				else
				{
					if (!QUERY_FLAG(op, FLAG_NO_PASS))
						update = UP_OBJ_FLAGFACE;

					/* The coast is clear, block the way */
					SET_FLAG(op, FLAG_NO_PASS);

					if (!op->arch->clone.stats.ac)
					{
						if (!QUERY_FLAG(op, FLAG_BLOCKSVIEW))
							update = UP_OBJ_FLAGFACE;

						SET_FLAG(op, FLAG_BLOCKSVIEW);
					}
				}
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);

		/* Takes care about map tile and player los update! */
		update_object(op, update);
	}
}

/**
 * Move a timed gate.
 * @param op Timed gate object */
void move_timed_gate(object *op)
{
	int v = op->value;

	if (op->stats.sp)
	{
		move_gate(op);

		/* Change direction? */
		if (op->value != v)
		{
			op->stats.sp = 0;
		}

		return;
	}

	/* Keep gate down */
	if (--op->stats.hp <= 0)
	{
		move_gate(op);

		/* Ready? */
		if (op->value != v)
		{
			op->speed = 0;
			update_ob_speed(op);
		}
	}
}

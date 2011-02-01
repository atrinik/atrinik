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
 * Handles code used for @ref PIT "pits". */

#include <global.h>

/**
 * Close or open a pit. op->value is set when something connected to
 * the pit is triggered.
 * @param op The pit object */
void move_pit(object *op)
{
	object *next,*tmp;

	/* We're opening */
	if (op->value)
	{
		/* Opened, let's stop */
		if (--op->stats.wc <= 0)
		{
			op->stats.wc = 0;
			op->speed = 0;
			update_ob_speed(op);
			SET_FLAG(op, FLAG_WALK_ON);

			for (tmp = op->above; tmp != NULL; tmp = next)
			{
				next = tmp->above;
				move_apply(op, tmp, tmp, 0);
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
		return;
	}

	/* We're closing */
	CLEAR_FLAG(op, FLAG_WALK_ON);
	op->stats.wc++;

	if ((int) op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
	{
		op->stats.wc = NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1;
	}

	op->state = (uint8) op->stats.wc;
	SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
	update_object(op, UP_OBJ_FACE);

	if ((unsigned char) op->stats.wc == (NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1))
	{
		op->speed = 0;

		/* Closed, let's stop */
		update_ob_speed(op);
		return;
	}
}

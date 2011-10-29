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

static int move_on_func(object *trap, object *victim, object *originator)
{
	(void) originator;

	/* Hole not open? */
	if (trap->stats.wc > 0)
	{
		return OBJECT_METHOD_OK;
	}

	draw_info(COLOR_WHITE, victim, "You fall through the hole!\n");
	play_sound_map(victim->map, CMD_SOUND_EFFECT, "fallhole.ogg", victim->x, victim->y, 0, 0);
	transfer_ob(HEAD(victim), EXIT_X(trap), EXIT_Y(trap), trap->last_sp, victim, trap);

	return OBJECT_METHOD_OK;
}

static void process_func(object *op)
{
	/* We're opening. */
	if (op->value)
	{
		/* Opened, let's stop. */
		if (--op->stats.wc <= 0)
		{
			object *tmp, *next;

			op->stats.wc = 0;
			op->speed = 0;
			update_ob_speed(op);
			SET_FLAG(op, FLAG_WALK_ON);

			for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = next)
			{
				next = tmp->above;
				object_move_on(op, tmp, tmp);
			}
		}

		op->state = op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
	}
	/* We're closing. */
	else
	{
		CLEAR_FLAG(op, FLAG_WALK_ON);
		op->stats.wc++;

		if ((int) op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
		{
			op->stats.wc = NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1;
		}

		op->state = op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);

		/* If closed, stop. */
		if ((uint8) op->stats.wc == (NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1))
		{
			op->speed = 0;
			update_ob_speed(op);
		}
	}
}

/**
 * Initialize the pit type object methods. */
void object_type_init_pit(void)
{
	object_type_methods[PIT].move_on_func = move_on_func;
	object_type_methods[PIT].process_func = process_func;
}

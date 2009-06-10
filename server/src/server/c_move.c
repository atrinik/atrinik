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
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

static int move_internal(object *op, char *params, int dir)
{
	if (params)
	{
		if (params[0] == 'f')
		{
			if (!CONTR(op)->fire_on)
			{
				CONTR(op)->fire_on = 1;
				move_player(op, dir);
				CONTR(op)->fire_on = 0;
				return 0;
			}
		}
		else if (params[0] == 'r' && !CONTR(op)->run_on)
			CONTR(op)->run_on = 1;
	}
	move_player(op, dir);
	return 0;
}

int command_east(object *op, char *params)
{
  	return move_internal(op, params, 3);
}

int command_north(object *op, char *params)
{
  	return move_internal(op, params, 1);
}

int command_northeast(object *op, char *params)
{
  	return move_internal(op, params, 2);
}

int command_northwest(object *op, char *params)
{
  	return move_internal(op, params, 8);
}

int command_south(object *op, char *params)
{
  	return move_internal(op, params, 5);
}

int command_southeast(object *op, char *params)
{
  	return move_internal(op, params, 4);
}

int command_southwest(object *op, char *params)
{
  	return move_internal(op, params, 6);
}

int command_west(object *op, char *params)
{
  	return move_internal(op, params, 7);
}

int command_stay(object *op, char *params)
{
	if (!CONTR(op)->fire_on && (!params || params[0] != 'f'))
		return 0;

	fire(op, 0);
	return 0;
}

int command_turn_right(object *op, char *params)
{
    sint8 dir = absdir(op->facing + 1);

    op->anim_last_facing = op->anim_last_facing_last = op->facing = dir;

    return 1;
}

int command_turn_left(object *op, char *params)
{
    sint8 dir = absdir(op->facing - 1);

    op->anim_last_facing = op->anim_last_facing_last = op->facing = dir;

    return 1;
}

int command_push_object(object *op, char *params)
{
	push_roll_object(op, op->facing, TRUE);
	return 0;
}

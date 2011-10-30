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
 * Handles code related to @ref LIGHTNING "lightning". */

#include <global.h>

/**
 * Causes lightning to fork.
 * @param op Original bolt.
 * @param tmp First piece of the fork. */
static void lightning_fork(object *op, object *tmp)
{
	mapstruct *m;
	int x, y, dir, dir_adjust;
	object *bolt;

	if (!tmp->stats.Dex || rndm(1, 100) > tmp->stats.Dex)
	{
		return;
	}

	/* Fork left. */
	if (rndm(0, 99) < tmp->stats.Con)
	{
		dir_adjust = -1;
	}
	/* Fork right. */
	else
	{
		dir_adjust = 1;
	}

	dir = absdir(tmp->direction + dir_adjust);

	x = tmp->x + freearr_x[dir];
	y = tmp->y + freearr_y[dir];
	m = get_map_from_coord(tmp->map, &x, &y);

	if (!m || wall(m, x, y))
	{
		return;
	}

	bolt = get_object();
	copy_object(tmp, bolt, 0);

	bolt->stats.food = 0;
	/* Reduce chances of subsequent forking. */
	bolt->stats.Dex -= 10;
	/* Less forks from main bolt too. */
	tmp->stats.Dex -= 10;
	/* Adjust the left bias. */
	bolt->stats.Con += 25 * dir;
	bolt->speed_left = -0.1f;
	bolt->direction = dir;
	bolt->stats.hp++;
	bolt->x = x;
	bolt->y = y;
	/* Reduce daughter bolt damage. */
	bolt->stats.dam /= 2;
	bolt->stats.dam++;
	/* Reduce father bolt damage. */
	tmp->stats.dam /= 2;
	tmp->stats.dam++;

	bolt = insert_ob_in_map(bolt, m, op, 0);

	if (!bolt)
	{
		return;
	}

	update_turn_face(bolt);
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
	object *tmp;

	if (--(op->stats.hp) < 0)
	{
		destruct_ob(op);
		return;
	}

	if (!op->direction)
	{
		return;
	}

	if (blocks_magic(op->map, op->x + DIRX(op), op->y + DIRY(op)))
	{
		return;
	}

	if (!bullet_reflect(op, op->map, op->x + DIRX(op), op->y + DIRY(op)))
	{
		check_fired_arch(op);

		if (!OBJECT_ACTIVE(op))
		{
			return;
		}
	}

	if (wall(op->map, op->x + DIRX(op), op->y + DIRY(op)))
	{
		if (QUERY_FLAG(op, FLAG_REFLECTING))
		{
			if (op->direction & 1)
			{
				op->direction = absdir(op->direction + 4);
			}
			else
			{
				int left, right;

				left = wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]);
				right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (left == right)
				{
					op->direction = absdir(op->direction + 4);
				}
				else if (left)
				{
					op->direction = absdir(op->direction + 2);
				}
				else if (right)
				{
					op->direction = absdir(op->direction - 2);
				}
			}

			update_turn_face(op);
		}
		else
		{
			return;
		}
	}

	if (!op->stats.hp)
	{
		return;
	}

	if (op->stats.food == 0)
	{
		remove_ob(op);
		op->x = op->x + DIRX(op);
		op->y = op->y + DIRY(op);
		op = insert_ob_in_map(op, op->map, op, 0);

		if (!op)
		{
			return;
		}

		op->stats.food = 1;
	}
	else if (op->stats.food == 1)
	{
		tmp = get_object();
		copy_object(op, tmp, 0);
		tmp->speed_left = -0.1f;
		tmp->x = op->x + DIRX(tmp);
		tmp->y = op->y + DIRY(tmp);
		tmp = insert_ob_in_map(tmp, op->map, op, 0);

		if (!tmp)
		{
			return;
		}

		op->stats.food = 2;

		lightning_fork(op, tmp);
	}
}

/**
 * Initialize the lightning type object methods. */
void object_type_init_lightning(void)
{
	object_type_methods[LIGHTNING].process_func = process_func;
}

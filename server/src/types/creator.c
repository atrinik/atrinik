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
 * Handles code used for @ref CREATOR "creators".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Check whether creator has already created the specified object on its
 * square.
 * @param op The creator.
 * @param check The object to check for.
 * @return 1 if such object already exists, 0 otherwise. */
static int creator_obj_exists(object *op, object *check)
{
	object *tmp;

	FOR_MAP_LAYER_BEGIN(op->map, op->x, op->y, check->layer, tmp)
	{
		if (tmp->arch == check->arch && tmp->name == check->name && tmp->type == check->type)
		{
			return 1;
		}
	}
	FOR_MAP_LAYER_END

	return 0;
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
	int idx, roll;
	object *tmp, *clone_ob;
	uint8 created;

	(void) cause;
	(void) state;

	if (op->stats.hp <= 0 && !QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		return OBJECT_METHOD_OK;
	}

	created = 0;

	if (QUERY_FLAG(op, FLAG_SPLITTING))
	{
		int num_obs;

		num_obs = 0;

		for (tmp = op->inv; tmp; tmp = tmp->below)
		{
			if (tmp->type == EVENT_OBJECT)
			{
				continue;
			}

			num_obs++;
		}

		roll = rndm(1, num_obs) - 1;
	}

	for (tmp = op->inv, idx = 0; tmp; tmp = tmp->below, idx++)
	{
		if (tmp->type == EVENT_OBJECT)
		{
			continue;
		}

		if (QUERY_FLAG(op, FLAG_SPLITTING) && roll != idx)
		{
			continue;
		}

		if (QUERY_FLAG(op, FLAG_ONE_DROP) && creator_obj_exists(op, tmp))
		{
			continue;
		}

		clone_ob = object_create_clone(tmp);
		clone_ob->x = op->x;
		clone_ob->y = op->y;
		clone_ob = insert_ob_in_map(clone_ob, op->map, op, 0);

		if (clone_ob)
		{
			created = 1;
		}
	}

	if (created && !QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		op->stats.hp--;
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the creator type object methods. */
void object_type_init_creator(void)
{
	object_type_methods[CREATOR].trigger_func = trigger_func;
}

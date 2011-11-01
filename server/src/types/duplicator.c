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
 * Handles code related to @ref DUPLICATOR "duplicators".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Try matching an object for duplicator.
 * @param op Duplicator.
 * @param tmp The object to try to match. */
static void duplicator_match_obj(object *op, object *tmp)
{
	if (op->slaying != tmp->arch->name)
	{
		return;
	}

	if (op->level <= 0)
	{
		destruct_ob(tmp);
	}
	else
	{
		tmp->nrof = MIN(SINT32_MAX, (uint64) tmp->nrof * op->level);
	}
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
	(void) originator;

	if (state)
	{
		duplicator_match_obj(op, victim);
	}

	return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
	object *tmp, *next;

	(void) cause;
	(void) state;

	for (tmp = GET_MAP_OB(op->map, op->x, op->y); tmp; tmp = next)
	{
		next = tmp->above;

		duplicator_match_obj(op, tmp);
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the duplicator type object methods. */
void object_type_init_duplicator(void)
{
	object_type_methods[DUPLICATOR].move_on_func = move_on_func;
	object_type_methods[DUPLICATOR].trigger_func = trigger_func;
}

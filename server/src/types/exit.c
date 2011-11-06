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
 * Handles code related to @ref EXIT "exits".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	(void) aflags;

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_OK;
	}

	/* If no map path specified, we assume it is the map path of the exit. */
	if (!EXIT_PATH(op) && op->map)
	{
		FREE_AND_ADD_REF_HASH(EXIT_PATH(op), op->map->path);
	}

	if (!EXIT_PATH(op) || (EXIT_Y(op) == -1 && EXIT_X(op) == -1))
	{
		if (!QUERY_FLAG(op, FLAG_SYS_OBJECT))
		{
			draw_info_format(COLOR_WHITE, applier, "The %s is closed.", query_name(op, NULL));
		}
	}
	else
	{
		/* Don't display messages for random maps. */
		if (op->msg && strncmp(EXIT_PATH(op), "/!", 2) != 0 && strncmp(EXIT_PATH(op), "/random/", 8) != 0)
		{
			draw_info(COLOR_NAVY, applier, op->msg);
		}

		enter_exit(applier, op);
	}

	return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator, int state)
{
	(void) originator;
	(void) state;

	return apply_func(op, victim, 0);
}

/**
 * Initialize the exit type object methods. */
void object_type_init_exit(void)
{
	object_type_methods[EXIT].apply_func = apply_func;
	object_type_methods[EXIT].move_on_func = move_on_func;
}

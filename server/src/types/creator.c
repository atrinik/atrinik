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
 * Handles code used for @ref CREATOR "creators". */

#include <global.h>

/**
 * Have a creator do its tick.
 * @param op The creator.
 * @todo Check if it works properly with multi arch objects.
 * @todo Perhaps if other_arch is set try to check for an object in its
 * inventory, and copy any modified values? */
void move_creator(object *op)
{
	object *tmp;

	if (!op->other_arch)
	{
		return;
	}

	op->stats.hp--;

	if (op->stats.hp < 0 && !QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		op->stats.hp = -1;
		return;
	}

	tmp = arch_to_object(op->other_arch);

	if (op->slaying)
	{
		FREE_AND_COPY_HASH(tmp->name, op->slaying);
		FREE_AND_COPY_HASH(tmp->title, op->slaying);
	}

	tmp->x = op->x;
	tmp->y = op->y;
	tmp->map = op->map;
	tmp->level = op->level;
	insert_ob_in_map(tmp, op->map, op, 0);
}

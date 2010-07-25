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
 * Handles code related to @ref DUPLICATOR "duplicators". */

#include <global.h>

/**
 * Trigger a duplicator.
 *
 * Will duplicate a specified object placed on top of it.
 *
 * - connected: What will trigger it.
 * - level: Multiplier. 0 to destroy.
 * - slaying: The object to look for and duplicate.
 * @param op Duplicator. */
void move_duplicator(object *op)
{
	object *tmp;

	if (!op->slaying)
	{
		LOG(llevInfo, "Duplicator with no slaying! %d %d %s\n", op->x, op->y, op->map ? op->map->path : "<no map>");
		return;
	}

	if (!op->above)
	{
		return;
	}

	for (tmp = op->above; tmp; tmp = tmp->above)
	{
		if (tmp->arch->name == op->slaying)
		{
			if (op->level <= 0)
			{
				destruct_ob(tmp);
			}
			else
			{
				uint64 new_nrof = (uint64) tmp->nrof * op->level;

				if (new_nrof >= 1UL << 31)
				{
					new_nrof = 1UL << 31;
				}

				tmp->nrof = new_nrof;
			}

			break;
		}
	}
}

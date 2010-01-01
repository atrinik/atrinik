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
 * Handles code for @ref DETECTOR "detectors". */

#include <global.h>

/**
 * Have the detector do its tick.
 * @param op The detector. */
void move_detector(object *op)
{
	object *tmp;
	int last = op->value, detected = 0;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL && !detected; tmp = tmp->above)
	{
		object *tmp2;

		if (op->stats.hp)
		{
			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
				if (op->slaying && !strcmp(op->slaying, tmp->name))
				{
					detected = 1;
				}

				if (tmp2->type == FORCE && tmp->slaying && tmp2->slaying == op->slaying)
				{
					detected = 1;
				}
			}
		}

		if (op->slaying && op->slaying == tmp->name)
		{
			detected = 1;
		}
		else if (tmp->type == SPECIAL_KEY && tmp->slaying == op->slaying)
		{
			detected = 1;
		}
	}

	/* the detector sets the button if detection is found */
	if (op->stats.sp == 1)
	{
		if (detected && last == 0)
		{
			op->value = 1;
			push_button(op);
		}

		if (!detected && last == 1)
		{
			op->value = 0;
			push_button(op);
		}
	}
	/* in this case, we unset buttons */
	else
	{
		if (detected && last == 1)
		{
			op->value = 0;
			push_button(op);
		}

		if (!detected && last == 0)
		{
			op->value = 1;
			push_button(op);
		}
	}
}

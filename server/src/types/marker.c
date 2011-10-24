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
 * Handles code for @ref MARKER "marker" type objects. */

#include <global.h>

/**
 * When moved, a marker will search for a player sitting above it, and
 * insert an invisible, weightless force into him with a specific code
 * as the slaying field. At that time, it writes the contents of its own
 * message field to the player.
 * @param op The marker object. */
void move_marker(object *op)
{
	object *tmp, *tmp2;

	for (tmp = GET_BOTTOM_MAP_OB(op); tmp != NULL; tmp = tmp->above)
	{
		/* we've got someone to MARK */
		if (tmp->type == PLAYER)
		{
			/* remove an old force with a slaying field == op->name */
			for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == FORCE && tmp2->slaying && tmp2->slaying == op->name)
				{
					break;
				}
			}

			if (tmp2)
			{
				remove_ob(tmp2);
				object_destroy(tmp2);
			}

			/* cycle through his inventory to look for the MARK we want to place */
			for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == FORCE && tmp2->slaying && tmp2->slaying == op->slaying)
				{
					break;
				}
			}

			/* if we didn't find our own MARK */
			if (tmp2 == NULL)
			{
				object *force = get_archetype("force");
				force->speed = 0;

				if (op->stats.food)
				{
					force->speed = 0.01f;
					force->speed_left = (float) -op->stats.food;
				}

				update_ob_speed(force);
				/* put in the lock code */
				FREE_AND_COPY_HASH(force->slaying, op->slaying);
				insert_ob_in_ob(force, tmp);

				if (op->msg)
				{
					draw_info(COLOR_NAVY, tmp, op->msg);
				}

				if (op->stats.hp > 0)
				{
					op->stats.hp--;

					if (op->stats.hp == 0)
					{
						/* marker expires -- granted mark number limit */
						destruct_ob(op);
						return;
					}
				}
			}
		}
	}
}

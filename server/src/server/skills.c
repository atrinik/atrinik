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
 * This file contains core skill handling. */

#include <global.h>
#include <book.h>
#include <skillist.h>

/**
 * Checks for traps on the spaces around the player or in certain
 * objects.
 * @param pl Player searching.
 * @param level Level of the find traps skill.
 * @return Experience gained for finding traps. */
sint64 find_traps(object *pl, int level)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int xt, yt, i, suc = 0;

	/* First we search all around us for runes and traps, which are
	 * all type RUNE */
	for (i = 0; i < 9; i++)
	{
		/* Check everything in the square for trapness */
		xt = pl->x + freearr_x[i];
		yt = pl->y + freearr_y[i];

		if (!(m = get_map_from_coord(pl->map, &xt, &yt)))
		{
			continue;
		}

		for (tmp = GET_MAP_OB(m, xt, yt); tmp != NULL; tmp = tmp->above)
		{
			/* And now we'd better do an inventory traversal of each
			 * of these objects' inventory */
			if (pl != tmp && (tmp->type == PLAYER || tmp->type == MONSTER))
			{
				continue;
			}

			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE)
				{
					if (trap_see(pl, tmp2, level))
					{
						trap_show(tmp2, tmp);

						if (!suc)
						{
							suc = 1;
						}
					}
					else
					{
						/* Give out a "we have found signs of traps"
						 * if the traps level is not 1.8 times higher. */
						if (tmp2->level <= (level * 1.8f))
						{
							suc = 2;
						}
					}
				}
			}

			if (tmp->type == RUNE)
			{
				if (trap_see(pl, tmp, level))
				{
					trap_show(tmp, tmp);

					if (!suc)
					{
						suc = 1;
					}
				}
				else
				{
					/* Give out a "we have found signs of traps"
					 * if the traps level is not 1.8 times higher. */
					if (tmp->level <= (level * 1.8f))
					{
						suc = 2;
					}
				}
			}
		}
	}

	if (!suc)
	{
		draw_info(COLOR_WHITE, pl, "You can't detect any trap here.");
	}
	else if (suc == 2)
	{
		draw_info(COLOR_WHITE, pl, "You detect trap signs!");
	}

	return 0;
}

/**
 * This skill will disarm any previously discovered trap.
 * @param op Player disarming.
 * @return 0. */
sint64 remove_trap(object *op)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int i, x, y;

	for (i = 0; i < 9; i++)
	{
		x = op->x + freearr_x[i];
		y = op->y + freearr_y[i];

		if (!(m = get_map_from_coord(op->map, &x, &y)))
		{
			continue;
		}

		/* Check everything in the square for trapness */
		for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
		{
			/* And now we'd better do an inventory traversal of each
			 * of these objects' inventory */
			for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
				if (tmp2->type == RUNE && tmp2->stats.Cha <= 1)
				{
					if (QUERY_FLAG(tmp2, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp2, FLAG_IS_INVISIBLE))
					{
						trap_show(tmp2, tmp);
					}

					trap_disarm(op, tmp2);
					return 0;
				}
			}

			if (tmp->type == RUNE && tmp->stats.Cha <= 1)
			{
				if (QUERY_FLAG(tmp, FLAG_SYS_OBJECT) || QUERY_FLAG(tmp, FLAG_IS_INVISIBLE))
				{
					trap_show(tmp, tmp);
				}

				trap_disarm(op, tmp);
				return 0;
			}
		}
	}

	draw_info(COLOR_WHITE, op, "There is no trap to remove nearby.");
	return 0;
}

/**
 * Requests an object to throw by tag reported by the client.
 *
 * We search for it in the inventory of the owner (you've got to be
 * carrying something in order to throw it).
 *
 * Also checks to see if the object is throwable (ie, not applied, cursed
 * worn, etc).
 * @param op Object to search in.
 * @param tag Tag of the object we're looking for.
 * @return The found object or NULL. */
object *find_throw_tag(object *op)
{
	object *tmp;

	tmp = CONTR(op)->ready_object[READY_OBJ_THROW];

	if (!tmp || !OBJECT_VALID(tmp, CONTR(op)->ready_object_tag[READY_OBJ_THROW]))
	{
		return NULL;
	}

	if (QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		/* We can't apply throwing stuff like darts, so this must be a
		 * weapon. Skip if not OR when it can't be thrown OR when it is
		 * startequip which can't be dropped. */
		if (tmp->type != WEAPON || !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			draw_info_format(COLOR_WHITE, op, "You can't throw %s.", query_base_name(tmp, NULL));
			return NULL;
		}
		else if (QUERY_FLAG(tmp, FLAG_STARTEQUIP))
		{
			draw_info(COLOR_WHITE, op, "You can't throw god-given item!");
			return NULL;
		}
		/* If cursed or damned, we can't unapply it - no throwing. */
		else if (QUERY_FLAG(tmp, FLAG_CURSED) || QUERY_FLAG(tmp, FLAG_DAMNED))
		{
			draw_info_format(COLOR_WHITE, op, "The %s sticks to your hand!", query_base_name(tmp, NULL));
			return NULL;
		}
		/* It's a throw hybrid weapon - unapply it. Then we will fire it
		 * after this function returns. */
		else
		{
			if (object_apply_item(tmp, op, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK)
			{
				LOG(llevBug, "find_throw_ob(): couldn't unapply throwing item %s from %s\n", query_name(tmp, NULL), query_name(op, NULL));
				return NULL;
			}
		}
	}
	else
	{
		/* Not weapon nor throwable - no throwing. */
		if ((tmp->type != WEAPON && tmp->type != POTION) && !QUERY_FLAG(tmp, FLAG_IS_THROWN))
		{
			draw_info_format(COLOR_WHITE, op, "You can't throw %s.", query_base_name(tmp, NULL));
			return NULL;
		}
		/* Special message for throw hybrid weapons. */
		else if (tmp->type == WEAPON)
		{
			draw_info_format(COLOR_WHITE, op, "You must apply the %s first.", query_base_name(tmp, NULL));
			return NULL;
		}
	}

	return tmp;
}

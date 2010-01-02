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
 * Handles operation of @ref IDENTIFY_ALTAR "identify altars". */

#include <global.h>
#include <sproto.h>

/**
 * Operate an identify altar.
 * @param money The money object.
 * @param altar The altar.
 * @param pl Player object that triggered the altar operation.
 * @return 1 if the money was destroyed, 0 otherwise. */
int apply_identify_altar(object *money, object *altar, object *pl)
{
	object *id, *marked;
	int success = 0;

	if (pl == NULL || pl->type != PLAYER)
	{
		return 0;
	}

	/* Check for MONEY type is a special hack - it prevents 'nothing needs
	 * identifying' from being printed out more than it needs to be. */
	if (!check_altar_sacrifice(altar, money) || money->type != MONEY)
	{
		return 0;
	}

	marked = find_marked_object(pl);

	/* if the player has a marked item, identify that if it needs to be
	 * identified.  IF it doesn't, then go through the player inventory. */
	if (marked && !QUERY_FLAG(marked, FLAG_IDENTIFIED) && need_identify(marked))
	{
		if (operate_altar(altar, &money))
		{
			identify(marked);
			new_draw_info_format(NDI_UNIQUE, pl, "You have %s.", long_desc(marked, pl));

			if (marked->msg)
			{
				new_draw_info(NDI_UNIQUE, pl, "The item has a story:");
				new_draw_info(NDI_UNIQUE, pl, marked->msg);
			}

			return money == NULL;
		}
	}

	for (id = pl->inv; id; id = id->below)
	{
		if (!QUERY_FLAG(id, FLAG_IDENTIFIED) && !IS_INVISIBLE(id, pl) && need_identify(id))
		{
			if (operate_altar(altar, &money))
			{
				identify(id);
				new_draw_info_format(NDI_UNIQUE, pl, "You have %s.", long_desc(id, pl));

				if (id->msg)
				{
					new_draw_info(NDI_UNIQUE, pl, "The item has a story:");
					new_draw_info(NDI_UNIQUE, pl, id->msg);
				}

				success = 1;

				/* If no more money, might as well quit now */
				if (money == NULL || !check_altar_sacrifice(altar, money))
				{
					break;
				}
			}
			else
			{
				LOG(llevBug, "BUG: apply_identify_altar(): Couldn't do sacrifice when we should have been able to.\n");
				break;
			}
		}
	}

	if (!success)
	{
		new_draw_info(NDI_UNIQUE, pl, "You have nothing that needs identifying.");
	}

	return money == NULL;
}

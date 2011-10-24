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
 * Handles code for operation of @ref CONVERTER "converters". */

#include <global.h>

#define CONV_FROM(xyz)  (xyz->slaying)
#define CONV_TO(xyz)    (xyz->other_arch)
#define CONV_NR(xyz)    ((unsigned long) xyz->stats.sp)
#define CONV_NEED(xyz)  ((unsigned long) xyz->stats.food)

/**
 * Transforms an item into another item.
 * @param item The object that triggered the converter - if it isn't of a
 * type accepted by the converter, nothing will happen
 * @param converter The object that is doing the conversion
 * @return 1 if the item got converted, 0 otherwise. */
int convert_item(object *item, object *converter)
{
	int nr = 0;
	object *tmp;

	/* We make some assumptions - we assume if it takes money as it type,
	 * it wants some amount.  We don't make change (ie, if something costs
	 * 3 gp and player drops a platinum, tough luck) */
	if (!strcmp(CONV_FROM(converter), "money"))
	{
		int cost;
		nr = (item->nrof * item->value) / CONV_NEED(converter);

		if (!nr)
		{
			return 0;
		}

		cost = nr * CONV_NEED(converter) / item->value;

		/* take into account rounding errors */
		if (nr * CONV_NEED(converter) % item->value)
		{
			cost++;
		}

		decrease_ob_nr(item, cost);
	}
	else
	{
		if (item->type == PLAYER || CONV_FROM(converter) != item->arch->name || (CONV_NEED(converter) && CONV_NEED(converter) > item->nrof))
		{
			return 0;
		}

		if (CONV_NEED(converter))
		{
			nr = item->nrof / CONV_NEED(converter);
			decrease_ob_nr(item, nr * CONV_NEED(converter));
		}
		else
		{
			remove_ob(item);
			object_destroy(item);
		}
	}

	item = arch_to_object(converter->other_arch);

	if (CONV_NR(converter))
	{
		item->nrof = CONV_NR(converter);
	}

	if (nr)
	{
		item->nrof *= nr;
	}

	for (tmp = GET_MAP_OB(converter->map, converter->x, converter->y); tmp != NULL; tmp = tmp->above)
	{
		if (tmp->type == SHOP_FLOOR)
		{
			break;
		}
	}

	if (tmp != NULL)
	{
		SET_FLAG(item, FLAG_UNPAID);
	}

	item->x = converter->x;
	item->y = converter->y;
	insert_ob_in_map(item, converter->map, converter, 0);

	return 1;
}

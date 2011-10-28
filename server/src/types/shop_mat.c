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
 * Handles code related to @ref SHOP_MAT "shop mats". */

#include <global.h>

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator)
{
	(void) originator;

	if (victim->type == PLAYER && !get_payment(victim, victim->inv))
	{
		int i;

		i = find_free_spot(victim->arch, NULL, victim->map, victim->x, victim->y, 1, SIZEOFFREE1 + 1);

		if (i != -1)
		{
			remove_ob(victim);
			victim->x += freearr_x[i];
			victim->y += freearr_y[i];
			insert_ob_in_map(victim, victim->map, op, 0);
		}

		return OBJECT_METHOD_OK;
	}

	if (op->msg)
	{
		draw_info(COLOR_WHITE, victim, op->msg);
	}
	else
	{
		int sub_layer;
		object *tmp;

		for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
		{
			tmp = GET_MAP_OB_LAYER(victim->map, victim->x, victim->y, LAYER_FLOOR, sub_layer);

			if (tmp && tmp->type == SHOP_FLOOR)
			{
				draw_info(COLOR_WHITE, victim, "Thank you for visiting our shop.");
				break;
			}
		}
	}

	teleport(op, SHOP_MAT, victim);

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the shop mat type object methods. */
void object_type_init_shop_mat(void)
{
	object_type_methods[SHOP_MAT].move_on_func = move_on_func;
}

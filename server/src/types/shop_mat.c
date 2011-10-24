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

/**
 * Apply/trigger a shop map.
 * @param shop_mat The shop map object.
 * @param op Object that triggered this.
 * @return Returns 1 if 'op' was destroyed, 0 otherwise. */
int apply_shop_mat(object *shop_mat, object *op)
{
	int rv = 0;

	/* prevent loops */
	SET_FLAG(op, FLAG_NO_APPLY);

	if (op->type != PLAYER)
	{
		if (QUERY_FLAG(op, FLAG_UNPAID))
		{
			/* Somebody dropped an unpaid item, just move to an adjacent place. */
			int i = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, 9);

			if (i != -1)
			{
				rv = transfer_ob(op, op->x + freearr_x[i], op->y + freearr_y[i], 0, shop_mat, NULL);
			}
		}

		rv = teleport(shop_mat, SHOP_MAT, op);
	}
	/* Only used for players. */
	else if (get_payment(op, op->inv))
	{
		object *tmp;

		rv = teleport(shop_mat, SHOP_MAT, op);

		if (shop_mat->msg)
		{
			draw_info(COLOR_WHITE, op, shop_mat->msg);
		}
		/* This check below is a bit simplistic - generally it should be correct,
		 * but there is never a guarantee that the bottom space on the map is
		 * actually the shop floor. */
		else if (!rv && (tmp = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0)) != NULL && tmp->type != SHOP_FLOOR)
		{
			draw_info(COLOR_WHITE, op, "Thank you for visiting our shop.");
		}
	}
	/* If we get here, a player tried to leave a shop but was not able
	 * to afford the items he has. We try to move the player so that
	 * they are not on the mat anymore */
	else
	{
		int i = find_free_spot(op->arch, NULL, op->map, op->x, op->y, 1, 9);

		if (i == -1)
		{
			LOG(llevBug, "Internal shop-mat problem (map:%s object:%s pos: %d,%d).\n", op->map->name, op->name, op->x, op->y);
		}
		else
		{
			remove_ob(op);
			op->x += freearr_x[i];
			op->y += freearr_y[i];
			rv = (insert_ob_in_map(op, op->map, shop_mat, 0) == NULL);
		}
	}

	CLEAR_FLAG(op, FLAG_NO_APPLY);

	return rv;
}

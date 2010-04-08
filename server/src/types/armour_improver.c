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
 * Handles code used for @ref ARMOUR_IMPROVER "armour improvers".
 * @todo Test how the code works and perhaps make use of it for
 * Atrinik. */

#include <global.h>

static void improve_armour(object *op, object *improver, object *armour);

/**
 * Apply an armour improvement scroll
 * @param op Player improving the armor.
 * @param tmp Improvement scroll. */
void apply_armour_improver(object *op, object *tmp)
{
	object *armor;

	if (blocks_magic(op->map, op->x, op->y))
	{
		new_draw_info(NDI_UNIQUE, op, "Something blocks the magic of the scroll.");
		return;
	}

	armor = find_marked_object(op);
	if (!armor)
	{
		new_draw_info(NDI_UNIQUE, op, "You need to mark an armor object.");
		return;
	}

	if (armor->type != ARMOUR && armor->type != CLOAK && armor->type != BOOTS && armor->type != GLOVES && armor->type != BRACERS && armor->type != SHIELD && armor->type != HELMET)
	{
		new_draw_info(NDI_UNIQUE, op, "Your marked item is not armour!\n");
		return;
	}

	new_draw_info(NDI_UNIQUE, op, "Applying armour enchantment.");
	improve_armour(op, tmp, armor);
}

/**
 * Actually improve an armour.
 * @param op Player object improving the armour.
 * @param improver Armour improvement scroll.
 * @param armour The armour to be improved. */
static void improve_armour(object *op, object *improver, object *armour)
{
	int new_armour = armour->resist[ATNR_PHYSICAL] + armour->resist[ATNR_PHYSICAL] / 25 + op->level / 20 + 1;

	if (new_armour > 90)
	{
		new_armour = 90;
	}

	if (armour->magic >= (op->level / 10 + 1) || new_armour > op->level)
	{
		new_draw_info(NDI_UNIQUE, op, "You are not yet powerful enough to improve this armour.");
		return;
	}

	if (new_armour > armour->resist[ATNR_PHYSICAL])
	{
		armour->resist[ATNR_PHYSICAL] = new_armour;
		armour->weight += (unsigned long) ((double) armour->weight * (double) 0.05);
	}
	else
	{
		new_draw_info(NDI_UNIQUE, op, "The armour value of this equipment cannot be further improved.");
	}

	armour->magic++;

	if (op->type == PLAYER)
	{
		esrv_send_item(op, armour);

		if (QUERY_FLAG(armour, FLAG_APPLIED))
		{
			fix_player(op);
		}
	}

	decrease_ob(improver);
}

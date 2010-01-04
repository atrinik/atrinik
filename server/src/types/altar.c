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
 * Handles code related to @ref ALTAR "altars". */

#include <global.h>
#include <sproto.h>

#define ARCH_SACRIFICE(xyz) ((xyz)->slaying)
#define NROF_SACRIFICE(xyz) ((xyz)->stats.food)

/**
 * Operate an altar.
 * @param altar The altar object.
 * @param sacrifice Sacrifice object for the altar.
 * @param originator Originator object who put the sacrifice on the
 * altar.
 * @return 1 if the sacrifice was accepted, 0 otherwise. */
int apply_altar(object *altar, object *sacrifice, object *originator)
{
	/* Only players can make sacrifices on spell casting altars. */
	if (altar->stats.sp != -1 && (!originator || originator->type != PLAYER))
	{
		return 0;
	}

	if (operate_altar(altar, &sacrifice))
	{
		/* Simple check. with an altar. We call it a Potion - altars are
		 * stationary - it is up to map designers to use them
		 * properly. */
		if (altar->stats.sp != -1)
		{
			new_draw_info_format(NDI_WHITE, originator, "The altar casts %s.", spells[altar->stats.sp].name);
			cast_spell(originator, altar, altar->last_sp, altar->stats.sp, 0, spellPotion, NULL);
			/* If it is connected, push the button. Fixes some problems
			 * with old maps. */
			push_button(altar);
		}
		else
		{
			/* Works only once */
			altar->value = 1;
			push_button(altar);
		}

		return sacrifice == NULL;
	}

	return 0;
}

/**
 * Check if the sacrifice meets the needs of the altar.
 *
 * Identify altars won't grab money unnecessarily - we can see
 * if there is sufficient money, see if something needs to be
 * identified, and then remove money if needed.
 *
 * Linked objects (ie, objects that are connected) can not be
 * sacrificed. This fixes a bug of trying to put multiple altars/related
 * objects on the same space that take the same sacrifice.
 * @param altar The altar object
 * @param sacrifice The sacrifice
 * @return 1 if it meets the needs, 0 otherwise */
int check_altar_sacrifice(object *altar, object *sacrifice)
{
	if (!IS_LIVE(sacrifice) && !QUERY_FLAG(sacrifice, FLAG_IS_LINKED))
	{
		if ((ARCH_SACRIFICE(altar) == sacrifice->arch->name || ARCH_SACRIFICE(altar) == sacrifice->name || ARCH_SACRIFICE(altar) == sacrifice->slaying) && NROF_SACRIFICE(altar) <= (sint16)(sacrifice->nrof ? sacrifice->nrof : 1))
		{
			return 1;
		}

		if (strcmp(ARCH_SACRIFICE(altar), "money") == 0 && sacrifice->type == MONEY && sacrifice->nrof * sacrifice->value >= (uint32) NROF_SACRIFICE(altar))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Checks if sacrifice was accepted and removes sacrificed objects.
 * Might be better to call check_altar_sacrifice (above) than
 * depend on the return value, since operate_altar will remove the
 * sacrifice also.
 *
 * If this function returns 1, '*sacrifice' is modified to point to the
 * remaining sacrifice, or is set to NULL if the sacrifice was used up.
 * @param altar The altar object
 * @param sacrifice The object to be sacrificed
 * @return 1 if sacrifice was succeed, 0 otherwise */
int operate_altar(object *altar, object **sacrifice)
{
	if (!altar->map)
	{
		LOG(llevBug, "BUG: operate_altar(): altar has no map\n");
		return 0;
	}

	if (!altar->slaying || altar->value)
	{
		return 0;
	}

	if (!check_altar_sacrifice(altar, *sacrifice))
	{
		return 0;
	}

	/* check_altar_sacrifice should have already verified that enough money
	 * has been dropped. */
	if (!strcmp(ARCH_SACRIFICE(altar), "money"))
	{
		int number = NROF_SACRIFICE(altar) / (*sacrifice)->value;

		/* Round up any sacrifices.  Altars don't make change either */
		if (NROF_SACRIFICE(altar) % (*sacrifice)->value)
		{
			number++;
		}

		*sacrifice = decrease_ob_nr(*sacrifice, number);
	}
	else
	{
		*sacrifice = decrease_ob_nr(*sacrifice, NROF_SACRIFICE(altar));
	}

	if (altar->msg)
	{
		new_info_map(NDI_WHITE, altar->map, altar->x, altar->y, MAP_INFO_NORMAL, altar->msg);
	}

	return 1;
}

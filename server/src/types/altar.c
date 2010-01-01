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
			new_draw_info_format(NDI_WHITE, 0, originator, "The altar casts %s.", spells[altar->stats.sp].name);
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

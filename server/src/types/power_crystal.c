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
 * Handles code related to @ref POWER_CRYSTAL "power crystals". */

#include <global.h>

/**
 * This function handles the application of power crystals.
 *
 * Power crystals, when applied, either suck mana from the applier, if
 * he's at full spellpoints, or give him mana, if it's got spellpoints
 * stored.
 * @param op Who is applying the crystal.
 * @param crystal The crystal. */
void apply_power_crystal(object *op, object *crystal)
{
	int available_power = op->stats.sp - op->stats.maxsp;
	int power_space = crystal->stats.maxsp - crystal->stats.sp;
	int power_grab = 0;

	if (available_power >= 0 && power_space > 0)
	{
		power_grab = (int) MIN((float) power_space, ((float) 0.5 * (float) op->stats.sp));
	}

	if (available_power < 0 && crystal->stats.sp > 0)
	{
		power_grab = -MIN(-available_power, crystal->stats.sp);
	}

	op->stats.sp -= power_grab;
	crystal->stats.sp += power_grab;
	crystal->speed = (float) crystal->stats.sp / (float) crystal->stats.maxsp;
	update_ob_speed(crystal);

	if (op->type == PLAYER)
	{
		esrv_update_item(UPD_ANIMSPEED, op, crystal);
	}
}

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Handles code related to @ref POWER_CRYSTAL "power crystals".
 *
 * Power crystals, when applied, either suck mana from the applier, if
 * he's at full spellpoints, or give him mana, if it's got spellpoints
 * stored.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	int power_available, power_space, power_grab;

	(void) aflags;

	power_available = applier->stats.sp - applier->stats.maxsp;
	power_space = op->stats.maxsp - op->stats.sp;
	power_grab = 0;

	if (power_available >= 0 && power_space > 0)
	{
		power_grab = MIN(power_space, applier->stats.sp / 2);
	}

	if (power_available < 0 && op->stats.sp > 0)
	{
		power_grab = -MIN(-power_available, op->stats.sp);
	}

	applier->stats.sp -= power_grab;
	op->stats.sp += power_grab;

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the power crystal type object methods. */
void object_type_init_power_crystal(void)
{
	object_type_methods[POWER_CRYSTAL].apply_func = apply_func;
}

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
 * Handles code for @ref DETECTOR "detectors".
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Check whether detector matches the specified object.
 * @param op Detector.
 * @param tmp Object to check.
 * @return 1 if the object matches, 0 otherwise. */
static int detector_matches_obj(object *op, object *tmp)
{
	/* Check type. */
	if (op->stats.hp && tmp->type != op->stats.hp)
	{
		return 0;
	}

	/* Check name. */
	if (op->slaying && tmp->name != op->slaying)
	{
		return 0;
	}

	/* Check archname. */
	if (op->race && tmp->arch->name != op->race)
	{
		return 0;
	}

	return 1;
}

/** @copydoc object_methods::move_on_func */
static int move_on_func(object *op, object *victim, object *originator)
{
	int matches;

	(void) originator;

	matches = detector_matches_obj(op, victim);
	connection_trigger(op, matches);

	if (op->last_heal)
	{
		decrease_ob(victim);
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the detector type object methods. */
void object_type_init_detector(void)
{
	object_type_methods[DETECTOR].move_on_func = move_on_func;
}

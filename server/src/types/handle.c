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
 * Handles code for @ref HANDLE "handles".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	(void) aflags;

	/* Toggle the state. */
	op->value = !op->value;
	SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->value);
	update_object(op, UP_OBJ_FACE);

	/* Inform the applier. */
	draw_info_format(COLOR_WHITE, applier, "You turn the %s.", op->name);
	play_sound_map(applier->map, CMD_SOUND_EFFECT, "pull.ogg", applier->x, applier->y, 0, 0);

	connection_trigger(op, op->value);

	return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
	(void) cause;

	op->value = op->stats.maxsp ? !state : state;
	SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->value);
	update_object(op, UP_OBJ_FACE);

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the handle type object methods. */
void object_type_init_handle(void)
{
	object_type_methods[HANDLE].apply_func = apply_func;
	object_type_methods[HANDLE].trigger_func = trigger_func;
}

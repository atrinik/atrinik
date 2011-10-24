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

#ifndef OBJECT_METHODS_H
#define OBJECT_METHODS_H

#define OBJECT_METHOD_OK 0
#define OBJECT_METHOD_UNHANDLED 1
#define OBJECT_METHOD_ERROR 2

typedef struct object_methods
{
	/**
	 * Applies an object.
	 * @param op The object to apply.
	 * @param applier The object that executes the apply action.
	 * @param aflags Special (always apply/unapply) flags. */
	int (*apply_func)(object *op, object *applier, int aflags);

	/**
	 * Processes an object, giving it the opportunity to move or react.
	 * @param op The object to process. */
	void (*process_func)(object *op);

	/**
	 * Returns the description of an object, as seen by the given observer.
	 * @param op The object to describe.
	 * @param observer The object to which the description is made.
	 * @param buf Buffer that will contain the description.
	 * @param size Size of 'buf'. */
	void (*describe_func)(object *, object *, char *buf, size_t size);

	/**
	 * Makes an object move on top of another one.
	 * @param op The object over which to move.
	 * @param victim The object moving over op.
	 * @param originator The object that is the cause of the move. */
	int (*move_on_func)(object *op, object *victim, object *originator);

	/**
	 * An object is triggered by another one.
	 * @param op The object being triggered.
	 * @param cause The object that is the cause of the trigger.
	 * @param state Trigger state. */
	int (*trigger_func)(object *op, object *cause, int state);

	/**
	 * Fallback method. */
	struct object_methods *fallback;
} object_methods;

#endif

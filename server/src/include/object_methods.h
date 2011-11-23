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

/**
 * @defgroup OBJECT_METHOD_xxx Object method return values
 * Object method return values.
 *@{*/
/** The object was not handled. */
#define OBJECT_METHOD_UNHANDLED 0
/** Successfully handled. */
#define OBJECT_METHOD_OK 1
/** Error handling the object. */
#define OBJECT_METHOD_ERROR 2
/*@}*/

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
	 * Triggered when an object moves moves off a square and when object
	 * moves onto a square.
	 * @param op The object that wants to catch this event.
	 * @param victim The object moving.
	 * @param originator The object that is the cause of the move.
	 * @param state 1 if the object is moving onto a square, 0 if moving
	 * off a square. */
	int (*move_on_func)(object *op, object *victim, object *originator, int state);

	/**
	 * An object is triggered by another one.
	 * @param op The object being triggered.
	 * @param cause The object that is the cause of the trigger.
	 * @param state Trigger state. */
	int (*trigger_func)(object *op, object *cause, int state);

	/**
	 * An object is triggered by a button.
	 * @param op The object being triggered.
	 * @param cause The object that is the cause of the trigger; the
	 * button.
	 * @param state Trigger state. */
	int (*trigger_button_func)(object *op, object *cause, int state);

	/**
	 * Called when an object is removed from map.
	 * @param op The object being removed. */
	void (*remove_map_func)(object *op);

	/**
	 * Called when an object is removed from inventory.
	 * @param op The object being removed. */
	void (*remove_inv_func)(object *op);

	/**
	 * Function to handle firing a projectile, eg, an arrow being fired
	 * from a bow, or a potion being thrown.
	 * @param op What is being fired.
	 * @param shooter Who is doing the firing.
	 * @param dir Direction to fire into.
	 * @return The fired object on success, NULL on failure. */
	object *(*projectile_fire_func)(object *op, object *shooter, int dir);

	/**
	 * Function to handle a fired object moving, eg, arrow moving to the
	 * next square along its path.
	 * @param op The fired object.
	 * @return The fired object, NULL if it was destroyed for some
	 * reason. */
	object *(*projectile_move_func)(object *op);

	/**
	 * Called when a fired object finds an alive object on the square it
	 * just moved to.
	 * @param op The fired object.
	 * @param victim The found alive object. Note that this just means
	 * that the object is on the @ref LAYER_LIVING layer, which may or
	 * may not imply that the object is actually alive.
	 * @retval OBJECT_METHOD_OK Successfully processed and should stop
	 * the fired arch.
	 * @retval OBJECT_METHOD_UNHANDLED Did not handle the event, should
	 * continue trying to look for another alive object.
	 * @retval OBJECT_METHOD_ERROR 'op' was destroyed. */
	int (*projectile_hit_func)(object *op, object *victim);

	/**
	 * Called to stop a fired object.
	 * @param op The fired object.
	 * @param reason Reason for stopping, one of @ref OBJECT_PROJECTILE_xxx.
	 * @return The fired object if it still exists, NULL otherwise. */
	object *(*projectile_stop_func)(object *op, int reason);

	/**
	 * Used to fire a ranged weapon, eg, a bow firing arrows, throwing
	 * weapons/potions, firing wands/rods, etc.
	 * @param op The weapon being fired (bow, wand, throwing object).
	 * @param shooter Who is doing the firing.
	 * @dir dir Direction to fire into.
	 * @return One of @ref OBJECT_METHOD_xxx. */
	int (*ranged_fire_func)(object *op, object *shooter, int dir);

	/**
	 * Fallback method. */
	struct object_methods *fallback;
} object_methods;

#define OBJECT_PROJECTILE_STOP_EOL 1
#define OBJECT_PROJECTILE_STOP_HIT 2
#define OBJECT_PROJECTILE_STOP_WALL 3

#endif

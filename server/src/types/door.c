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
 * @ref DOOR "Door" related code.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Open the specified door.
 * @param ob The door to open.
 * @param opener Who is opening the door.
 * @param nearby Whether this door was opened by opening a nearby door. */
static void door_open(object *ob, object *opener, uint8 nearby)
{
	object *tmp;

	/* Already open, nothing to do. */
	if (ob->last_eat)
	{
		return;
	}

	object_remove(ob, 0);

	/* Spring any traps in the door's inventory. */
	for (tmp = ob->inv; tmp; tmp = tmp->below)
	{
		if (tmp->type == RUNE && tmp->level)
		{
			rune_spring(tmp, opener);
		}
	}

	/* Mark this door as opened. */
	ob->last_eat = 1;
	/* Put it on the active list, so it will close automatically. */
	ob->speed = 0.1f;
	update_ob_speed(ob);
	ob->speed_left = -0.2f;
	ob->state = 1;
	/* Initialize counter that controls how long to allow the door to
	 * stay open. */
	ob->last_sp = ob->stats.sp;

	CLEAR_FLAG(ob, FLAG_BLOCKSVIEW);
	CLEAR_FLAG(ob, FLAG_DOOR_CLOSED);

	/* Update animation state. */
	if (QUERY_FLAG(ob, FLAG_IS_TURNABLE) || QUERY_FLAG(ob, FLAG_ANIMATE))
	{
		SET_ANIMATION(ob, (NUM_ANIMATIONS(ob) / NUM_FACINGS(ob)) * ob->direction + ob->state);
	}

	if (ob->sub_type == ST1_DOOR_NORMAL && !nearby)
	{
		play_sound_map(ob->map, CMD_SOUND_EFFECT, "door.ogg", ob->x, ob->y, 0, 0);
	}

	insert_ob_in_map(ob, ob->map, ob, 0);
}

/**
 * Open a door, including all nearby doors.
 * @param ob Door object to open.
 * @param opener Object opening the door. */
static void doors_open(object *ob, object *opener)
{
	int i, sub_layer, x, y;
	mapstruct *m;
	object *tmp;

	door_open(ob, opener, 0);

	/* Try to open nearby doors. */
	for (i = 1; i < 9; i += 2)
	{
		x = ob->x + freearr_x[i];
		y = ob->y + freearr_y[i];

		if (!(m = get_map_from_coord(ob->map, &x, &y)))
		{
			continue;
		}

		/* If there's no closed door, no need to check the objects on
		 * this square. */
		if (!(GET_MAP_FLAGS(m, x, y) & P_DOOR_CLOSED))
		{
			continue;
		}

		for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
		{
			for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_WALL, sub_layer); tmp && tmp->layer == LAYER_WALL && tmp->sub_layer == sub_layer; tmp = tmp->above)
			{
				if (tmp->type == DOOR && tmp->slaying == ob->slaying)
				{
					door_open(tmp, opener, 1);
				}
			}
		}
	}
}

/**
 * Open a door (or check whether it can be opened).
 * @param op Object which will open the door.
 * @param m Map where the door is.
 * @param x X position of the door.
 * @param y Y position of the door.
 * @param test If 1, only check whether the door can be opened, but do
 * not actually open the door.
 * @return 1 if door was opened (or can be), 0 if not and is not possible
 * to open. */
int door_try_open(object *op, mapstruct *m, int x, int y, int test)
{
	object *tmp, *key;
	int sub_layer;

	/* Make sure a monster/NPC can actually open doors */
	if (op->type == MONSTER && !(op->behavior & BEHAVIOR_OPEN_DOORS))
	{
		return 0;
	}

	for (sub_layer = 0; sub_layer < NUM_SUB_LAYERS; sub_layer++)
	{
		/* Look for objects on layer 5. */
		for (tmp = GET_MAP_OB_LAYER(m, x, y, LAYER_WALL, sub_layer); tmp && tmp->layer == LAYER_WALL && tmp->sub_layer == sub_layer; tmp = tmp->above)
		{
			if (tmp->type != DOOR)
			{
				continue;
			}

			/* Door needs a key? */
			if (tmp->slaying)
			{
				key = find_key(op, tmp);

				if (!key)
				{
					if (!test)
					{
						draw_info(COLOR_NAVY, op, tmp->msg);
					}

					return 0;
				}
				else if (!test)
				{
					if (key->type == KEY)
					{
						draw_info_format(COLOR_WHITE, op, "You open the %s with the %s.", tmp->name, query_short_name(key, NULL));
					}
					else if (key->type == FORCE)
					{
						draw_info_format(COLOR_WHITE, op, "The %s is opened for you.", tmp->name);
					}
				}
			}

			/* If we are here, the door can be opened. */
			if (!test)
			{
				doors_open(tmp, op);
			}

			return 1;
		}
	}

	return 0;
}

/**
 * Search object for the needed key to open a door/container.
 * @param op Object to search in.
 * @param door The object to find the key for.
 * @return The key pointer if found, NULL otherwise. */
object *find_key(object *op, object *door)
{
	object *tmp, *key;

	/* First, let's try to find a key in the top level inventory. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if ((tmp->type == KEY || tmp->type == FORCE) && tmp->slaying == door->slaying)
		{
			return tmp;
		}

		/* Go through containers. */
		if (tmp->type == CONTAINER && tmp->inv)
		{
			key = find_key(tmp, door);

			if (key)
			{
				return key;
			}
		}
	}

	return NULL;
}

/** @copydoc object_methods::process_func */
static void process_func(object *op)
{
	/* Check to see if the door should still remain open. */
	if (op->last_sp-- > 0)
	{
		return;
	}

	/* If there's something blocking the door from closing, reset the
	 * counter. */
	if (blocked(NULL, op->map, op->x, op->y, TERRAIN_ALL) & (P_NO_PASS | P_IS_MONSTER | P_IS_PLAYER))
	{
		op->last_sp = op->stats.sp;
		return;
	}

	object_remove(op, 0);

	/* The door is no longer open. */
	op->last_eat = 0;

	/* Remove from active list. */
	op->speed = 0.0f;
	op->speed_left = 0.0f;
	update_ob_speed(op);

	op->state = 0;

	if (QUERY_FLAG(&op->arch->clone, FLAG_BLOCKSVIEW))
	{
		SET_FLAG(op, FLAG_BLOCKSVIEW);
	}

	SET_FLAG(op, FLAG_DOOR_CLOSED);

	/* Update animation state. */
	if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE))
	{
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
	}

	if (op->sub_type == ST1_DOOR_NORMAL)
	{
		play_sound_map(op->map, CMD_SOUND_EFFECT, "door_close.ogg", op->x, op->y, 0, 0);
	}

	insert_ob_in_map(op, op->map, op, 0);
}

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	(void) op;
	(void) applier;
	(void) aflags;

	return OBJECT_METHOD_UNHANDLED;
}

/**
 * Initialize the door type object methods. */
void object_type_init_door(void)
{
	object_type_methods[DOOR].process_func = process_func;
	object_type_methods[DOOR].apply_func = apply_func;
}

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
 * @ref DOOR "Door" related code. */

#include <global.h>

/**
 * Open a door (or check whether it can be opened).
 * @param op Object which will open the door.
 * @param m Map where the door is.
 * @param x X position of the door.
 * @param y Y position of the door.
 * @param mode
 * - <b>0</b>: Check but don't open the door.
 * - <b>1</b>: Check and open the door if possible.
 * @return 1 if door was opened (or can be), 0 if not and is not possible to open. */
int open_door(object *op, mapstruct *m, int x, int y, int mode)
{
	object *tmp, *key = NULL;

	/* Make sure a monster/NPC can actually open doors */
	if (op->type == MONSTER && !(op->behavior & BEHAVIOR_OPEN_DOORS))
	{
		return 0;
	}

	/* Look for objects on layer 5. */
	for (tmp = GET_MAP_OB_LAYER(m, x, y, 4); tmp && tmp->layer == 5; tmp = tmp->above)
	{
		if (tmp->type == DOOR)
		{
			/* Door needs a key? */
			if (tmp->slaying)
			{
				if (!(key = find_key(op, tmp)))
				{
					if (op->type == PLAYER && mode)
					{
						new_draw_info(NDI_UNIQUE | NDI_NAVY, op, tmp->msg);
					}

					return 0;
				}
			}

			/* If we are here, the door can be opened */
			if (mode)
			{
				open_locked_door(tmp, op);

				if (op->type == PLAYER && key)
				{
					if (key->type == KEY)
					{
						new_draw_info_format(NDI_UNIQUE, op, "You open the door with the %s.", query_short_name(key, NULL));
					}
					else if (key->type == FORCE)
					{
						new_draw_info(NDI_UNIQUE, op, "The door is opened for you.");
					}
				}
			}

			return 1;
		}
	}

	LOG(llevSystem, "BUG: open_door() - Door on wrong layer. Map: %s (%d, %d) (op: %s)\n", m->path, x, y, query_name(op, NULL));
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

/**
 * Open a locked door. This function checks to see if there are similar
 * locked doors nearby, if the door to open has FLAG_CURSED (cursed 1)
 * set. The nearby doors must also have FLAG_CURSED set.
 * @param op Door object to open.
 * @param opener Object opening the door. */
void open_locked_door(object *op, object *opener)
{
	object *tmp, *tmp2;

	/* If set, just exchange and delete old door */
	if (op->other_arch)
	{
		tmp = arch_to_object(op->other_arch);
		/* 0 = closed, 1 = opened */
		tmp->state = 0;
		tmp->x = op->x;
		tmp->y = op->y;
		tmp->map = op->map;
		tmp->level = op->level;
		tmp->direction = op->direction;

		if (QUERY_FLAG(tmp, FLAG_IS_TURNABLE) || QUERY_FLAG(tmp, FLAG_ANIMATE))
		{
			SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction + tmp->state);
		}

		insert_ob_in_map(tmp, op->map, op, 0);

		if (op->sub_type == ST1_DOOR_NORMAL)
		{
			play_sound_map(op->map, CMD_SOUND_EFFECT, "door.ogg", op->x, op->y, 0, 0);
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	}
	/* If set, we have opened a closed door - now handle autoclose */
	else if (!op->last_eat)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		for (tmp2 = op->inv; tmp2; tmp2 = tmp2->below)
		{
			if (tmp2 && tmp2->type == RUNE && tmp2->level)
			{
				spring_trap(tmp2, opener);
			}
		}

		/* Mark this door as "it's open" */
		op->last_eat = 1;
		/* Put it on active list, so it will close automatically */
		op->speed = 0.1f;
		update_ob_speed(op);
		op->speed_left= -0.2f;
		/* Change to "open door" faces */
		op->state = 1;
		/* Init "open" counter */
		op->last_sp = op->stats.sp;
		/* Save and clear blocksview and no_pass */
		QUERY_FLAG(op, FLAG_BLOCKSVIEW) ? (op->stats.grace = 1) : (op->stats.grace = 0);
		QUERY_FLAG(op, FLAG_DOOR_CLOSED) ? (op->last_grace = 1) : (op->last_grace = 0);
		CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
		CLEAR_FLAG(op, FLAG_DOOR_CLOSED);

		if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE))
		{
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		}

		if (op->sub_type == ST1_DOOR_NORMAL)
		{
			play_sound_map(op->map, CMD_SOUND_EFFECT, "door.ogg", op->x, op->y, 0, 0);
		}

		insert_ob_in_map(op, op->map, op, 0);
	}

	/* Door has FLAG_CURSED set? */
	if (QUERY_FLAG(op, FLAG_CURSED))
	{
		int i;

		/* Let's search for a locked door on nearby tiles */
		for (i = 1; i < 9; i += 2)
		{
			tmp = present(DOOR, op->map, op->x + freearr_x[i], op->y + freearr_y[i]);

			/* Found it, slaying matches, it has FLAG_CURSED and not opened? */
			if (tmp && tmp->slaying == op->slaying && QUERY_FLAG(tmp, FLAG_CURSED) && tmp->state == 0)
			{
				/* Open this door then! */
				open_locked_door(tmp, opener);
			}
		}
	}
}

/**
 * Close a locked door that has been opened.
 * @param op Door object to close. */
void close_locked_door(object *op)
{
	/* This is a bug - active speed but not marked as active */
	if (!op->last_eat)
	{
		LOG(llevBug, "BUG: Door has speed but is not marked as active. (%s - map: %s (%d, %d))\n", query_name(op, NULL), op->map ? op->map->name : "(no map name!)", op->x, op->y);
		op->last_eat = 0;
		return;
	}

	if (!op->map)
	{
		LOG(llevBug, "BUG: Door with speed but no map (%s - (%d, %d))\n", query_name(op, NULL), op->x, op->y);
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* Now check the door counter - if not <= 0 we're still open */
	if (op->last_sp-- > 0)
	{
		return;
	}

	/* Now we try to close the door. If the tile of the door is not blocked by
	 * a no_pass object or a player or a monster, then we close the door.
	 * If it is blocked - then restart a new "is open" phase. */
	if (blocked(NULL, op->map, op->x, op->y, TERRAIN_ALL) & (P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER))
	{
		/* Let it open one more round. Reinit "open" counter. */
		op->last_sp = op->stats.sp;
	}
	else
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		/* Mark this door as "it's closed" */
		op->last_eat = 0;

		/* Remove from active list */
		op->speed = 0.0f;
		op->speed_left = 0.0f;
		update_ob_speed(op);

		/* Change to "close door" faces */
		op->state = 0;
		op->stats.grace = 1 ? SET_FLAG(op, FLAG_BLOCKSVIEW) : CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
		op->last_grace = 1 ? SET_FLAG(op, FLAG_DOOR_CLOSED) : CLEAR_FLAG(op, FLAG_DOOR_CLOSED);
		op->stats.grace = 0;
		op->last_grace = 0;

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
}

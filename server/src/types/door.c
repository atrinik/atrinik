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
#include <sproto.h>

/**
 * Search object for the needed key to open a door.
 * @param op Object to search
 * @param door The door to find the key for
 * @return The key pointer if found, NULL otherwise */
object *find_key(object *op, object *door)
{
	object *tmp, *key;

	/* First, let's try to find a key in the top level inventory */
	for (tmp = op->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == SPECIAL_KEY && tmp->slaying == door->slaying)
		{
			return tmp;
		}

		/* We brute force us through every CONTAINER inventory. */
		if (tmp->type == CONTAINER && tmp->inv)
		{
			if ((key = find_key(tmp, door)) != NULL)
			{
				return key;
			}
		}
	}

	return NULL;
}

/**
 * This is out main open_door() function. It is used for doors
 * which will auto open/close and/or need a special key. It is
 * used from NPCs, monsters and players and uses the
 * remove_doorX() functions below.
 * @param op Object which will open the door.
 * @param m Map structure where the door is.
 * @param x X position of the door.
 * @param y Y position of the door.
 * @param mode
 * - <b>0</b>: Check but don't open the door.
 * - <b>1</b>: Check and open the door if possible.
 * @return 1 if door was opened, 0 if not and is not possible. */
int open_door(object *op, mapstruct *m, int x, int y, int mode)
{
	object *tmp, *key = NULL;

	/* Make sure a monster/NPC can actually open doors */
	if (op->type == MONSTER && !(op->will_apply & WILL_APPLY_DOOR))
	{
		return 0;
	}

	/* Ok, this trick will save us *some* search time - because we
	 * assume that a door is always on layer 5. But *careful* -
	 * if we assign in a map a different layer to a door this will
	 * fail badly! */
	for (tmp = GET_MAP_OB_LAYER(m, x, y, 4); tmp != NULL && tmp->layer == 5; tmp = tmp->above)
	{
		if (tmp->type == LOCKED_DOOR)
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

					/* We can't open it! */
					return 0;
				}
			}

			/* If we are here, the door can be opened 100% */
			if (mode)
			{
				/* Really opening the door? */
				open_locked_door(tmp, op);

				if (op->type == PLAYER && key)
				{
					new_draw_info_format(NDI_UNIQUE, op, "You open the door with the %s.", query_short_name(key, NULL));
				}
			}

			return 1;
		}
	}

	/* We should not be here... We have a misplaced door_closed flag
	 * or a door on a wrong layer... both is not good, so drop a bug msg. */
	LOG(llevSystem, "BUG: open_door() - door on wrong layer or misplaced P_DOOR_CLOSED flag - map: %s (%d, %d) (op: %s)\n", m->path, x, y, query_name(op, NULL));

	return 0;
}

/**
 * The following removes doors. The function checks to see if similar
 * doors are next to the one that is being removed, and if so, set it
 * so those will be removed shortly (in a cascade like fashion).
 *
 * Currently, Atrinik doesn't really make use of this function.
 * @param op The door object being removed. */
void remove_door(object *op)
{
	int i;
	object *tmp;

	for (i = 1; i < 9; i += 2)
	{
		if ((tmp = present(DOOR, op->map, op->x + freearr_x[i], op->y + freearr_y[i])) != NULL)
		{
			tmp->speed = 0.1f;
			update_ob_speed(tmp);
			tmp->speed_left= -0.2f;
		}
	}

	if (op->other_arch)
	{
		tmp = arch_to_object(op->other_arch);
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
	}

	if (op->sub_type1 == ST1_DOOR_NORMAL)
	{
		play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
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

	/* mow 2 ways to handle open doors.
	 * a.) if other_arch is set, we insert that object and remove the old door.
	 * b.) if not set, we toggle from close to open when needed. */

	/* if set, just exchange and delete old door */
	if (op->other_arch)
	{
		tmp = arch_to_object(op->other_arch);
		/* 0= closed, 1= opened */
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

		if (op->sub_type1 == ST1_DOOR_NORMAL)
		{
			play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	}
	/* if set, we are have opened a closed door - now handle autoclose */
	else if (!op->last_eat)
	{
		/* to trigger all the updates/changes on map and for player, we
		 * remove and reinsert it. a bit overhead but its secure and simple */
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		for (tmp2 = op->inv; tmp2; tmp2 = tmp2->below)
		{
			if (tmp2 && tmp2->type == RUNE && tmp2->level)
			{
				spring_trap(tmp2, opener);
			}
		}

		/* mark this door as "its open" */
		op->last_eat = 1;
		/* put it on active list, so it will close automatically */
		op->speed = 0.1f;
		update_ob_speed(op);
		op->speed_left= -0.2f;
		/* change to "open door" faces */
		op->state = 1;
		/* init "open" counter */
		op->last_sp = op->stats.sp;
		/* save and clear blocksview and no_pass */
		QUERY_FLAG(op, FLAG_BLOCKSVIEW) ? (op->stats.grace = 1) : (op->stats.grace = 0);
		QUERY_FLAG(op, FLAG_DOOR_CLOSED) ? (op->last_grace = 1) : (op->last_grace = 0);
		CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
		CLEAR_FLAG(op, FLAG_DOOR_CLOSED);

		if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE))
		{
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		}

		if (op->sub_type1 == ST1_DOOR_NORMAL)
		{
			play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);
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
			tmp = present(LOCKED_DOOR, op->map, op->x + freearr_x[i], op->y + freearr_y[i]);

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
 * @param op Door object to open. */
void close_locked_door(object *op)
{
	/* This is a bug - active speed but not marked as active */
	if (!op->last_eat)
	{
		LOG(llevBug, "BUG: door has speed but is not marked as active. (%s - map: %s (%d, %d))\n", query_name(op, NULL), op->map ? op->map->name : "(no map name!)", op->x, op->y);

		/* Not a real fix... */
		op->last_eat = 0;

		return;
	}

	if (!op->map)
	{
		LOG(llevBug, "BUG: door with speed but no map?! Killing object... done. (%s - (%d, %d))\n", query_name(op, NULL), op->x, op->y);
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		return;
	}

	/* Now check the door counter - if not <= 0 we're still open */
	if (op->last_sp-- > 0)
	{
		return;
	}

	/* Now we try to close the door. The rule is:
	 * If the tile of the door is not blocked by a no_pass object OR a player OR a mob -
	 * then close the door.
	 * If it is blocked - then restart a new "is open" phase. */

	/* Here we can use our blocked() function - we simply check the given flags */
	if (blocked(NULL, op->map, op->x, op->y, TERRAIN_ALL) & (P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER))
	{
		/* Let it open one more round. Reinit "open" counter. */
		op->last_sp = op->stats.sp;
	}
	/* Ok - now we close it */
	else
	{
		/* To trigger all the updates/changes on map and for player, we
		 * remove and reinsert it. A bit overhead but it's secure and simple */
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		/* Mark this door as "it's closed" */
		op->last_eat = 0;

		/* Remove from active list */
		op->speed = 0.0f;
		op->speed_left= 0.0f;
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

		if (op->sub_type1 == ST1_DOOR_NORMAL)
		{
			play_sound_map(op->map, op->x, op->y, SOUND_DOOR_CLOSE, SOUND_NORMAL);
		}

		insert_ob_in_map(op, op->map, op, 0);
	}
}

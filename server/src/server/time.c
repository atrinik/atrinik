/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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
 * Routines that are executed from objects based on their speed have been
 * collected in this file. */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

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
 * @param op Object which will open the door
 * @param m Map structure where the door is
 * @param x X position of the door
 * @param y Y position of the door
 * @param mode
 *  0: Check but don't open the door \n
 *  1: Check and open the door if possible
 * @return 1 if door was opened, 0 if not and is not possible */
int open_door(object *op, mapstruct *m, int x, int y, int mode)
{
	object *tmp, *key = NULL;

	/* Make sure a monster/NPC can actually open doors */
	if (op->type != PLAYER && !(op->will_apply & 8))
		return 0;

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
						new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, op, tmp->msg);

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
					new_draw_info_format(NDI_UNIQUE, NDI_BROWN, op, "You open the door with the %s.", query_short_name(key, NULL));
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
 * @param op The door object being removed */
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
			SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction + tmp->state);

		insert_ob_in_map(tmp, op->map, op, 0);
	}

  	if (op->sub_type1 == ST1_DOOR_NORMAL)
	  	play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);

  	remove_ob(op);
  	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

/**
 * Open a locked door. This function checks to see if there are similar
 * locked doors nearby, if the door to open has FLAG_CURSED (cursed 1)
 * set. The nearby doors must also have FLAG_CURSED set.
 * @param op Door object to open
 * @param opener Object opening the door */
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
			SET_ANIMATION(tmp, (NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction + tmp->state);

		insert_ob_in_map(tmp, op->map, op, 0);

		if (op->sub_type1 == ST1_DOOR_NORMAL)
		  	play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);

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
				spring_trap(tmp2, opener);
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
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);

		if (op->sub_type1 == ST1_DOOR_NORMAL)
			play_sound_map(op->map, op->x, op->y, SOUND_OPEN_DOOR, SOUND_NORMAL);

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
 * @param op Door object to open */
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
	 	 return;

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
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);

		if (op->sub_type1 == ST1_DOOR_NORMAL)
			play_sound_map(op->map, op->x, op->y, SOUND_DOOR_CLOSE, SOUND_NORMAL);

		insert_ob_in_map(op, op->map, op, 0);
	}
}

/**
 * Regenerate rod speed needed to fire the rod again.
 * @param rod The rod object to regenerate */
void regenerate_rod(object *rod)
{
	if (++rod->stats.food > rod->stats.hp / 10 || rod->type == HORN)
	{
		rod->stats.food = 0;

		if (rod->stats.hp < rod->stats.maxhp)
		{
			rod->stats.hp += 1 + rod->stats.maxhp / 10;

			if (rod->stats.hp > rod->stats.maxhp)
				rod->stats.hp = rod->stats.maxhp;

			fix_rod_speed(rod);
		}
	}
}

/**
 * Remove a force object from player, like potion effect.
 * @param op Force object to remove */
void remove_force(object *op)
{
	if (op->env == NULL)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);

		return;
	}

	CLEAR_FLAG(op, FLAG_APPLIED);
	change_abil(op->env, op);
	fix_player(op->env);
	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

void remove_blindness(object *op)
{
	if (--op->stats.food > 0)
		return;

	CLEAR_FLAG(op, FLAG_APPLIED);

	if (op->env != NULL)
	{
		change_abil(op->env, op);
		fix_player(op->env);
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

void remove_confusion(object *op)
{
	if (--op->stats.food > 0)
		return;

	if (op->env != NULL)
	{
		CLEAR_FLAG(op->env, FLAG_CONFUSED);
		new_draw_info(NDI_UNIQUE, 0, op->env, "You regain your senses.");
	}

	remove_ob(op);
	check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
}

void execute_wor(object *op)
{
	object *wor = op;
	while (op != NULL && op->type != PLAYER)
		op = op->env;

	if (op != NULL)
	{
		if (blocks_magic(op->map, op->x, op->y))
			new_draw_info(NDI_UNIQUE, 0, op, "You feel something fizzle inside you.");
		else
			enter_exit(op, wor);
	}

	remove_ob(wor);
	check_walk_off(wor, NULL, MOVE_APPLY_VANISHED);
}

void poison_more(object *op)
{
	if (op->env == NULL || !IS_LIVE(op->env) || op->env->stats.hp < 0)
	{
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (!op->stats.food)
	{
		/* need to remove the object before fix_player is called, else fix_player
		 * will not do anything. */
		if (op->env->type == PLAYER)
		{
			CLEAR_FLAG(op, FLAG_APPLIED);
			fix_player(op->env);
			new_draw_info(NDI_UNIQUE, 0, op->env, "You feel much better now.");
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	if (op->env->type == PLAYER)
	{
		op->env->stats.food--;
		new_draw_info(NDI_UNIQUE, 0, op->env, "You feel very sick...");
	}

	(void) hit_player(op->env, op->stats.dam, op, AT_INTERNAL);
}

/**
 * Move a gate up/down.
 *
 * 1 = going down, 0 = going up
 * @param op Gate object
 * @todo I have not included damage to mobs/player on reverse up going gates!
 * Look in the code! */
void move_gate(object *op)
{
	object *tmp;
	/* default update is only face */
	int update = UP_OBJ_FACE;

	if (op->stats.wc < 0 || (int) op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)))
	{
		dump_object(op);
		LOG(llevBug, "BUG: move_gate(): Gate animation was %d, max=%d\n:%s\n", op->stats.wc, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)), errmsg);
		op->stats.wc = 0;
	}

	/* We're going down (or reverse up) */
	if (op->value)
	{
		/* Reached bottom, let's stop */
		if (--op->stats.wc <= 0)
		{
			op->stats.wc = 0;

			if (op->arch->clone.speed)
			{
				op->value = 0;
			}
			else
			{
				op->speed = 0;
				update_ob_speed(op);
			}
		}

		if ((int) op->stats.wc < ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) / 2 + 1))
		{
			/* We do the QUERY_FLAG() here to check we must rebuild the tile flags or not,
			 * if we don't change the object settings here, just change the face but
			 * don't rebuild the flag tiles.
			 * If != 0, we have a reversed timed gate, which starts open */
			if (op->last_heal)
			{
				if (!QUERY_FLAG(op, FLAG_NO_PASS))
					update = UP_OBJ_FLAGFACE;

				/* The coast is clear, block the way */
				SET_FLAG(op, FLAG_NO_PASS);

				if (!op->arch->clone.stats.ac)
				{
					if (!QUERY_FLAG(op, FLAG_BLOCKSVIEW))
						update = UP_OBJ_FLAGFACE;

					SET_FLAG(op, FLAG_BLOCKSVIEW);
				}
			}
			else
			{
				if (QUERY_FLAG(op, FLAG_NO_PASS))
					update = UP_OBJ_FLAGFACE;

				CLEAR_FLAG(op, FLAG_NO_PASS);

				if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
					update = UP_OBJ_FLAGFACE;

				CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, update);

		return;
	}

	/* First, lets see if we are already at the top */
	if ((unsigned char) op->stats.wc == ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1))
	{
		/* Check to make sure that only non pickable and non rollable
		 * objects are above the gate. If so, we finish closing the gate,
		 * otherwise, we fall through to the code below which should lower
		 * the gate slightly. */
		for (tmp = op->above; tmp != NULL; tmp = tmp->above)
		{
			if (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || IS_LIVE(tmp))
				break;
		}

		if (tmp == NULL)
		{
			if (op->arch->clone.speed)
			{
				op->value = 1;
			}
			else
			{
				op->speed = 0;

				/* Reached top, let's stop */
				update_ob_speed(op);
			}

			return;
		}
	}

	/* The gate is going temporarily down */
	if (op->stats.food)
	{
		/* Gone all the way down? */
		if (--op->stats.wc <= 0)
		{
			/* Then let's try again */
			op->stats.food = 0;
			op->stats.wc = 0;
		}
	}
	/* The gate is still going up */
	else
	{
		op->stats.wc++;

		if ((int) op->stats.wc >= ((NUM_ANIMATIONS(op) / NUM_FACINGS(op))))
			op->stats.wc = (signed char) (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) - 1;

		/* If there is something on top of the gate, we try to roll it off.
		 * If a player/monster, we don't roll, we just hit them with damage */
		if ((int) op->stats.wc >= (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) / 2)
		{
			/* Halfway or further, check blocks
			 * First, get the top object on the square. */
			for (tmp = op->above; tmp != NULL && tmp->above != NULL; tmp = tmp->above);

			if (tmp != NULL)
			{
				if (IS_LIVE(tmp))
				{
					hit_player(tmp, 4, op, AT_PHYSICAL);

					if (tmp->type == PLAYER)
					{
						new_draw_info_format(NDI_UNIQUE, 0, tmp, "You are crushed by the %s!", op->name);
					}
				}

				/* If the object is not alive, and the object either can
				 * be picked up or the object rolls, move the object
				 * off the gate. */
				if (IS_LIVE(tmp) || (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL)))
				{
					/* If it has speed, it should move itself, otherwise: */
					int i = find_free_spot(tmp->arch, NULL, op->map, op->x, op->y, 1, 9);

					/* If there is a free spot, move the object someplace */
					if (i != -1)
					{
						remove_ob(tmp);
						check_walk_off(tmp, NULL, MOVE_APPLY_VANISHED);
						tmp->x += freearr_x[i], tmp->y += freearr_y[i];
						insert_ob_in_map(tmp, op->map, op, 0);
					}
				}
			}

			/* See if there is still anything blocking the gate */
			for (tmp = op->above; tmp != NULL; tmp = tmp->above)
			{
				if (!QUERY_FLAG(tmp, FLAG_NO_PICK) || QUERY_FLAG(tmp, FLAG_CAN_ROLL) || IS_LIVE(tmp))
				{
					break;
				}
			}

			/* If there is, start putting the gate down */
			if (tmp)
			{
				op->stats.food = 1;
			}
			else
			{
				/* If != 0, we have a reversed timed gate, which starts open */
				if (op->last_heal)
				{
					if (QUERY_FLAG(op, FLAG_NO_PASS))
						update = UP_OBJ_FLAGFACE;

					CLEAR_FLAG(op, FLAG_NO_PASS);

					if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
						update = UP_OBJ_FLAGFACE;

					CLEAR_FLAG(op, FLAG_BLOCKSVIEW);
				}
				else
				{
					if (!QUERY_FLAG(op, FLAG_NO_PASS))
						update = UP_OBJ_FLAGFACE;

					/* The coast is clear, block the way */
					SET_FLAG(op, FLAG_NO_PASS);

					if (!op->arch->clone.stats.ac)
					{
						if (!QUERY_FLAG(op, FLAG_BLOCKSVIEW))
							update = UP_OBJ_FLAGFACE;

						SET_FLAG(op, FLAG_BLOCKSVIEW);
					}
				}
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);

		/* Takes care about map tile and player los update! */
		update_object(op, update);
	}
}

/**
 * Move a timed gate.
 *
 * hp: How long gate is open/closed \n
 * maxhp: Initial value for hp \n
 * sp: 1 = open, 0 = close
 * @param op Timed gate object */
void move_timed_gate(object *op)
{
	int v = op->value;

	if (op->stats.sp)
	{
		move_gate(op);

		/* Change direction? */
		if (op->value != v)
			op->stats.sp = 0;

		return;
	}

	/* Keep gate down */
	if (--op->stats.hp <= 0)
	{
		move_gate(op);

		/* Ready? */
		if (op->value != v)
		{
			op->speed = 0;
			update_ob_speed(op);
		}
	}
}

/*  slaying:	name of the thing the detector is to look for
 *	 speed:	  frequency of 'glances'
 *	 connected:  connected value of detector
 *  sp:		 1 if detection sets buttons
 *			  -1 if detection unsets buttons */
void move_detector(object *op)
{
	object *tmp;
	int last = op->value;
	int detected;
	detected = 0;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL && !detected; tmp = tmp->above)
	{
		object *tmp2;
		if (op->stats.hp)
		{
		  	for (tmp2 = tmp->inv; tmp2; tmp2 = tmp2->below)
			{
			 	if (op->slaying && !strcmp(op->slaying, tmp->name))
					detected = 1;

			 	if (tmp2->type == FORCE && tmp2->slaying && !strcmp(tmp2->slaying, op->slaying))
					detected = 1;
		  	}
		}

		if (op->slaying && !strcmp(op->slaying, tmp->name))
		{
			detected = 1;
		}
		else if (tmp->type == SPECIAL_KEY && tmp->slaying == op->slaying)
			detected = 1;
	}

	/* the detector sets the button if detection is found */
	if (op->stats.sp == 1)
	{
		if (detected && last == 0)
		{
			op->value = 1;
			push_button(op);
		}

		if (!detected && last == 1)
		{
			op->value = 0;
			push_button(op);
		}
	}
	/* in this case, we unset buttons */
	else
	{
		if (detected && last == 1)
		{
			op->value = 0;
			push_button(op);
		}

		if (!detected && last == 0)
		{
			op->value = 1;
			push_button(op);
		}
	}
}

void animate_trigger(object *op)
{
	if ((unsigned char) ++op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
	{
		op->stats.wc = 0;
		check_trigger(op, NULL);
	}
	else
	{
		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
	}
}

/**
 * Close or open a pit. op->value is set when something connected to
 * the pit is triggered.
 * @param op The pit object */
void move_pit(object *op)
{
	object *next,*tmp;

	/* We're opening */
	if (op->value)
	{
		/* Opened, let's stop */
		if (--op->stats.wc <= 0)
		{
			op->stats.wc = 0;
			op->speed = 0;
			update_ob_speed(op);
			SET_FLAG(op, FLAG_WALK_ON);

			for (tmp = op->above; tmp != NULL; tmp = next)
			{
				next = tmp->above;
				move_apply(op, tmp, tmp, 0);
			}
		}

		op->state = (uint8) op->stats.wc;
		SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		update_object(op, UP_OBJ_FACE);
		return;
	}

	/* We're closing */
	CLEAR_FLAG(op, FLAG_WALK_ON);
	op->stats.wc++;

	if ((int) op->stats.wc >= NUM_ANIMATIONS(op) / NUM_FACINGS(op))
		op->stats.wc=NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1;

	op->state = (uint8) op->stats.wc;
	SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
	update_object(op, UP_OBJ_FACE);

	if ((unsigned char) op->stats.wc == (NUM_ANIMATIONS(op) / NUM_FACINGS(op) - 1))
	{
		op->speed = 0;

		/* Closed, let's stop */
		update_ob_speed(op);
		return;
	}
}


/* stop_item() returns a pointer to the stopped object.  The stopped object
 * may or may not have been removed from maps or inventories.  It will not
 * have been merged with other items.
 *
 * This function assumes that only items on maps need special treatment.
 *
 * If the object can't be stopped, or it was destroyed while trying to stop
 * it, NULL is returned.
 *
 * fix_stopped_item() should be used if the stopped item should be put on
 * the map. */
object *stop_item(object *op)
{
	if (op->map == NULL)
		return op;

	switch (op->type)
	{
		case THROWN_OBJ:
		{
			object *payload = op->inv;

			if (payload == NULL)
				return NULL;

			remove_ob(payload);
			check_walk_off(payload, NULL, MOVE_APPLY_VANISHED);
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
			return payload;
		}

		case ARROW:
			if (op->speed >= MIN_ACTIVE_SPEED)
				op = fix_stopped_arrow(op);
			return op;

		case CONE:
			if (op->speed < MIN_ACTIVE_SPEED)
				return op;
			else
				return NULL;

		default:
			return op;
	}
}

/* fix_stopped_item() - put stopped item where stop_item() had found it.
 * Inserts item into the old map, or merges it if it already is on the map.
 *
 * 'map' must be the value of op->map before stop_item() was called. */
void fix_stopped_item(object *op, mapstruct *map, object *originator)
{
	if (map == NULL)
		return;

	if (QUERY_FLAG(op, FLAG_REMOVED))
		insert_ob_in_map (op, map, originator, 0);
	/* only some arrows actually need this */
	else if (op->type == ARROW)
		merge_ob(op, NULL);
}

object *fix_stopped_arrow(object *op)
{
	object *tmp;

	if (op->type != ARROW)
		return op;

#if 0
	/* Small chance of breaking */
	if (rndm(0, 99) < op->stats.food)
	{
		remove_ob(op);
		return NULL;
	}
#endif

	/* Used as temp. vars to control reflection/move speed */
	op->stats.grace = 0;
	op->stats.maxgrace = 0;

	op->direction = 0;
	CLEAR_FLAG(op, FLAG_WALK_ON);
	CLEAR_FLAG(op, FLAG_FLY_ON);
	CLEAR_MULTI_FLAG(op, FLAG_FLYING);

	/* Food is a self destruct marker - that long the item will need to be destruct! */
	if ((!(tmp = get_owner(op)) || tmp->type != PLAYER) && op->stats.food && op->type == ARROW)
	{
		SET_FLAG(op, FLAG_IS_USED_UP);
		SET_FLAG(op, FLAG_NO_PICK);

		/* Important to neutralize the arrow! */
		op->type = MISC_OBJECT;
		op->speed = 0.1f;
		op->speed_left = 0.0f;
	}
	else
	{
		op->stats.wc_range = op->arch->clone.stats.wc_range;
		op->last_sp = op->arch->clone.last_sp;
		op->level = op->arch->clone.level;
		op->stats.food = op->arch->clone.stats.food;
		op->speed = 0;
	}

	update_ob_speed(op);
	op->stats.wc = op->last_heal;
	op->stats.dam = op->stats.hp;

	/* Reset these to zero, so that CAN_MERGE will work properly */
	op->last_heal = 0;
	op->stats.hp = 0;
	op->face = op->arch->clone.face;

	/* So that stopped arrows will be saved */
	op->owner = NULL;
	update_object(op, UP_OBJ_FACE);

	return op;
}

/* stop_arrow() - what to do when a non-living flying object
 * has to stop. Sept 96 - I added in thrown object code in
 * here too. -b.t. */
void stop_arrow(object *op)
{
	play_sound_map(op->map, op->x, op->y, SOUND_DROP_THROW, SOUND_NORMAL);
	CLEAR_FLAG(op, FLAG_IS_MISSILE);

	if (op->inv)
	{
		object *payload = op->inv;

		remove_ob(payload);
		check_walk_off(payload, NULL, MOVE_APPLY_VANISHED);

#ifdef PLUGINS
		/* GROS: Handle for plugin stop event */
		if (payload->event_flags & EVENT_FLAG_STOP)
		{
			CFParm CFP;
			int k, l, m;
			object *event_obj = get_event_object(payload, EVENT_STOP);
			k = EVENT_STOP;
			l = SCRIPT_FIX_NOTHING;
			m = 0;
			CFP.Value[0] = &k;
			CFP.Value[1] = NULL;
			CFP.Value[2] = payload;
			CFP.Value[3] = op;
			CFP.Value[4] = NULL;
			CFP.Value[5] = &m;
			CFP.Value[6] = &m;
			CFP.Value[7] = &m;
			CFP.Value[8] = &l;
			CFP.Value[9] = (char *)event_obj->race;
			CFP.Value[10] = (char *)event_obj->slaying;

			if (findPlugin(event_obj->name) >= 0)
			{
				((PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP));
			}
		}
#endif
		/* we have a thrown potion here.
		 * This potion has NOT hit a target.
		 * it has hitten a wall or just dropped to the ground.
		 * 1.) its a AE spell... detonate it.
		 * 2.) its something else - shatter the potion. */
		if (payload->type == POTION)
		{
			if (payload->stats.sp != SP_NO_SPELL && spells[payload->stats.sp].flags & SPELL_DESC_DIRECTION)
				cast_spell(payload, payload, payload->direction, payload->stats.sp, 1, spellPotion, NULL);
		}
		else
		{
			clear_owner(payload);
			/* Gecko: if the script didn't put the payload somewhere else */
			if (!payload->env && !OBJECT_FREE(payload))
				insert_ob_in_map(payload, op->map, payload, 0);
		}

		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
	}
	else
	{
		op = fix_stopped_arrow(op);

		if (op)
			merge_ob(op, NULL);
	}
}

/**
 * Move an arrow along its course.
 * @param op The arrow or thrown object */
void move_arrow(object *op)
{
	object *tmp = NULL, *hitter;
	int new_x, new_y;
	int flag_tmp;
	int was_reflected;
	mapstruct *m = op->map;

	if (op->map == NULL)
	{
		LOG(llevBug, "BUG: move_arrow(): Arrow %s had no map.\n", query_name(op, NULL));
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}

	/* we need to stop thrown objects and arrows at some point. Like here. */
	if (op->type == THROWN_OBJ)
	{
		if (op->inv == NULL)
			return;
	}

	if (op->last_sp-- < 0)
	{
		stop_arrow(op);
		return;
	}

	/* Calculate target map square */
	if (op->stats.grace == 666)
	{
		/* Experimental target throwing hack. Using bresenham line algo */
		int dx = op->stats.hp;
		int dy = op->stats.sp;

		if (dx > dy)
		{
			if (op->stats.exp >= 0)
			{
				new_y = op->y + op->stats.maxsp;
				/* same as fraction -= 2*dx */
				op->stats.exp -= dx;
			}
			else
				new_y = op->y;

			new_x = op->x + op->stats.maxhp;
			/* same as fraction -= 2*dy */
			op->stats.exp += dy;
		}
		else
		{
			if (op->stats.exp >= 0)
			{
				new_x = op->x + op->stats.maxhp;
				op->stats.exp -= dy;
			}
			else
				new_x = op->x;

			new_y = op->y + op->stats.maxsp;
			op->stats.exp += dx;
		}
	}
	else
	{
		new_x = op->x + DIRX(op);
		new_y = op->y + DIRY(op);
	}

	was_reflected = 0;

	/* check we are legal */
	if (!(m = out_of_map(op->map, &new_x, &new_y)))
	{
		/* out of map... here is the end */
		stop_arrow(op);
		return;
	}

	if (!(hitter = get_owner(op)))
		hitter = op;

	/* ok, lets check there is something we can hit */
	if ((flag_tmp = GET_MAP_FLAGS(m, new_x, new_y)) & (P_IS_ALIVE | P_IS_PLAYER))
	{
		/* search for a vulnerable object */
		for (tmp = GET_MAP_OB_LAYER(m, new_x, new_y, 5); tmp != NULL; tmp = tmp->above)
		{
			/* Now, let friends fire through friends */
			/* TODO: Shouldn't do this if on pvp map, but that
			 * also requires smarter mob/npc archers */
			if (is_friend_of(hitter, tmp) || tmp == hitter)
			{
				continue;
			}

			if (IS_LIVE(tmp) && (!QUERY_FLAG(tmp, FLAG_CAN_REFL_MISSILE) || (rndm(0, 99)) < 90 - op->level / 10))
			{
				/* Attack the object. */
				op = hit_with_arrow(op, tmp);

				/* the arrow has hit and is destroyed! */
				if (op == NULL)
					return;
			}
		}
	}

	/* if we are here, there is no target and/or we have not hit.
	 * now we do a simple reflection test. */
	if (flag_tmp & P_REFL_MISSILE)
	{
		missile_reflection_adjust(op, QUERY_FLAG(op, FLAG_WAS_REFLECTED));

		op->direction = absdir (op->direction + 4);
		op->state = 0;

		if (GET_ANIM_ID(op))
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);

		if (wall(m, new_x, new_y))
		{
			/* Target is standing on a wall.  Let arrow turn around before
			 * the wall. */
			 new_x = op->x;
			 new_y = op->y;
		}

		SET_FLAG(op, FLAG_WAS_REFLECTED);
		/* skip normal movement calculations */
		was_reflected = 1;
	}

	if (!was_reflected && wall(m, new_x, new_y))
	{
		/* if the object doesn't reflect, stop the arrow from moving */
		if (!QUERY_FLAG(op, FLAG_REFLECTING) || !(rndm(0, 19)))
		{
			stop_arrow (op);
			return;
		}
		/* object is reflected */
		else
		{
			/* If one of the major directions (n,s,e,w), just reverse it */
			if (op->direction & 1)
			{
				op->direction = absdir(op->direction + 4);
			}
			else
			{
				/* The below is just logic for figuring out what direction
				 * the object should now take. */
				int left = wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (left == right)
					op->direction = absdir(op->direction + 4);
				else if (left)
					op->direction = absdir(op->direction + 2);
				else if (right)
					op->direction = absdir(op->direction - 2);
			}

			/* Is the new direction also a wall?  If show, shuffle again */
			if (wall(op->map, op->x + DIRX(op), op->y + DIRY(op)))
			{
				int left= wall(op->map, op->x + freearr_x[absdir(op->direction - 1)], op->y + freearr_y[absdir(op->direction - 1)]), right = wall(op->map, op->x + freearr_x[absdir(op->direction + 1)], op->y + freearr_y[absdir(op->direction + 1)]);

				if (!left)
					op->direction = absdir(op->direction - 1);
				else if (!right)
					op->direction = absdir(op->direction + 1);
				/* is this possible? */
				else
				{
					stop_arrow (op);
					return;
				}
			}

			/* update object image for new facing */
			/* many thrown objects *don't* have more than one face */
			if (GET_ANIM_ID(op))
				SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
		}
	}

	/* Move the arrow. */
	remove_ob(op);

	if (check_walk_off(op, NULL, MOVE_APPLY_VANISHED) == CHECK_WALK_OK)
	{
		op->x = new_x;
		op->y = new_y;
		insert_ob_in_map(op, m, op,0);
	}
}

/* This routine doesnt seem to work for "inanimate" objects that
 * are being carried, ie a held torch leaps from your hands!.
 * Modified this routine to allow held objects. b.t. */
/* Doesn't handle linked objs yet */
void change_object(object *op)
{
	object *tmp, *env;
	int i, j;

	/* In non-living items only change when food value is 0 */
	if (!IS_LIVE(op))
	{
		if (op->stats.food-- > 0)
			return;
		else
		{
			/* we had hooked applyable light object here - handle them special */
			if (op->type == TYPE_LIGHT_APPLY)
			{
				CLEAR_FLAG(op, FLAG_CHANGING);

				/* thats special lights like lamp which can be refilled */
				if (op->other_arch == NULL)
				{
					op->stats.food = 0;
					if (op->other_arch && op->other_arch->clone.sub_type1 & 1)
					{
						op->animation_id = op->other_arch->clone.animation_id;
						SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
					}
					else
					{
						CLEAR_FLAG(op, FLAG_ANIMATE);
						op->face = op->arch->clone.face;
					}

					/* not on map? */
					if (op->env)
					{
						/* inside player char? */
						if (op->env->type == PLAYER)
						{
							new_draw_info_format(NDI_UNIQUE, 0, op->env, "The %s burnt out.", query_name(op, NULL));
							op->glow_radius = 0;
							esrv_send_item(op->env, op);
							/* fix player will take care about adjust light masks */
							fix_player(op->env);
						}
						/* atm, lights inside other inv as players don't set light masks */
						else
						{
							/* but we need to update container which are possible watched by players */
							op->glow_radius = 0;
							if (op->env->type == CONTAINER)
								esrv_send_item(NULL, op);
						}
					}
					/* object is on map */
					else
					{
						/* remove light mask from map */
						adjust_light_source(op->map, op->x, op->y, -(op->glow_radius));
						/* tell map update we have something changed */
						update_object(op, UP_OBJ_FACE);
						op->glow_radius = 0;
					}
					return;
				}
				/* this object will be deleted and exchanged with other_arch */
				else
				{
					/* but give the player a note about it too */
					if (op->env && op->env->type == PLAYER)
						new_draw_info_format(NDI_UNIQUE, 0, op->env, "The %s burnt out.", query_name(op, NULL));
				}

			}
		}
	}

	if (op->other_arch == NULL)
	{
		LOG(llevBug, "BUG: Change object (%s) without other_arch error.\n", op->name);
		return;
	}

	env = op->env;
	remove_ob(op);
	check_walk_off(op, NULL,MOVE_APPLY_VANISHED);

	/* atm we only generate per change tick *ONE* object */
	for (i = 0; i < 1; i++)
	{
		tmp = arch_to_object(op->other_arch);
		/* The only variable it keeps. */
		tmp->stats.hp = op->stats.hp;
		if (env)
		{
			tmp->x = env->x, tmp->y = env->y;
			tmp = insert_ob_in_ob(tmp, env);

			/* this should handle in future insert_ob_in_ob() */
			if (env->type == PLAYER)
			{
				esrv_del_item(CONTR(env), op->count, NULL);
				esrv_send_item(env, tmp);
			}
			else if (env->type == CONTAINER)
			{
				esrv_del_item(NULL, op->count, env);
				esrv_send_item(env, tmp);
			}
		}
		else
		{
			j = find_first_free_spot(tmp->arch, op->map, op->x, op->y);
			/* Found a free spot */
			if (j != -1)
			{
				tmp->x = op->x + freearr_x[j], tmp->y = op->y + freearr_y[j];
				insert_ob_in_map(tmp, op->map, op, 0);
			}
		}
	}
}


/* i do some heavy changes to telporters.
 * First, with tiled maps it is a big problem, that teleporters
 * can only move player over maps. Second, i added a "no_teleport"
 * flag to the engine.
 * The teleporter will now teleport ANY object on the tile node - also
 * multi arch objects which are with one part on the teleporter.
 * WARNING: Also system objects will be teleported when they don't
 * have a "no_teleport" flag.
 * Because we can teleport multi arch monster now with a single tile
 * teleporter, i removed multi arch teleporters. */
void move_teleporter(object *op)
{
	object *tmp, *next;

	/* get first object of this map node */
	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = next)
	{
		next = tmp->above;
		if (QUERY_FLAG(tmp, FLAG_NO_TELEPORT))
			continue;

		/* teleport to different map */
		if (EXIT_PATH(op))
		{
#ifdef PLUGINS
			/* GROS: Handle for plugin TRIGGER event */
			if (op->event_flags & EVENT_FLAG_TRIGGER)
			{
				CFParm CFP;
				CFParm* CFR;
				int k, l, m;
				int rtn_script = 0;
				object *event_obj = get_event_object(op, EVENT_TRIGGER);
				m = 0;
				k = EVENT_TRIGGER;
				l = SCRIPT_FIX_NOTHING;
				CFP.Value[0] = &k;
				/* activator first */
				CFP.Value[1] = tmp;
				/* thats whoisme */
				CFP.Value[2] = op;
				CFP.Value[3] = NULL;
				CFP.Value[4] = NULL;
				CFP.Value[5] = &m;
				CFP.Value[6] = &m;
				CFP.Value[7] = &m;
				CFP.Value[8] = &l;
				CFP.Value[9] = (char *)event_obj->race;
				CFP.Value[10]= (char *)event_obj->slaying;

				if (findPlugin(event_obj->name) >= 0)
				{
					CFR = (PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP);
					rtn_script = *(int *)(CFR->Value[0]);
				}

				if (rtn_script != 0)
					return;
			}
#endif
			enter_exit(tmp, op);
		}
		/* teleport inside this map */
		else if (EXIT_X(op) != -1 && EXIT_Y(op) != -1)
		{
			/* use OUT_OF_REAL_MAP() - we want be truly on THIS map */
			if (OUT_OF_REAL_MAP(op->map, EXIT_X(op), EXIT_Y(op)))
			{
				LOG(llevBug, "BUG: Removed illegal teleporter (map: %s (%d,%d)) -> (%d,%d)\n", op->map->name, op->x, op->y, EXIT_X(op), EXIT_Y(op));
				remove_ob(op);
				check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				return;
			}

#ifdef PLUGINS
			/* GROS: Handle for plugin TRIGGER event */
			if (op->event_flags & EVENT_FLAG_TRIGGER)
			{
				CFParm CFP;
				CFParm* CFR;
				int k, l,m;
				int rtn_script = 0;
				object *event_obj = get_event_object(op, EVENT_TRIGGER);
				m = 0;
				k = EVENT_TRIGGER;
				l = SCRIPT_FIX_NOTHING;
				CFP.Value[0] = &k;
				CFP.Value[1] = tmp;
				CFP.Value[2] = op;
				CFP.Value[3] = NULL;
				CFP.Value[4] = NULL;
				CFP.Value[5] = &m;
				CFP.Value[6] = &m;
				CFP.Value[7] = &m;
				CFP.Value[8] = &l;
				CFP.Value[9] = (char *)event_obj->race;
				CFP.Value[10]= (char *)event_obj->slaying;
				if (findPlugin(event_obj->name) >= 0)
				{
					CFR = (PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP);
					rtn_script = *(int *)(CFR->Value[0]);
				}
				if (rtn_script != 0)
					return;
			}
#endif
			transfer_ob(tmp, EXIT_X(op), EXIT_Y(op), 0, op, NULL);
		 }
		else
		{
			/* Random teleporter */
#ifdef PLUGINS
			/* GROS: Handle for plugin TRIGGER event */
			if (op->event_flags & EVENT_FLAG_TRIGGER)
			{
				CFParm CFP;
				CFParm* CFR;
				int k, l, m;
				int rtn_script = 0;
				object *event_obj = get_event_object(op, EVENT_TRIGGER);
				m = 0;
				k = EVENT_TRIGGER;
				l = SCRIPT_FIX_NOTHING;
				CFP.Value[0] = &k;
				CFP.Value[1] = op;
				CFP.Value[2] = tmp;
				CFP.Value[3] = NULL;
				CFP.Value[4] = NULL;
				CFP.Value[5] = &m;
				CFP.Value[6] = &m;
				CFP.Value[7] = &m;
				CFP.Value[8] = &l;
				CFP.Value[9] = (char *)event_obj->race;
				CFP.Value[10] = (char *)event_obj->slaying;

				if (findPlugin(event_obj->name) >= 0)
				{
					CFR = (PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP);
					rtn_script = *(int *)(CFR->Value[0]);
				}

				if (rtn_script != 0)
					return;
			}
#endif
			teleport(op, TELEPORTER, tmp);
		}
	}
}

/* peterm:  firewalls generalized to be able to shoot any type
 * of spell at all.  the stats.dam field of a firewall object
 * contains it's spelltype. The direction of the wall is stored
 * in op->direction. walls can have hp, so they can be torn down. */

/* added some new features to FIREWALL - on/off features by connected,
 * advanced spell selection and full turnable by connected and
 * autoturn. MT-2003 */
void move_firewall(object *op)
{
	/* last_eat 0 = off */
	/* dm has created a firewall in his inventory or no legal spell selected */
  	if (!op->map || !op->last_eat || op->stats.dam == -1)
		return;

  	cast_spell(op, op, op->direction, op->stats.dam, 1, spellNPC, NULL);
}

void move_firechest(object *op)
{
	/* dm has created a firechest in his inventory */
  	if (!op->map)
		return;

  	fire_a_ball(op, rndm(1, 8), 7);
}

/* move_player_mover:  this function takes a "player mover" as an
 * argument, and performs the function of a player mover, which is:
 * a player mover finds any players that are sitting on it.  It
 * moves them in the op->stats.sp direction.  speed is how often it'll move.
 * If attacktype is nonzero it will paralyze the player.  If lifesave is set,
 * it'll dissapear after hp+1 moves.  If hp is set and attacktype is set,
 * it'll paralyze the victim for hp*his speed/op->speed */
void move_player_mover(object *op)
{
	object *victim, *nextmover;
	mapstruct *mt;
	int xt, yt, dir = op->direction;

	if (!(blocked(NULL, op->map, op->x, op->y, TERRAIN_NOTHING) & (P_IS_ALIVE | P_IS_PLAYER)))
		return;

	/* Determine direction now for random movers so we do the right thing */
	if (!dir)
		dir = rndm(1, 8);

	for (victim = get_map_ob(op->map, op->x, op->y); victim != NULL; victim = victim->above)
	{
		if (IS_LIVE(victim) && (!(QUERY_FLAG(victim, FLAG_FLYING)) || op->stats.maxhp))
		{

			if (QUERY_FLAG(op, FLAG_LIFESAVE) && op->stats.hp-- < 0)
			{
				destruct_ob(op);
				return;
			}

			xt = op->x + freearr_x[dir];
			yt = op->y + freearr_y[dir];

			if (!(mt = out_of_map(op->map, &xt, &yt)))
				return;

			for (nextmover = get_map_ob(mt, xt, yt); nextmover != NULL; nextmover = nextmover->above)
			{
				if (nextmover->type == PLAYERMOVER)
					nextmover->speed_left = -0.99f;

				/* wait until the next thing gets out of the way */
				if (IS_LIVE(nextmover))
					op->speed_left = -1.1f;
			}

			if (victim->type == PLAYER)
			{
				/*  only level >=1 movers move people */
				if (op->level)
				{
					/* Following is a bit of hack.  We need to make sure it
					 * is cleared, otherwise the player will get stuck in
					 * place.  This can happen if the player used a spell to
					 * get to this space. */
					CONTR(victim)->fire_on = 0;
					victim->speed_left = -FABS(victim->speed);
					move_player(victim, dir);
				}
				else
					return;
			}
			else
				move_object(victim, dir);

			if (!op->stats.maxsp && op->attacktype)
				op->stats.maxsp = 2;

			/* flag to paralyze the player */
			if (op->attacktype)
				victim->speed_left = -FABS(op->stats.maxsp * victim->speed / op->speed);
		}
	}
}


/* move_creator (by peterm)
 * it has the creator object create it's other_arch right on top of it.
 * connected:  what will trigger it
 * hp:  how many times it may create before stopping
 * lifesave:  if set, it'll never disappear but will go on creating
 * everytime it's triggered
 * other_arch:  the object to create */
/* not multi arch fixed, i think MT */
void move_creator(object *op)
{
	object *tmp;

	if (!op->other_arch)
		return;

	op->stats.hp--;

	if (op->stats.hp < 0 && !QUERY_FLAG(op, FLAG_LIFESAVE))
	{
		op->stats.hp = -1;
		return;
	}

	tmp = arch_to_object(op->other_arch);
	if (op->slaying)
	{
		FREE_AND_COPY_HASH(tmp->name, op->slaying);
		FREE_AND_COPY_HASH(tmp->title, op->slaying);
	}

	tmp->x = op->x;
	tmp->y = op->y;
	tmp->map = op->map;
	tmp->level = op->level;
	insert_ob_in_map(tmp, op->map, op, 0);
}

/* move_marker --peterm@soda.csua.berkeley.edu
 * when moved, a marker will search for a player sitting above
 * it, and insert an invisible, weightless force into him
 * with a specific code as the slaying field.
 * At that time, it writes the contents of its own message
 * field to the player.  The marker will decrement hp to
 * 0 and then delete itself every time it grants a mark.
 * unless hp was zero to start with, in which case it is infinite. */
void move_marker(object *op)
{
	object *tmp, *tmp2;

	for (tmp = get_map_ob(op->map, op->x, op->y); tmp != NULL; tmp = tmp->above)
	{
		/* we've got someone to MARK */
		if (tmp->type == PLAYER)
		{
			/* remove an old force with a slaying field == op->name */
			for (tmp2 = tmp->inv; tmp2 !=NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == FORCE && tmp2->slaying && !strcmp(tmp2->slaying, op->name))
					break;
			}

			if (tmp2)
				remove_ob(tmp2);

			/* cycle through his inventory to look for the MARK we want to place */
			for (tmp2 = tmp->inv; tmp2 != NULL; tmp2 = tmp2->below)
			{
				if (tmp2->type == FORCE && tmp2->slaying && !strcmp(tmp2->slaying, op->slaying))
					break;
			}

			/* if we didn't find our own MARK */
			if (tmp2 == NULL)
			{
				object *force = get_archetype("force");
				force->speed = 0;
				if (op->stats.food)
				{
					force->speed = 0.01f;
					force->speed_left = (float)-op->stats.food;
				}

				update_ob_speed(force);
				/* put in the lock code */
				FREE_AND_COPY_HASH(force->slaying, op->slaying);
				insert_ob_in_ob(force, tmp);

				if (op->msg)
					new_draw_info(NDI_UNIQUE | NDI_NAVY, 0, tmp, op->msg);

				if (op->stats.hp > 0)
				{
					op->stats.hp--;
					if (op->stats.hp == 0)
					{
						/* marker expires -- granted mark number limit */
						destruct_ob(op);
						return;
					}
				}
			}

		}

	}
}

int process_object(object *op)
{
   	if (QUERY_FLAG(op, FLAG_MONSTER))
		if (move_monster(op) || OBJECT_FREE(op))
	  		return 1;

	if (QUERY_FLAG(op, FLAG_CHANGING) && !op->state)
	{
		change_object(op);
		return 1;
	}

	if (QUERY_FLAG(op, FLAG_IS_USED_UP) && --op->stats.food <= 0)
	{
		if (QUERY_FLAG(op, FLAG_APPLIED) && op->type != CONTAINER)
			remove_force(op);
		else
		{
			/* we have a decying container on the floor (asuming its only possible here) ! */
			if (op->type == CONTAINER && (op->sub_type1&1) == ST1_CONTAINER_CORPSE)
			{
				/* this means someone access the corpse */
				if (op->attacked_by)
				{
					/* then stop decaying! - we don't want delete this under the hand of the guy! */

					/* give him a bit time back */
					op->stats.food += 3;
					/* go on */
					goto process_object_dirty_jump;
				}

				/* now we do something funny: WHEN the corpse is a (personal) bounty,
				 * we delete the bounty marker (->slaying) and reseting the counter.
				 * Now other people can access the corpse for stuff which are leaved
				 * here perhaps. */
				if (op->slaying || op->stats.maxhp)
				{
					if (op->slaying)
						FREE_AND_CLEAR_HASH2(op->slaying);

					if (op->stats.maxhp)
						op->stats.maxhp = 0;

					op->stats.food = op->arch->clone.stats.food;
					/* another lame way to update view of players... */
					remove_ob(op);
					insert_ob_in_map(op, op->map, NULL, INS_NO_WALK_ON);
					return 1;
				}

				if (op->env && op->env->type == CONTAINER)
					esrv_del_item(NULL, op->count, op->env);
				else
				{
					object *pl = is_player_inv(op);
					if (pl)
						esrv_del_item(CONTR(pl), op->count, op->env);
				}

				remove_ob(op);
				check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				return 1;
			}

			/* IF necessary, delete the item from the players inventory */
			if (op->env && op->env->type == CONTAINER)
				esrv_del_item(NULL, op->count, op->env);
			else
			{
				object *pl=is_player_inv(op);
				if (pl)
					esrv_del_item(CONTR(pl), op->count, op->env);
			}
			destruct_ob(op);
		}
		return 1;
	}

	process_object_dirty_jump:

	/* i don't like this script object here ..
	 * this is *the* core loop.*/
#ifdef PLUGINS
	/* GROS: Handle for plugin time event */
	if (op->event_flags & EVENT_FLAG_TIME)
	{
		CFParm CFP;
		int k, l, m;
		object *event_obj = get_event_object(op, EVENT_TIME);
		k = EVENT_TIME;
		l = SCRIPT_FIX_NOTHING;
		m = 0;
		CFP.Value[0] = &k;
		CFP.Value[1] = NULL;
		CFP.Value[2] = op;
		CFP.Value[3] = NULL;
		CFP.Value[4] = NULL;
		CFP.Value[5] = &m;
		CFP.Value[6] = &m;
		CFP.Value[7] = &m;
		CFP.Value[8] = &l;
		CFP.Value[9] = (char *)event_obj->race;
		CFP.Value[10] = (char *)event_obj->slaying;
		if (findPlugin(event_obj->name) >= 0)
			((PlugList[findPlugin(event_obj->name)].eventfunc) (&CFP));
	}
#endif

	switch (op->type)
	{
		case ROD:
		case HORN:
			regenerate_rod(op);
			return 1;

		case FORCE:
		case POTION_EFFECT:
			if (!QUERY_FLAG(op, FLAG_IS_USED_UP))
				remove_force(op);
			return 1;

		case SPAWN_POINT:
			spawn_point(op);
			return 0;

		case BLINDNESS:
			remove_blindness(op);
			return 0;

		case CONFUSION:
			remove_confusion(op);
			return 0;

		case POISONING:
			poison_more(op);
			return 0;

		case DISEASE:
			move_disease(op);
			return 0;

		case SYMPTOM:
			move_symptom(op);
			return 0;

		case WORD_OF_RECALL:
			execute_wor(op);
			return 0;

		case BULLET:
			move_fired_arch(op);
			return 0;

		case MMISSILE:
			move_missile(op);
			return 0;

		case THROWN_OBJ:
		case ARROW:
			move_arrow(op);
			return 0;

		case FBULLET:
			move_fired_arch(op);
			return 0;

		case FBALL:
		case POISONCLOUD:
			explosion(op);
			return 0;

		/* It now moves twice as fast */
		case LIGHTNING:
			move_bolt(op);
			return 0;

		case CONE:
			move_cone(op);
			return 0;

		case DOOR:
			remove_door(op);
			return 0;

		/* handle autoclosing */
		case LOCKED_DOOR:
			close_locked_door(op);
			return 0;

		case TELEPORTER:
			move_teleporter(op);
			return 0;

		case BOMB:
			animate_bomb(op);
			return 0;

		case GOLEM:
			move_golem(op);
			return 0;

		case EARTHWALL:
			hit_player(op, 2, op, AT_PHYSICAL);
			return 0;

		case FIREWALL:
			move_firewall(op);
			return 0;

		case FIRECHEST:
			move_firechest(op);
			return 0;

		case MOOD_FLOOR:
			do_mood_floor(op, op);
			return 0;

		case GATE:
			move_gate(op);
			return 0;

		case TIMED_GATE:
			move_timed_gate(op);
			return 0;

		case TRIGGER:
		case TRIGGER_BUTTON:
		case TRIGGER_PEDESTAL:
		case TRIGGER_ALTAR:
			animate_trigger(op);
			return 0;

		case DETECTOR:
			move_detector(op);
			return 0;

		case PIT:
			move_pit(op);
			return 0;

		case DEEP_SWAMP:
			move_deep_swamp(op);
			return 0;

		case CANCELLATION:
			move_cancellation(op);
			return 0;

		case BALL_LIGHTNING:
			move_ball_lightning(op);
			return 0;

		case SWARM_SPELL:
			move_swarm_spell(op);
			return 0;

		case PLAYERMOVER:
			move_player_mover(op);
			return 0;

		case CREATOR:
			move_creator(op);
			return 0;

		case MARKER:
			move_marker(op);
			return 0;

		case AURA:
			move_aura(op);
			return 0;

		case PEACEMAKER:
			move_peacemaker(op);
			return 0;
	}

  	return 0;
}

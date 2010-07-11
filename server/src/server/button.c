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
 * Buttons, gates, handles, etc related code. */

#include <global.h>

static void animate_turning(object *op);
static void trigger_move(object *op, int state);
static objectlink *get_button_links(object *button);

/**
 * Push the specified object. This can affect other buttons/gates/handles
 * altars/pedestals/holes on the whole map.
 *
 * The routine loops through _all_ objects on the map.
 * Better hurry with that linked list...
 * @param op The object to push */
void push_button(object *op)
{
	object *tmp;
	objectlink *ol;

	/*LOG(llevDebug, "push_button: %s (%d)\n", op->name, op->count);*/
	for (ol = get_button_links(op); ol; ol = ol->next)
	{
		tmp = ol->objlink.ob;

		if (!tmp || tmp->count != ol->id)
		{
			LOG(llevBug, "BUG: Internal error in push_button (%s).\n", op->name);
			continue;
		}

		/* a button link object can become freed when the map is saving.  As
		 * a map is saved, objects are removed and freed, and if an object is
		 * on top of a button, this function is eventually called.  If a map
		 * is getting moved out of memory, the status of buttons and levers
		 * probably isn't important - it will get sorted out when the map is
		 * re-loaded.  As such, just exit this function if that is the case. */
		if (!OBJECT_ACTIVE(tmp))
		{
			LOG(llevDebug, "DEBUG: push_button: button link with invalid object! (%x - %x)", QUERY_FLAG(tmp, FLAG_REMOVED), tmp->count);
			return;
		}

		switch (tmp->type)
		{
			case GATE:
			case PIT:
				tmp->value = tmp->stats.maxsp ? !op->value : op->value;
				tmp->speed = 0.5;
				update_ob_speed(tmp);
				break;

			case HANDLE:
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + (tmp->value = tmp->stats.maxsp ? !op->value : op->value));
				update_object(tmp, UP_OBJ_FACE);
				break;

			case SIGN:
				if (!tmp->stats.food || tmp->last_eat < tmp->stats.food)
				{
					new_info_map(NDI_UNIQUE | NDI_NAVY, tmp->map, tmp->x, tmp->y, MAP_INFO_NORMAL, tmp->msg);

					if (tmp->stats.food)
					{
						tmp->last_eat++;
					}
				}

				break;

			case ALTAR:
				tmp->value = 1;
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
				update_object(tmp, UP_OBJ_FACE);
				break;

			case BUTTON:
			case PEDESTAL:
				tmp->value = op->value;
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
				update_object(tmp, UP_OBJ_FACE);
				break;

			case TIMED_GATE:
				tmp->speed = tmp->arch->clone.speed;
				/* original values */
				update_ob_speed(tmp);
				tmp->value = tmp->arch->clone.value;
				tmp->stats.sp = 1;
				tmp->stats.hp = tmp->stats.maxhp;
				break;

			case FIREWALL:
				/* connection flag1 = on/off */
				if (op->last_eat)
				{
					tmp->last_eat != 0 ? (tmp->last_eat = 0) : (tmp->last_eat = 1);
				}
				/* "normal" connection - turn wall */
				else
				{
					/* next direction */
					if (tmp->stats.maxsp)
					{
						if ((tmp->direction += tmp->stats.maxsp) > 8)
						{
							tmp->direction = (tmp->direction % 8) + 1;
						}

						animate_turning(tmp);
					}
				}

				break;

			case DIRECTOR:
				/* next direction */
				if (tmp->stats.maxsp)
				{
					if ((tmp->direction += tmp->stats.maxsp) > 8)
					{
						tmp->direction = (tmp->direction % 8) + 1;
					}

					animate_turning(tmp);
				}

				break;

			case TELEPORTER:
				move_teleporter(tmp);
				break;

			case CREATOR:
				move_creator(tmp);
				break;

			case SPAWN_POINT:
				spawn_point(tmp);
				break;
		}
	}
}

/**
 * Updates everything connected with the button object.
 * After changing the state of a button, this function must be called
 * to make sure that all gates and other buttons connected to the
 * button react to the (eventual) change of state.
 * @param op The button object */
void update_button(object *op)
{
	object *ab, *tmp, *head;
	int move, fly, tot, any_down = 0, old_value = op->value;
	objectlink *ol;

	for (ol = get_button_links(op); ol; ol = ol->next)
	{
		tmp = ol->objlink.ob;

		if (!tmp || tmp->count != ol->id)
		{
			LOG(llevDebug, "Internal error in update_button (%s).\n", op->name);
			continue;
		}

		if (tmp->type == BUTTON)
		{
			fly = QUERY_FLAG(tmp, FLAG_FLY_ON);
			move = QUERY_FLAG(tmp, FLAG_WALK_ON);

			for (ab = GET_BOTTOM_MAP_OB(tmp), tot = 0; ab != NULL; ab = ab->above)
			{
				if (ab != tmp && (fly ? (int) QUERY_FLAG(ab, FLAG_FLYING) : move))
				{
					tot += ab->weight * (ab->nrof ? ab->nrof : 1) + ab->carrying;
				}
			}

			tmp->value = (tot >= tmp->weight) ? 1 : 0;

			if (tmp->value)
			{
				any_down = 1;
			}

		}
		else if (tmp->type == PEDESTAL)
		{
			tmp->value = 0;
			fly = QUERY_FLAG(tmp, FLAG_FLY_ON);
			move = QUERY_FLAG(tmp, FLAG_WALK_ON);

			for (ab = GET_BOTTOM_MAP_OB(tmp); ab != NULL; ab = ab->above)
			{
				head = ab->head ? ab->head : ab;

				if (ab != tmp && (fly ? (int) QUERY_FLAG(ab, FLAG_FLYING) : move) && (head->race == tmp->slaying || (!strcmp(tmp->slaying, "player") && head->type == PLAYER)))
				{
					tmp->value = 1;
				}
			}

			if (tmp->value)
			{
				any_down = 1;
			}
		}
	}

	/* If any other buttons were down, force this to remain down */
	if (any_down)
	{
		op->value = 1;
	}

	/* If this button hasn't changed, don't do anything */
	if (op->value != old_value)
	{
		SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->value);
		update_object(op, UP_OBJ_FACE);
		/* Make all other buttons the same */
		push_button(op);
	}
}

/**
 * Updates every button on the map by calling update_button()
 * for them.
 * @param m The map to update buttons for */
void update_buttons(mapstruct *m)
{
	objectlink *ol, *obp;
	object *ab, *tmp;
	int fly, move;

	for (obp = m->buttons; obp; obp = obp->next)
	{
		for (ol = obp->objlink.link; ol; ol = ol->next)
		{
			if (!ol->objlink.ob || ol->objlink.ob->count != ol->id)
			{
				LOG(llevBug, "BUG: Internal error in update_button (%s (%dx%d):%d, connected %ld ).\n", ol->objlink.ob ? ol->objlink.ob->name : "null", ol->objlink.ob ? ol->objlink.ob->x:-1, ol->objlink.ob ? ol->objlink.ob->y:-1, ol->id, obp->value);
				continue;
			}

			if (ol->objlink.ob->type == BUTTON || ol->objlink.ob->type == PEDESTAL)
			{
				update_button(ol->objlink.ob);
			}
			else if (ol->objlink.ob->type == CHECK_INV)
			{
				tmp = ol->objlink.ob;
				fly = QUERY_FLAG(tmp, FLAG_FLY_ON);
				move = QUERY_FLAG(tmp, FLAG_WALK_ON);

				for (ab = GET_BOTTOM_MAP_OB(tmp); ab != NULL; ab = ab->above)
				{
					if (ab != tmp && (fly ? (int) QUERY_FLAG(ab, FLAG_FLYING) : move))
					{
						check_inv(ab, tmp);
					}
				}
			}
			else if (ol->objlink.ob->type == TRIGGER_BUTTON || ol->objlink.ob->type == TRIGGER_PEDESTAL || ol->objlink.ob->type == TRIGGER_ALTAR)
			{
				/* check_trigger will itself sort out the numbers of
				 * items above the trigger */
				check_trigger(ol->objlink.ob, ol->objlink.ob);
			}
		}
	}
}

/**
 * Toggles the state of specified button.
 * @param op Object to toggle. */
void use_trigger(object *op)
{
	/* Toggle value */
	op->value = !op->value;
	push_button(op);
}

/**
 * Animates one step of object.
 * @param op Object to animate. */
static void animate_turning(object *op)
{
	SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->state);
	update_object(op, UP_OBJ_FACE);
}

/* 1 down and 0 up */
static void trigger_move(object *op, int state)
{
	op->stats.wc = state;

	if (state)
	{
		use_trigger(op);

		if (op->stats.exp)
		{
			op->speed = 1.0f / op->stats.exp;
			update_ob_speed(op);
			op->speed_left = -1;
		}
	}
	else
	{
		use_trigger(op);
		op->speed = 0;
		update_ob_speed(op);
	}
}

/* cause != NULL: something has moved on top of op
 *
 * cause == NULL: nothing has moved, we have been called from
 * animate_trigger().
 *
 * TRIGGER_ALTAR: Returns 1 if 'cause' was destroyed, 0 if not.
 *
 * TRIGGER: Returns 1 if handle could be moved, 0 if not.
 *
 * TRIGGER_BUTTON, TRIGGER_PEDESTAL: Returns 0. */
int check_trigger(object *op, object *cause)
{
	object *tmp;
	int push = 0, tot = 0;
	int in_movement = op->stats.wc || op->speed;

	switch (op->type)
	{
		case TRIGGER_BUTTON:
			if (op->weight > 0)
			{
				if (cause)
				{
					for (tmp = GET_BOTTOM_MAP_OB(op); tmp; tmp = tmp->above)
					{
						if (!QUERY_FLAG(tmp, FLAG_FLYING))
						{
							tot += tmp->weight * (tmp->nrof ? tmp->nrof : 1) + tmp->carrying;
						}
					}

					if (tot >= op->weight)
					{
						push = 1;
					}

					if (op->stats.ac == push)
					{
						return 0;
					}

					op->stats.ac = push;
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + push);
					update_object(op, UP_OBJ_FACE);

					if (in_movement || !push)
					{
						return 0;
					}
				}

				trigger_move(op, push);
			}

			return 0;

		case TRIGGER_PEDESTAL:
			if (cause)
			{
				for (tmp = GET_BOTTOM_MAP_OB(op); tmp; tmp = tmp->above)
				{
					object *head = tmp->head ? tmp->head : tmp;

					if ((!QUERY_FLAG(head, FLAG_FLYING) || QUERY_FLAG(op, FLAG_FLY_ON)) && (head->race == op->slaying || (!strcmp(op->slaying, "player") && head->type == PLAYER)))
					{
						push = 1;
						break;
					}
				}

				if (op->stats.ac == push)
				{
					return 0;
				}

				op->stats.ac = push;
				SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + push);
				update_object(op, UP_OBJ_FACE);

				if (in_movement || !push)
				{
					return 0;
				}
			}

			trigger_move(op, push);
			return 0;

		case TRIGGER_ALTAR:
			if (cause)
			{
				if (in_movement)
				{
					return 0;
				}

				if (operate_altar(op, &cause))
				{
					SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + 1);
					update_object(op, UP_OBJ_FACE);

					if (op->last_sp >= 0)
					{
						trigger_move (op, 1);

						if (op->last_sp > 0)
						{
							op->last_sp = -op->last_sp;
						}
					}
					else
					{
						/* for trigger altar with last_sp, the ON/OFF
						 * status (-> +/- value) is "simulated": */
						op->value = !op->value;
						trigger_move(op, 1);
						op->last_sp = -op->last_sp;
						op->value = !op->value;
					}

					return cause == NULL;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction);
				update_object(op, UP_OBJ_FACE);

				/* If trigger_altar has "last_sp > 0" set on the map,
				 * it will push the connected value only once per sacrifice.
				 * Otherwise (default), the connected value will be
				 * pushed twice: First by sacrifice, second by reset! -AV */
				if (!op->last_sp)
				{
					trigger_move(op, 0);
				}
				else
				{
					op->stats.wc = 0;
					op->value = !op->value;
					op->speed = 0;
					update_ob_speed(op);
				}
			}

			return 0;

		case TRIGGER:
			if (cause)
			{
				if (in_movement)
				{
					return 0;
				}

				push = 1;
			}

			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + push);
			update_object(op, UP_OBJ_FACE);
			trigger_move(op, push);
			return 1;

		default:
			LOG(llevDebug, "Unknown trigger type: %s (%d)\n", op->name, op->type);
			return 0;
	}
}

/**
 * Links specified object in the map.
 * @param button Object to link. Must not be NULL.
 * @param map Map we are on. Should not be NULL.
 * @param connected Connection value for the item. */
void add_button_link(object *button, mapstruct *map, int connected)
{
	objectlink *obp, *ol = get_objectlink();

	if (!map)
	{
		LOG(llevBug, "BUG: Tried to add button-link without map.\n");
		return;
	}

	button->path_attuned = connected;

	SET_FLAG(button, FLAG_IS_LINKED);

	ol->objlink.ob = button;
	ol->id = button->count;

	for (obp = map->buttons; obp && obp->value != connected; obp = obp->next);
	{
		if (obp)
		{
			ol->next = obp->objlink.link;
			obp->objlink.link = ol;
		}
		else
		{
			obp = get_objectlink();
			obp->value = connected;

			obp->next = map->buttons;
			map->buttons = obp;
			obp->objlink.link = ol;
		}
	}
}

/**
 * Remove the object from the linked lists of buttons in the map.
 * @param op Object to remove. Must be on a map, and linked. */
void remove_button_link(object *op)
{
	objectlink *obp, **olp, *ol;

	if (op->map == NULL)
	{
		LOG(llevBug, "BUG: remove_button_link(): Object without map.\n");
		return;
	}

	if (!QUERY_FLAG(op, FLAG_IS_LINKED))
	{
		LOG(llevBug, "BUG: remove_button_link(): Unlinked object.\n");
		return;
	}

	for (obp = op->map->buttons; obp; obp = obp->next)
	{
		for (olp = &obp->objlink.link; (ol = *olp); olp = &ol->next)
		{
			if (ol->objlink.ob == op)
			{
				*olp = ol->next;
				free_objectlink_simple(ol);
				return;
			}
		}
	}

	LOG(llevBug, "BUG: remove_button_link(): Couldn't find object.\n");
	CLEAR_FLAG(op, FLAG_IS_LINKED);
}

/**
 * Return the first objectlink in the objects linked to this one.
 * @param button Object to check. Must not be NULL.
 * @return ::objectlink for this object, or NULL. */
static objectlink *get_button_links(object *button)
{
	objectlink *obp, *ol;

	if (!button->map)
	{
		return NULL;
	}

	for (obp = button->map->buttons; obp; obp = obp->next)
	{
		for (ol = obp->objlink.link; ol; ol = ol->next)
		{
			if (ol->objlink.ob == button && ol->id == button->count)
			{
				return obp->objlink.link;
			}
		}
	}

	return NULL;
}

/**
 * Returns the first value linked to this button.
 * Made as a separate function to increase efficiency.
 * @param button Object to check. Must not be NULL.
 * @return Connection value, or 0 if not connected. */
int get_button_value(object *button)
{
	objectlink *obp, *ol;

	if (!button->map)
	{
		return 0;
	}

	for (obp = button->map->buttons; obp; obp = obp->next)
	{
		for (ol = obp->objlink.link; ol; ol = ol->next)
		{
			if (ol->objlink.ob == button && ol->id == button->count)
			{
				return obp->value;
			}
		}
	}

	return 0;
}

/**
 * This routine makes monsters who are standing on the
 * 'mood floor' change their disposition if it is
 * different.
 *
 * If floor is set to be triggered must have a speed
 * of zero (default is 1 for all but the charm floor
 * type).
 * @author b.t. thomas@nomad.astro.psu.edu
 * @param op The mood floor object. */
void do_mood_floor(object *op)
{
	object *tmp;

	for (tmp = op->above; tmp; tmp = tmp->above)
	{
		if (QUERY_FLAG(tmp, FLAG_MONSTER))
		{
			break;
		}
	}

	/* Doesn't affect players, and if there is a player on this space, won't also
	 * be a monster here. */
	if (!tmp || tmp->type == PLAYER)
	{
		return;
	}

	switch (op->last_sp)
	{
		/* Furious -- makes all monsters mad */
		case 0:
			if (QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE))
			{
				CLEAR_FLAG(tmp, FLAG_UNAGGRESSIVE);
			}

			if (QUERY_FLAG(tmp, FLAG_FRIENDLY))
			{
				CLEAR_FLAG(tmp, FLAG_FRIENDLY);
				tmp->move_type = 0;
				tmp->owner = NULL;
			}

			break;

		/* Angry -- get neutral monsters mad */
		case 1:
			if (QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE) && !QUERY_FLAG(tmp, FLAG_FRIENDLY))
			{
				CLEAR_FLAG(tmp, FLAG_UNAGGRESSIVE);
			}

			break;

		/* Calm -- pacify unfriendly monsters */
		case 2:
			if (!QUERY_FLAG(tmp, FLAG_UNAGGRESSIVE))
			{
				SET_FLAG(tmp, FLAG_UNAGGRESSIVE);
			}

			break;

		/* Make all monsters fall asleep */
		case 3:
			if (!QUERY_FLAG(tmp, FLAG_SLEEP))
			{
				SET_FLAG(tmp, FLAG_SLEEP);
			}

			break;

		default:
			break;
	}
}

/**
 * Checks object and its inventory for specific item.
 *
 * It will descend through containers to find the object.
 *
 * - slaying = match object slaying
 * - race = match object archetype name
 * - hp = match object type
 * @param op Object of which to search inventory
 * @param trig What to search
 * @return Object that matches, or NULL if none matched. */
object *check_inv_recursive(object *op, const object *trig)
{
	object *tmp, *ret = NULL;

	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (tmp->inv && !IS_SYS_INVISIBLE(tmp))
		{
			ret = check_inv_recursive(tmp, trig);

			if (ret)
			{
				return ret;
			}
		}
		else if ((trig->stats.hp && tmp->type == trig->stats.hp) || (trig->slaying && trig->stats.sp ? (tmp->slaying && trig->slaying == tmp->slaying) : (tmp->name && trig->slaying == tmp->name)) || (trig->race && trig->race == tmp->arch->name))
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Function to search the inventory of a player and then based on a set
 * of conditions, the square will activate connected items.
 *
 * Monsters can't trigger this square (for now)
 * Values are:   last_sp = 1/0 obj/no obj triggers
 *               last_heal = 1/0  remove/dont remove obj if triggered
 * @param op Object to check. Must be a player.
 * @param trig Trigger object that may be activated. */
void check_inv(object *op, object *trig)
{
	object *match;

	if (op->type != PLAYER)
	{
		return;
	}

	match = check_inv_recursive(op, trig);

	if (match && trig->last_sp)
	{
		if (trig->last_heal)
		{
			decrease_ob(match);
		}

		use_trigger(trig);
	}
	else if (!match && !trig->last_sp)
	{
		use_trigger(trig);
	}
}

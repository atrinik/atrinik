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
 * Buttons, gates, handles, etc related code. */

#include <global.h>

static void trigger_move(object *op, int state);
static objectlink *get_button_links(object *button);

void connection_trigger(object *op, int state)
{
	object *tmp;
	objectlink *ol;

	for (ol = get_button_links(op); ol; ol = ol->next)
	{
		tmp = ol->objlink.ob;

		/* a button link object can become freed when the map is saving.  As
		 * a map is saved, objects are removed and freed, and if an object is
		 * on top of a button, this function is eventually called.  If a map
		 * is getting moved out of memory, the status of buttons and levers
		 * probably isn't important - it will get sorted out when the map is
		 * re-loaded.  As such, just exit this function if that is the case. */
		if (!OBJECT_ACTIVE(tmp))
		{
			LOG(llevDebug, "push_button: button link with invalid object! (%x - %x)", QUERY_FLAG(tmp, FLAG_REMOVED), tmp->count);
			return;
		}

		/* If the criteria isn't appropriate, don't do anything. */
		if (state && QUERY_FLAG(tmp, FLAG_CONNECT_NO_PUSH))
		{
			continue;
		}

		if (!state && QUERY_FLAG(tmp, FLAG_CONNECT_NO_RELEASE))
		{
			continue;
		}

		if (HAS_EVENT(tmp, EVENT_TRIGGER) && trigger_event(EVENT_TRIGGER, tmp, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
		{
			continue;
		}

		switch (tmp->type)
		{
			case GATE:
			case PIT:
				tmp->value = tmp->stats.maxsp ? !state : state;
				tmp->speed = 0.5;
				update_ob_speed(tmp);
				break;

			case HANDLE:
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + (tmp->value = tmp->stats.maxsp ? !state : state));
				update_object(tmp, UP_OBJ_FACE);
				break;

			case ALTAR:
				tmp->value = 1;
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
				update_object(tmp, UP_OBJ_FACE);
				break;

			case BUTTON:
			case PEDESTAL:
				tmp->value = state;
				SET_ANIMATION(tmp, ((NUM_ANIMATIONS(tmp) / NUM_FACINGS(tmp)) * tmp->direction) + tmp->value);
				update_object(tmp, UP_OBJ_FACE);
				break;

			case TIMED_GATE:
				tmp->speed = tmp->arch->clone.speed;
				/* original values */
				update_ob_speed(tmp);
				tmp->value = 1;
				tmp->stats.sp = 1;
				tmp->stats.hp = tmp->stats.maxhp;
				break;

			case DIRECTOR:
				/* next direction */
				if (tmp->stats.maxsp)
				{
					tmp->direction = absdir(tmp->direction + tmp->stats.maxsp);
					animate_turning(tmp);
				}

				break;

			default:
				object_trigger(tmp, op, state);
		}
	}
}

/**
 * Push the specified object. This can affect other buttons/gates/handles
 * altars/pedestals/holes on the whole map.
 * @param op The object to push. */
void push_button(object *op)
{
	connection_trigger(op, op->value);
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

				if (QUERY_FLAG(tmp, FLAG_CONNECT_RESET))
				{
					tmp->value = 0;
				}
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

				if (QUERY_FLAG(tmp, FLAG_CONNECT_RESET))
				{
					tmp->value = 0;
				}
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
		push_button(op);
	}

	if (op->value && QUERY_FLAG(op, FLAG_CONNECT_RESET))
	{
		op->value = 0;
		SET_ANIMATION(op, ((NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction) + op->value);
		update_object(op, UP_OBJ_FACE);
	}
}

/**
 * Updates every button on the map by calling update_button()
 * for them.
 * @param m The map to update buttons for */
void update_buttons(mapstruct *m)
{
	objectlink *ol, *obp;

	for (obp = m->buttons; obp; obp = obp->next)
	{
		for (ol = obp->objlink.link; ol; ol = ol->next)
		{
			if (!ol->objlink.ob || ol->objlink.ob->count != ol->id)
			{
				LOG(llevBug, "Internal error in update_button (%s (%dx%d):%d, connected %ld ).\n", ol->objlink.ob ? ol->objlink.ob->name : "null", ol->objlink.ob ? ol->objlink.ob->x:-1, ol->objlink.ob ? ol->objlink.ob->y:-1, ol->id, obp->value);
				continue;
			}

			if (ol->objlink.ob->type == BUTTON || ol->objlink.ob->type == PEDESTAL)
			{
				update_button(ol->objlink.ob);
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
		LOG(llevBug, "Tried to add button-link without map.\n");
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
		LOG(llevBug, "remove_button_link(): Object without map.\n");
		return;
	}

	if (!QUERY_FLAG(op, FLAG_IS_LINKED))
	{
		LOG(llevBug, "remove_button_link(): Unlinked object.\n");
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

	LOG(llevBug, "remove_button_link(): Couldn't find object.\n");
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

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
 * Connection system handling. */

#include <global.h>

/**
 * Creates a new connection.
 * @param op Object to connect.
 * @param map Map to create the connection on.
 * @param connected Connection ID of the object. */
void connection_object_add(object *op, mapstruct *map, int connected)
{
	objectlink *ol2, *ol;

	if (!op || !map)
	{
		return;
	}

	/* Remove previous connection if any. */
	if (QUERY_FLAG(op, FLAG_IS_LINKED))
	{
		connection_object_remove(op);
	}

	SET_FLAG(op, FLAG_IS_LINKED);
	op->path_attuned = connected;

	for (ol = map->buttons; ol; ol = ol->next)
	{
		if (ol->value == connected)
		{
			break;
		}
	}

	ol2 = get_objectlink();
	ol2->objlink.ob = op;
	ol2->id = op->count;

	/* Link to the existing one. */
	if (ol)
	{
		ol2->next = ol->objlink.link;
		ol->objlink.link = ol2;
	}
	/* Create new. */
	else
	{
		ol = get_objectlink();
		ol->value = connected;

		ol->next = map->buttons;
		map->buttons = ol;
		ol->objlink.link = ol2;
	}
}

/**
 * Remove a connection.
 * @param op Object to remove. Must be on a map, and connected. */
void connection_object_remove(object *op)
{
	objectlink *ol, **ol2, *tmp;

	if (!op->map)
	{
		LOG(llevBug, "connection_object_remove(): Object without map.\n");
		return;
	}

	if (!QUERY_FLAG(op, FLAG_IS_LINKED))
	{
		LOG(llevBug, "connection_object_remove(): Unlinked object.\n");
		return;
	}

	for (ol = op->map->buttons; ol; ol = ol->next)
	{
		for (ol2 = &ol->objlink.link; (tmp = *ol2); ol2 = &tmp->next)
		{
			if (tmp->objlink.ob == op)
			{
				*ol2 = tmp->next;
				free_objectlink_simple(ol);
				return;
			}
		}
	}

	LOG(llevBug, "connection_object_remove(): Couldn't find object.\n");
	CLEAR_FLAG(op, FLAG_IS_LINKED);
}

/**
 * Acquire the connection ID of the specified object.
 * @param op Object to get the connection ID of.
 * @return Connection ID, or 0 if not connected. */
int connection_object_get_value(object *op)
{
	if (!op || !op->map || !QUERY_FLAG(op, FLAG_IS_LINKED))
	{
		return 0;
	}

	return op->path_attuned;
}

/**
 * Return the first objectlink in the objects linked to this one.
 * @param op Object to get the link for.
 * @return ::objectlink for this object, or NULL. */
static objectlink *connection_object_links(object *op)
{
	objectlink *ol, *ol2;

	if (!op || !op->map)
	{
		return NULL;
	}

	for (ol = op->map->buttons; ol; ol = ol->next)
	{
		for (ol2 = ol->objlink.link; ol2; ol2 = ol2->next)
		{
			if (ol2->objlink.ob == op && ol2->id == op->count)
			{
				return ol->objlink.link;
			}
		}
	}

	return NULL;
}

/**
 * Trigger an object.
 * @param op The object.
 * @param state The trigger state. */
void connection_trigger(object *op, int state)
{
	objectlink *ol;
	object *tmp;

	for (ol = connection_object_links(op); ol; ol = ol->next)
	{
		tmp = ol->objlink.ob;

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
				tmp->value = tmp->stats.maxsp ? !state : state;
				tmp->speed = 0.5;
				update_ob_speed(tmp);
				break;

			case DIRECTOR:
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
 * Trigger a button-like object.
 * @param op The button-like object.
 * @param state The trigger state. */
void connection_trigger_button(object *op, int state)
{
	objectlink *ol;
	object *tmp;
	sint64 old_state;
	uint8 down;

	old_state = op->value;
	down = 0;

	for (ol = connection_object_links(op); ol; ol = ol->next)
	{
		tmp = ol->objlink.ob;

		if (object_trigger_button(tmp, op, state) == OBJECT_METHOD_OK)
		{
			if (tmp->value)
			{
				down = 1;

				if (QUERY_FLAG(tmp, FLAG_CONNECT_RESET))
				{
					tmp->value = 0;
				}
			}
		}
	}

	if (down)
	{
		op->value = down;
	}

	if (op->value != old_state)
	{
		connection_trigger(op, op->value);
	}

	if (op->value && QUERY_FLAG(op, FLAG_CONNECT_RESET))
	{
		op->value = 0;
	}
}

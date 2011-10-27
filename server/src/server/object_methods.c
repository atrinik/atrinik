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
 * Object methods system. */

#include <global.h>

/**
 * Registered method handlers. */
object_methods object_type_methods[OBJECT_TYPE_MAX];

object_methods object_methods_base;

static int object_move_on_recursion_depth = 0;

void object_methods_init_one(object_methods *methods)
{
	memset(methods, 0, sizeof(*methods));
}

/**
 * Initializes the object methods system. */
void object_methods_init(void)
{
	size_t i;

	object_methods_init_one(&object_methods_base);
	object_methods_base.apply_func = common_object_apply;
	object_methods_base.describe_func = common_object_describe;

	for (i = 0; i < arraysize(object_type_methods); i++)
	{
		object_methods_init_one(&object_type_methods[i]);
		object_type_methods[i].fallback = &object_methods_base;
	}

	object_type_init_book();
	object_type_init_pit();
	object_type_init_rod();
	object_type_init_spawn_point();
}

/** @copydoc object_methods::apply_func */
int object_apply(object *op, object *applier, int aflags)
{
	object_methods *methods;

	for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback)
	{
		if (methods->apply_func)
		{
			return methods->apply_func(op, applier, aflags);
		}
	}

	return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::process_func */
void object_process(object *op)
{
	object_methods *methods;

	/* No need to process objects inside creators. */
	if (op->env && op->env->type == CREATOR)
	{
		return;
	}

	if (common_object_process(op))
	{
		return;
	}

	if (HAS_EVENT(op, EVENT_TIME))
	{
		if (trigger_event(EVENT_TIME, NULL, op, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
		{
			return;
		}
	}

	for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback)
	{
		if (methods->process_func)
		{
			methods->process_func(op);
		}
	}
}

/** @copydoc object_methods::describe_func */
char *object_describe(object *op, object *observer, char *buf, size_t size)
{
	object_methods *methods;

	for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback)
	{
		if (methods->describe_func)
		{
			methods->describe_func(op, observer, buf, size);
			return buf;
		}
	}

	buf[0] = '\0';
	return buf;
}

/** @copydoc object_methods::move_on_func */
int object_move_on(object *op, object *victim, object *originator)
{
	object_methods *methods;
	int ret;

	op = HEAD(op);

	if (trigger_event(EVENT_TRIGGER, victim, op, originator, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return OBJECT_METHOD_OK;
	}

	for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback)
	{
		if (methods->move_on_func)
		{
			if (object_move_on_recursion_depth >= 500)
			{
				LOG(llevDebug, "object_move_on(): Aborting recursion [op arch %s, name %s; victim arch %s, name %s]\n", op->arch->name, op->name, victim->arch->name, victim->name);
				return OBJECT_METHOD_OK;
			}

			object_move_on_recursion_depth++;
			ret = methods->move_on_func(op, victim, originator);
			object_move_on_recursion_depth--;

			return ret;
		}
	}

	return OBJECT_METHOD_UNHANDLED;
}

/** @copydoc object_methods::trigger_func */
int object_trigger(object *op, object *cause, int state)
{
	object_methods *methods;

	for (methods = &object_type_methods[op->type]; methods; methods = methods->fallback)
	{
		if (methods->trigger_func)
		{
			return methods->trigger_func(op, cause, state);
		}
	}

	return OBJECT_METHOD_UNHANDLED;
}

/**
 * An item (::ARROW or such) stops moving.
 *
 * This function assumes that only items on maps need special treatment.
 *
 * If the object can't be stopped, or it was destroyed while trying to
 * stop it, NULL is returned.
 * @param op Object to check.
 * @return Pointer to stopped object, NULL if destroyed or can't be
 * stopped. */
object *stop_item(object *op)
{
	if (op->map == NULL)
	{
		return op;
	}

	switch (op->type)
	{
		case THROWN_OBJ:
		{
			object *payload = op->inv;

			if (payload == NULL)
			{
				return NULL;
			}

			remove_ob(payload);
			remove_ob(op);
			return payload;
		}

		case ARROW:
			if (op->speed >= MIN_ACTIVE_SPEED)
			{
				op = fix_stopped_arrow(op);
			}

			return op;

		case CONE:
			if (op->speed < MIN_ACTIVE_SPEED)
			{
				return op;
			}
			else
			{
				return NULL;
			}

		default:
			return op;
	}
}

/**
 * Put stopped item where stop_item() had found it.
 * Inserts item into the old map, or merges it if it already is on the
 * map.
 * @param op Object to stop.
 * @param map Must be the value of op->map before stop_item() was called.
 * @param originator What caused op to be stopped. */
void fix_stopped_item(object *op, mapstruct *map, object *originator)
{
	if (map == NULL)
	{
		return;
	}

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		insert_ob_in_map(op, map, originator, 0);
	}
	else if (op->type == ARROW)
	{
		/* Only some arrows actually need this. */
		merge_ob(op, NULL);
	}
}

/**
 * Move for ::FIREWALL.
 *
 * Firewalls fire other spells. The direction of the wall is stored in
 * op->stats.dam.
 * @param op Firewall. */
void move_firewall(object *op)
{
	/* DM has created a firewall in his inventory or no legal spell
	 * selected. */
	if (!op->map || !op->last_eat || op->stats.dam == -1)
	{
		return;
	}

	cast_spell(op, op, op->direction, op->stats.dam, 1, CAST_NPC, NULL);
}

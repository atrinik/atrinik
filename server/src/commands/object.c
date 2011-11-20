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
 * Object ID parsing functions */

#include <global.h>

/**
 * Search the inventory of 'pl' for what matches best with params.
 * We use item_matched_string above - this gives us consistent behavior
 * between many commands.
 * @param pl Player object.
 * @param params Parameters string.
 * @return Best match, or NULL if no match. */
object *find_best_object_match(object *pl, char *params)
{
	object *tmp, *best = NULL;
	int match_val = 0, tmpmatch;

	for (tmp = pl->inv; tmp; tmp = tmp->below)
	{
		if (IS_SYS_INVISIBLE(tmp))
		{
			continue;
		}

		if ((tmpmatch = item_matched_string(pl, tmp, params)) > match_val)
		{
			match_val = tmpmatch;
			best = tmp;
		}
	}

	return best;
}


/**
 * Use skill command.
 * @param pl Player object.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_uskill(object *pl, char *params)
{
	if (!params)
	{
		draw_info(COLOR_WHITE, pl, "Usage: /use_skill <skill name>");
		return 0;
	}

	if (pl->type == PLAYER)
	{
		CONTR(pl)->praying = 0;
	}

	return use_skill(pl, params);
}

/**
 * Ready skill command.
 * @param pl Player object.
 * @param params Command parameters.
 * @return 1 on success, 0 on failure. */
int command_rskill(object *pl, char *params)
{
	int skillno;

	if (!params)
	{
		draw_info(COLOR_WHITE, pl, "Usage: /ready_skill <skill name>");
		return 0;
	}

	if (pl->type == PLAYER)
	{
		CONTR(pl)->praying = 0;
	}

	skillno = lookup_skill_by_name(params);

	if (skillno == -1)
	{
		draw_info_format(COLOR_WHITE, pl, "Couldn't find the skill %s", params);
		return 0;
	}

	return change_skill(pl, skillno);
}

/**
 * Apply an item. Accepts parameters like -a to always apply, and -u to
 * always unapply.
 * @param op Object requesting this.
 * @param params Command parameters.
 * @return Always returns 0. */
int command_apply(object *op, char *params)
{
	if (op->type == PLAYER)
		CONTR(op)->praying = 0;

	if (!params)
	{
		player_apply_below(op);
		return 0;
	}
	else
	{
		enum apply_flag aflag = 0;
		object *inv;

		while (*params == ' ')
		{
			params++;
		}

		if (!strncmp(params, "-a ", 3))
		{
			aflag = AP_APPLY;
			params += 3;
		}

		if (!strncmp(params, "-u ", 3))
		{
			aflag = AP_UNAPPLY;
			params += 3;
		}

		while (*params == ' ')
		{
			params++;
		}

		inv = find_best_object_match(op, params);

		if (inv)
		{
			player_apply(op, inv, aflag, 0);
		}
		else
		{
			draw_info_format(COLOR_WHITE, op, "Could not find any match to the %s.", params);
		}
	}

	return 0;
}

/**
 * Check if an item op can be put into a sack. If pl exists then tell
 * a player the reason of failure.
 * @param pl Player object.
 * @param sack The sack.
 * @param op The object to check.
 * @param nrof Number of objects we want to put in.
 * @return 1 if the object will fit, 0 if it will not. */
int sack_can_hold(object *pl, object *sack, object *op, int nrof)
{
	char buf[MAX_BUF];

	buf[0] = '\0';

	if (!QUERY_FLAG(sack, FLAG_APPLIED))
	{
		snprintf(buf, sizeof(buf), "The %s is not active.", query_name(sack, NULL));
	}

	if (sack == op)
	{
		snprintf(buf, sizeof(buf), "You can't put the %s into itself.", query_name(sack, NULL));
	}

	if ((sack->race && (sack->sub_type & 1) != ST1_CONTAINER_CORPSE) && (sack->race != op->race || op->type == CONTAINER || (sack->stats.food && sack->stats.food != op->type)))
	{
		snprintf(buf, sizeof(buf), "You can put only %s into the %s.", sack->race, query_name(sack, NULL));
	}

	if (sack->weight_limit && sack->carrying + (sint32) ((float) (((nrof ? nrof : 1) * op->weight) + op->carrying) * sack->weapon_speed) > (sint32) sack->weight_limit)
	{
		snprintf(buf, sizeof(buf), "That won't fit in the %s!", query_name(sack, NULL));
	}

	if (buf[0])
	{
		if (pl)
		{
			draw_info(COLOR_WHITE, pl, buf);
		}

		return 0;
	}

	return 1;
}

static object *get_pickup_object(object *pl, object *op, int nrof)
{
	if (QUERY_FLAG(op, FLAG_UNPAID) && QUERY_FLAG(op, FLAG_NO_PICK))
	{
		op = object_create_clone(op);
		CLEAR_FLAG(op, FLAG_NO_PICK);
		SET_FLAG(op, FLAG_STARTEQUIP);
		op->nrof = nrof;

		draw_info_format(COLOR_WHITE, pl, "You pick up %s for %s from the storage.", query_name(op, NULL), query_cost_string(op, pl, COST_BUY));
	}
	else
	{
		op = object_stack_get_removed(op, nrof);

		if (QUERY_FLAG(op, FLAG_UNPAID))
		{
			draw_info_format(COLOR_WHITE, pl, "%s will cost you %s.", query_name(op, NULL), query_cost_string(op, pl, COST_BUY));
		}
		else
		{
			draw_info_format(COLOR_WHITE, pl, "You pick up the %s.", query_name(op, NULL));
		}
	}

	return op;
}

/**
 * Pick up object.
 * @param pl Object that is picking up the object.
 * @param op Object to put tmp into.
 * @param tmp Object to pick up.
 * @param nrof Number to pick up (0 means all of them).
 * @param no_mevent If 1, no map-wide pickup event will be triggered. */
static void pick_up_object(object *pl, object *op, object *tmp, int nrof, int no_mevent)
{
	int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

	if (pl->type == PLAYER)
	{
		CONTR(pl)->praying = 0;
	}

	/* IF the player is flying & trying to take the item out of a container
	 * that is in his inventory, let him.  tmp->env points to the container
	 * (sack, luggage, etc), tmp->env->env then points to the player (nested
	 * containers not allowed as of now) */
	if (QUERY_FLAG(pl, FLAG_FLYING) && !QUERY_FLAG(pl, FLAG_WIZ) && is_player_inv(tmp) != pl)
	{
		draw_info(COLOR_WHITE, pl, "You are levitating, you can't reach the ground!");
		return;
	}

	if (nrof > tmp_nrof || nrof == 0)
	{
		nrof = tmp_nrof;
	}

	if (!player_can_carry(pl, WEIGHT_NROF(tmp, nrof)))
	{
		draw_info(COLOR_WHITE, pl, "That item is too heavy for you to pick up.");
		return;
	}

	if (tmp->type == CONTAINER)
	{
		container_close(NULL, tmp);
	}

	/* Trigger the PICKUP event */
	if (trigger_event(EVENT_PICKUP, pl, tmp, op, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return;
	}

	/* Trigger the map-wide pick up event. */
	if (!no_mevent && pl->map && pl->map->events && trigger_map_event(MEVENT_PICK, pl->map, pl, tmp, op, NULL, nrof))
	{
		return;
	}

#ifndef REAL_WIZ
	if (QUERY_FLAG(pl, FLAG_WAS_WIZ))
	{
		SET_FLAG(tmp, FLAG_WAS_WIZ);
	}
#endif

	if (pl->type == PLAYER)
	{
		CONTR(pl)->stat_items_picked++;
	}

	tmp = get_pickup_object(pl, tmp, nrof);
	insert_ob_in_ob(tmp, op);
}

/**
 * Try to pick up an item.
 * @param op Object trying to pick up.
 * @param alt Optional object op is trying to pick. If NULL, try to pick
 * first item under op.
 * @param no_mevent If 1, no map-wide pickup event will be triggered. */
void pick_up(object *op, object *alt, int no_mevent)
{
	int need_fix_tmp = 0, count;
	object *tmp = NULL;
	mapstruct *tmp_map = NULL;
	tag_t tag;

	/* Decide which object to pick. */
	if (alt)
	{
		if (!can_pick(op, alt))
		{
			draw_info_format(COLOR_WHITE, op, "You can't pick up %s.", alt->name);
			goto leave;
		}

		tmp = alt;
	}
	else
	{
		if (op->below == NULL || !can_pick(op, op->below))
		{
			draw_info(COLOR_WHITE, op, "There is nothing to pick up here.");
			goto leave;
		}

		tmp = op->below;
	}

	if (tmp->type == CONTAINER)
	{
		container_close(NULL, tmp);
	}

	/* Try to catch it. */
	tmp_map = tmp->map;
	tmp = stop_item(tmp);

	if (tmp == NULL)
	{
		goto leave;
	}

	need_fix_tmp = 1;

	if (!can_pick(op, tmp))
	{
		goto leave;
	}

	if (op->type == PLAYER)
	{
		count = CONTR(op)->count;

		if (count == 0)
		{
			count = tmp->nrof;
		}
	}
	else
	{
		count = tmp->nrof;
	}

	/* Container is open, so use it */
	if (op->type == PLAYER && CONTR(op)->container)
	{
		alt = CONTR(op)->container;

		if (alt != tmp->env && !sack_can_hold(op, alt, tmp, count) && !check_magical_container(tmp, alt))
		{
			goto leave;
		}
	}
	/* Con container pickup */
	else
	{
		for (alt = op->inv; alt; alt = alt->below)
		{
			if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && alt->race && alt->race == tmp->race && sack_can_hold(NULL, alt, tmp, count) && !check_magical_container(tmp, alt))
			{
				/* Perfect match */
				break;
			}
		}

		if (!alt)
		{
			for (alt = op->inv; alt; alt = alt->below)
			{
				if (alt->type == CONTAINER && QUERY_FLAG(alt, FLAG_APPLIED) && sack_can_hold(NULL, alt, tmp, count) && !check_magical_container(tmp, alt))
				{
					/* General container comes next */
					break;
				}
			}
		}

		/* No free containers */
		if (!alt)
		{
			alt = op;
		}
	}

	if (tmp->env == alt)
	{
		alt = op;
	}

	/* Startequip items are not allowed to be put into containers. */
	if (op->type == PLAYER && alt->type == CONTAINER && QUERY_FLAG(tmp, FLAG_STARTEQUIP))
	{
		draw_info(COLOR_WHITE, op, "This object cannot be put into containers!");
		goto leave;
	}

	tag = tmp->count;
	pick_up_object(op, alt, tmp, count, no_mevent);

	if (was_destroyed(tmp, tag) || tmp->env)
	{
		need_fix_tmp = 0;
	}

	if (op->type == PLAYER)
	{
		CONTR(op)->count = 0;
	}

	goto leave;

leave:
	if (need_fix_tmp)
	{
		fix_stopped_item(tmp, tmp_map, op);
	}
}

/**
 * Player tries to put object into sack, if nrof is non zero, then
 * nrof objects is tried to put into sack.
 * @param op Player object.
 * @param sack The sack.
 * @param tmp The object to put into sack.
 * @param nrof Number of items to put into sack (0 for all). */
void put_object_in_sack(object *op, object *sack, object *tmp, long nrof)
{
	char buf[MAX_BUF];
	int tmp_nrof = tmp->nrof ? tmp->nrof : 1;

	if (op->type != PLAYER)
	{
		LOG(llevDebug, "put_object_in_sack: op not a player.\n");
		return;
	}

	/* Can't put an object in itself */
	if (sack == tmp)
	{
		return;
	}

	if (sack->type != CONTAINER)
	{
		draw_info_format(COLOR_WHITE, op, "The %s is not a container.", query_name(sack, NULL));
		return;
	}

	if (check_magical_container(tmp, sack))
	{
		draw_info(COLOR_WHITE, op, "You can't put a magical container into another magical container.");
		return;
	}

	if (tmp->map && sack->env)
	{
		if (trigger_event(EVENT_PICKUP, op, tmp, sack, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR))
		{
			return;
		}
	}

	/* Trigger the map-wide put event. */
	if (op->map && op->map->events && trigger_map_event(MEVENT_PUT, op->map, op, tmp, sack, NULL, nrof))
	{
		return;
	}

	if (tmp->type == CONTAINER)
	{
		container_close(NULL, tmp);
	}

	if (nrof > tmp_nrof || nrof == 0)
	{
		nrof = tmp_nrof;
	}

	if (!sack_can_hold(op, sack, tmp, nrof))
	{
		return;
	}

	if (QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		if (object_apply_item(tmp, op, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK)
		{
			return;
		}
	}

	tmp = get_pickup_object(op, tmp, nrof);

	snprintf(buf, sizeof(buf), "You put the %s in %s.", query_name(tmp, NULL), query_name(sack, NULL));
	insert_ob_in_ob(tmp, sack);
	draw_info(COLOR_WHITE, op, buf);
	/* This is overkill, fix_player() is called somewhere in object.c */
	fix_player(op);
}

/**
 * Drop an object onto the floor.
 * @param op Player object.
 * @param tmp The object to drop.
 * @param nrof Number of items to drop (0 for all).
 * @param no_mevent If 1, no map-wide event will be triggered. */
void drop_object(object *op, object *tmp, long nrof, int no_mevent)
{
	object *floor_ob;

	if (QUERY_FLAG(tmp, FLAG_NO_DROP) && !QUERY_FLAG(op, FLAG_WIZ))
	{
		draw_info(COLOR_WHITE, op, "You can't drop that item.");
		return;
	}

	/* Trigger the map-wide drop event. */
	if (!no_mevent && op->map && op->map->events && trigger_map_event(MEVENT_DROP, op->map, op, tmp, NULL, NULL, nrof))
	{
		return;
	}

	/* Stop praying. */
	if (op->type == PLAYER)
	{
		CONTR(op)->praying = 0;
	}

	if (QUERY_FLAG(tmp, FLAG_APPLIED))
	{
		/* Can't unapply it */
		if (object_apply_item(tmp, op, AP_UNAPPLY | AP_NO_MERGE) != OBJECT_METHOD_OK)
		{
			return;
		}
	}

	if (tmp->type == CONTAINER)
	{
		container_close(NULL, tmp);
	}

	/* Trigger the DROP event */
	if (trigger_event(EVENT_DROP, op, tmp, NULL, NULL, nrof, 0, 0, SCRIPT_FIX_ACTIVATOR))
	{
		return;
	}

	tmp = object_stack_get_removed(tmp, nrof);

	if (op->type == PLAYER)
	{
		CONTR(op)->stat_items_dropped++;
	}

	if (QUERY_FLAG(tmp, FLAG_STARTEQUIP) || QUERY_FLAG(tmp, FLAG_UNPAID))
	{
		if (op->type == PLAYER)
		{
			draw_info_format(COLOR_WHITE, op, "You drop the %s.", query_name(tmp, NULL));

			if (QUERY_FLAG(tmp, FLAG_UNPAID))
			{
				draw_info(COLOR_WHITE, op, "The shop magic put it back to the storage.");

				floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

				/* If the player is standing on a unique shop floor or unique randomitems shop floor, drop the object back to the floor */
				if (floor_ob && floor_ob->type == SHOP_FLOOR && (QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL) || (floor_ob->randomitems && QUERY_FLAG(floor_ob, FLAG_CURSED))))
				{
					tmp->x = op->x;
					tmp->y = op->y;
					insert_ob_in_map(tmp, op->map, op, 0);
				}
			}
			else
			{
				draw_info(COLOR_WHITE, op, "The god-given item vanishes to nowhere as you drop it!");
			}
		}

		fix_player(op);
		return;
	}

	/* If SAVE_INTERVAL is commented out, we never want to save
	 * the player here. */
#ifdef SAVE_INTERVAL
	if (op->type == PLAYER && !QUERY_FLAG(tmp, FLAG_UNPAID) && (tmp->nrof ? tmp->value * tmp->nrof : tmp->value > 2000) && (CONTR(op)->last_save_time + SAVE_INTERVAL) <= time(NULL))
	{
		save_player(op, 1);
		CONTR(op)->last_save_time = time(NULL);
	}
#endif

	floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

	if (floor_ob && floor_ob->type == SHOP_FLOOR && !QUERY_FLAG(tmp, FLAG_UNPAID) && tmp->type != MONEY)
	{
		sell_item(tmp, op, -1);

		/* Ok, we have really sold it - not only dropped. Run this only
		 * if the floor is not magical (i.e., unique shop) */
		if (QUERY_FLAG(tmp, FLAG_UNPAID) && !QUERY_FLAG(floor_ob, FLAG_IS_MAGICAL))
		{
			if (op->type == PLAYER)
			{
				draw_info(COLOR_WHITE, op, "The shop magic put it to the storage.");
			}

			fix_player(op);
			return;
		}
	}

	tmp->x = op->x;
	tmp->y = op->y;

	insert_ob_in_map(tmp, op->map, op, 0);

	SET_FLAG(op, FLAG_NO_APPLY);
	object_remove(op, 0);
	insert_ob_in_map(op, op->map, op, INS_NO_MERGE | INS_NO_WALK_ON);
	CLEAR_FLAG(op, FLAG_NO_APPLY);

	/* Need to update the weight for the player */
	if (op->type == PLAYER)
	{
		fix_player(op);
	}
}

/**
 * Drop an item, either on the floor or in a container.
 * @param op Who is dropping an item.
 * @param tmp What object to drop.
 * @param no_mevent If 1, no drop map-wide event will be triggered. */
void drop(object *op, object *tmp, int no_mevent)
{
	if (tmp == NULL)
	{
		draw_info(COLOR_WHITE, op, "You don't have anything to drop.");
		return;
	}

	if (QUERY_FLAG(tmp, FLAG_INV_LOCKED))
	{
		draw_info(COLOR_WHITE, op, "This item is locked.");
		return;
	}

	if (QUERY_FLAG(tmp, FLAG_NO_DROP))
	{
		draw_info(COLOR_WHITE, op, "You can't drop that item.");
		return;
	}

	if (op->type == PLAYER)
	{
		if (CONTR(op)->container)
		{
			put_object_in_sack(op, CONTR(op)->container, tmp, CONTR(op)->count);
		}
		else
		{
			drop_object(op, tmp, CONTR(op)->count, no_mevent);
		}

		CONTR(op)->count = 0;
	}
	else
	{
		drop_object(op, tmp, 0, no_mevent);
	}
}

/**
 * /take command.
 * @param op Player.
 * @param params What to take.
 * @return 0. */
int command_take(object *op, char *params)
{
	object *tmp, *next;
	int did_one = 0, missed = 0, ground_total = 0, ival;

	if (!params)
	{
		draw_info(COLOR_WHITE, op, "Take what?");
		return 0;
	}

	if (CONTR(op)->container)
	{
		tmp = CONTR(op)->container->inv;
	}
	else
	{
		tmp = GET_MAP_OB_LAST(op->map, op->x, op->y);
	}

	if (!tmp)
	{
		draw_info(COLOR_WHITE, op, "Nothing to take.");
		return 0;
	}

	if (op->map && op->map->events && trigger_map_event(MEVENT_CMD_TAKE, op->map, op, tmp, NULL, params, 0))
	{
		return 0;
	}

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);

	for ( ; tmp; tmp = next)
	{
		next = tmp->below;

		if ((tmp->layer != LAYER_ITEM && tmp->layer != LAYER_ITEM2) || QUERY_FLAG(tmp, FLAG_NO_PICK) || IS_SYS_INVISIBLE(tmp))
		{
			continue;
		}

		ival = item_matched_string(op, tmp, params);

		if (ival > 0)
		{
			if (ival <= 2 && !can_pick(op, tmp))
			{
				missed++;
			}
			else
			{
				pick_up(op, tmp, 1);
				did_one = 1;
			}
		}

		/* Keep track how many visible objects are left on the ground. */
		if (!CONTR(op)->container && !tmp->env)
		{
			if (!(tmp->layer <= LAYER_FMASK || IS_SYS_INVISIBLE(tmp) || (!QUERY_FLAG(op, FLAG_SEE_INVISIBLE) && QUERY_FLAG(tmp, FLAG_IS_INVISIBLE))) || QUERY_FLAG(op, FLAG_WIZ))
			{
				ground_total++;
			}
		}
	}

	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

	if (did_one)
	{
		fix_player(op);

		/* Update below inventory positions for all players on this tile. */
		for (tmp = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_LIVING, 0); tmp && tmp->layer == LAYER_LIVING; tmp = tmp->above)
		{
			/* Ensures the below inventory position is not higher than
			 * the actual number of visible items on the tile. */
			if (tmp->type == PLAYER && CONTR(tmp) && CONTR(tmp)->socket.look_position > ground_total)
			{
				/* Update the visible row of objects. */
				CONTR(tmp)->socket.look_position = ((int) (((float) ground_total / NUM_LOOK_OBJECTS) - 0.5f)) * NUM_LOOK_OBJECTS;
			}
		}
	}
	else if (!missed)
	{
		draw_info(COLOR_WHITE, op, "Nothing to take.");
	}

	if (missed == 1)
	{
		draw_info(COLOR_WHITE, op, "You were unable to take one of the items.");
	}
	else if (missed > 1)
	{
		draw_info_format(COLOR_WHITE, op, "You were unable to take %d of the items.", missed);
	}

	if (op->type == PLAYER)
	{
		CONTR(op)->count = 0;
	}

	return 0;
}

/**
 * /drop command.
 * @param op Player.
 * @param params What to drop.
 * @return 0. */
int command_drop(object *op, char *params)
{
	object *tmp, *next;
	int did_one = 0, missed = 0, ival;

	if (!params)
	{
		draw_info(COLOR_WHITE, op, "Drop what?");
		return 0;
	}

	if (op->map && op->map->events && trigger_map_event(MEVENT_CMD_DROP, op->map, op, NULL, NULL, params, 0))
	{
		return 0;
	}

	SET_FLAG(op, FLAG_NO_FIX_PLAYER);

	for (tmp = op->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (QUERY_FLAG(tmp, FLAG_NO_DROP) || QUERY_FLAG(tmp, FLAG_STARTEQUIP) || IS_SYS_INVISIBLE(tmp))
		{
			continue;
		}

		ival = item_matched_string(op, tmp, params);

		if (ival > 0)
		{
			if (ival <= 2 && QUERY_FLAG(tmp, FLAG_INV_LOCKED))
			{
				missed++;
			}
			else
			{
				drop(op, tmp, 1);
				did_one = 1;
			}
		}
	}

	CLEAR_FLAG(op, FLAG_NO_FIX_PLAYER);

	if (did_one)
	{
		fix_player(op);
	}
	else if (!missed)
	{
		draw_info(COLOR_WHITE, op, "Nothing to drop.");
	}

	if (missed == 1)
	{
		draw_info(COLOR_WHITE, op, "One item couldn't be dropped because it was locked.");
	}
	else if (missed > 1)
	{
		draw_info_format(COLOR_WHITE, op, "%d items couldn't be dropped because they were locked.", missed);
	}

	if (op->type == PLAYER)
	{
		CONTR(op)->count = 0;
	}

	return 0;
}

/**
 * Recursive helper function for find_marked_object() to search for
 * marked object in containers.
 * @param op Object. Should be a player.
 * @param marked Marked object.
 * @param marked_count Marked count.
 * @return The object if found, NULL otherwise. */
static object *find_marked_object_rec(object *op, object **marked, uint32 *marked_count)
{
	object *tmp, *tmp2;
	int wiz = QUERY_FLAG(op, FLAG_WIZ);

	/* This may seem like overkill, but we need to make sure that they
	 * player hasn't dropped the item. We use count on the off chance
	 * that an item got reincarnated at some point. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		if (IS_SYS_INVISIBLE(tmp) && !wiz)
		{
			continue;
		}

		if (tmp == *marked)
		{
			if (tmp->count == *marked_count)
			{
				return tmp;
			}
			else
			{
				*marked = NULL;
				*marked_count = 0;
				return NULL;
			}
		}
		else if (tmp->inv)
		{
			tmp2 = find_marked_object_rec(tmp, marked, marked_count);

			if (tmp2)
			{
				return tmp2;
			}

			if (*marked == NULL)
			{
				return NULL;
			}
		}
	}

	return NULL;
}

/**
 * Return the object the player has marked.
 * @param op Object. Should be a player.
 * @return Marked object if still valid, NULL otherwise. */
object *find_marked_object(object *op)
{
	if (op->type != PLAYER || !op || !CONTR(op) || !CONTR(op)->mark)
	{
		return NULL;
	}

	return find_marked_object_rec(op, &CONTR(op)->mark, &CONTR(op)->mark_count);
}

/**
 * Player examines a living object.
 * @param op Player.
 * @param tmp Object being examined. */
static void examine_living(object *op, object *tmp, StringBuffer *sb_capture)
{
	object *mon = tmp->head ? tmp->head : tmp;
	int val, val2, i, gender;

	CONTR(op)->praying = 0;
	gender = object_get_gender(mon);

	if (QUERY_FLAG(mon, FLAG_IS_GOOD))
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is a good aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
	}
	else if (QUERY_FLAG(mon, FLAG_IS_EVIL))
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is an evil aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
	}
	else if (QUERY_FLAG(mon, FLAG_IS_NEUTRAL))
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is a neutral aligned %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
	}
	else
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is a %s %s.", gender_subjective_upper[gender], gender_noun[gender], mon->race);
	}

	draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is level %d.", gender_subjective_upper[gender], mon->level);
	draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s has a base damage of %d and hp of %d.", gender_subjective_upper[gender], mon->stats.dam, mon->stats.maxhp);
	draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s has a wc of %d and ac of %d.", gender_subjective_upper[gender], mon->stats.wc, mon->stats.ac);

	for (val = val2 = -1, i = 0; i < NROFATTACKS; i++)
	{
		if (mon->protection[i] > 0)
		{
			val = i;
		}
		else if (mon->protection[i] < 0)
		{
			val = i;
		}
	}

	if (val != -1)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s can naturally resist some attacks.", gender_subjective_upper[gender]);
	}

	if (val2 != -1)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is naturally vulnerable to some attacks.", gender_subjective_upper[gender]);
	}

	for (val =- 1, val2 = i = 0; i < NROFATTACKS; i++)
	{
		if (mon->protection[i] > val2)
		{
			val = i;
			val2 = mon->protection[i];
		}
	}

	if (val != -1)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "Best armour protection seems to be for %s.", attack_name[val]);
	}

	if (QUERY_FLAG(mon, FLAG_UNDEAD))
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is an undead force.", gender_subjective_upper[gender]);
	}

	switch ((mon->stats.hp + 1) * 4 / (mon->stats.maxhp + 1))
	{
		case 1:
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is in a bad shape.", gender_subjective_upper[gender]);
			break;

		case 2:
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is hurt.", gender_subjective_upper[gender]);
			break;

		case 3:
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is somewhat hurt.", gender_subjective_upper[gender]);
			break;

		default:
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s is in excellent shape.", gender_subjective_upper[gender]);
			break;
	}

	if (present_in_ob(POISONING, mon) != NULL)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s looks very ill.", gender_subjective_upper[gender]);
	}
}

/**
 * Long description of an object.
 * @param tmp Object to get description of.
 * @param caller Caller.
 * @return The returned description. */
char *long_desc(object *tmp, object *caller)
{
	static char buf[VERY_BIG_BUF];
	char *cp;

	if (tmp == NULL)
	{
		return "";
	}

	buf[0] = '\0';

	switch (tmp->type)
	{
		case RING:
		case SKILL:
		case WEAPON:
		case ARMOUR:
		case BRACERS:
		case HELMET:
		case SHIELD:
		case BOOTS:
		case GLOVES:
		case AMULET:
		case GIRDLE:
		case POTION:
		case BOW:
		case ARROW:
		case CLOAK:
		case FOOD:
		case DRINK:
		case HORN:
		case WAND:
		case ROD:
		case FLESH:
		case BOOK:
		case CONTAINER:
			if (*(cp = describe_item(tmp)) != '\0')
			{
				size_t len;

				strncat(buf, query_name(tmp, caller), VERY_BIG_BUF - 1);

				buf[VERY_BIG_BUF - 1] = '\0';
				len = strlen(buf);

				if (len < VERY_BIG_BUF - 5 && ((tmp->type != AMULET && tmp->type != RING) || tmp->title))
				{
					/* Since we know the length, we save a few CPU cycles by using
					 * it instead of calling strcat */
					strcpy(buf + len, " ");
					len++;
					strncpy(buf + len, cp, VERY_BIG_BUF - len - 1);
					buf[VERY_BIG_BUF - 1] = '\0';
				}
			}

			break;
	}

	if (buf[0] == '\0')
	{
		strncat(buf, query_name(tmp, caller), VERY_BIG_BUF - 1);
		buf[VERY_BIG_BUF - 1] = '\0';
	}

	return buf;
}

/**
 * Player examines some object.
 * @param op Player.
 * @param tmp Object to examine. */
void examine(object *op, object *tmp, StringBuffer *sb_capture)
{
	char buf[VERY_BIG_BUF], tmp_buf[64];
	int i;

	if (tmp == NULL)
	{
		return;
	}

	strcpy(buf, "That is ");
	strncat(buf, long_desc(tmp, op), VERY_BIG_BUF - strlen(buf) - 1);
	buf[VERY_BIG_BUF - 1] = '\0';

	/* Only add this for usable items, not for objects like walls or
	 * floors for example. */
	if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED) && need_identify(tmp))
	{
		strncat(buf, " (unidentified)", VERY_BIG_BUF - strlen(buf) - 1);
	}

	buf[VERY_BIG_BUF - 1] = '\0';
	draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);
	buf[0] = '\0';

	if (tmp->custom_name)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "You name it %s.", tmp->custom_name);
	}

	if (QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER)
	{
		draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s.", describe_item(tmp->head ? tmp->head : tmp));
		examine_living(op, tmp, sb_capture);
	}
	/* We don't double use the item_xxx arch commands, so they are always valid */
	else if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
	{
		if (QUERY_FLAG(tmp, FLAG_IS_GOOD))
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "It is good aligned.");
		}
		else if (QUERY_FLAG(tmp, FLAG_IS_EVIL))
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "It is evil aligned.");
		}
		else if (QUERY_FLAG(tmp, FLAG_IS_NEUTRAL))
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "It is neutral aligned.");
		}

		if (tmp->item_level)
		{
			if (tmp->item_skill)
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "It needs a level of %d in %s to use.", tmp->item_level, find_skill_exp_skillname(tmp->item_skill));
			}
			else
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "It needs a level of %d to use.", tmp->item_level);
			}
		}

		if (tmp->item_quality)
		{
			if (QUERY_FLAG(tmp, FLAG_INDESTRUCTIBLE))
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "Qua: %d Con: Indestructible.", tmp->item_quality);
			}
			else
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "Qua: %d Con: %d.", tmp->item_quality, tmp->item_condition);
			}
		}

		buf[0] = '\0';
	}

	switch (tmp->type)
	{
		case BOOK:
			if (tmp->msg)
			{
				strcpy(buf, "Something is written in it.");

				if (op->type == PLAYER && !QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT))
				{
					int level = CONTR(op)->skill_ptr[SK_LITERACY]->level;

					draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);

					/* Gray. */
					if (tmp->level < level_color[level].green)
					{
						strcpy(buf, "It seems to contain no knowledge you could learn from.");
					}
					/* Green. */
					else if (tmp->level < level_color[level].blue)
					{
						strcpy(buf, "It seems to contain tiny bits of knowledge you could learn from.");
					}
					/* Blue. */
					else if (tmp->level < level_color[level].yellow)
					{
						strcpy(buf, "It seems to contain a small amount of knowledge you could learn from.");
					}
					/* Yellow. */
					else if (tmp->level < level_color[level].orange)
					{
						strcpy(buf, "It seems to contain an average amount of knowledge you could learn from.");
					}
					/* Orange. */
					else if (tmp->level < level_color[level].red)
					{
						strcpy(buf, "It seems to contain a moderate amount of knowledge you could learn from.");
					}
					/* Red. */
					else if (tmp->level < level_color[level].purple)
					{
						strcpy(buf, "It seems to contain a fair amount of knowledge you could learn from.");
					}
					/* Purple. */
					else
					{
						strcpy(buf, "It seems to contain a great amount of knowledge you could learn from.");
					}
				}
			}

			break;

		case CONTAINER:
			if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
				if (tmp->race != NULL)
				{
					if (tmp->weight_limit)
					{
						snprintf(buf, sizeof(buf), "It can hold only %s and its weight limit is %.1f kg.", tmp->race, (float) tmp->weight_limit / 1000.0f);
					}
					else
					{
						snprintf(buf, sizeof(buf), "It can hold only %s.", tmp->race);
					}

					/* Has magic modifier? */
					if (tmp->weapon_speed != 1.0f)
					{
						draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);

						/* Bad */
						if (tmp->weapon_speed > 1.0f)
						{
							snprintf(buf, sizeof(buf), "It increases the weight of items inside by %.1f%%.", tmp->weapon_speed * 100.0f);
						}
						/* Good */
						else
						{
							snprintf(buf, sizeof(buf), "It decreases the weight of items inside by %.1f%%.", 100.0f - (tmp->weapon_speed * 100.0f));
						}
					}
				}
				else
				{
					if (tmp->weight_limit)
					{
						snprintf(buf, sizeof(buf), "Its weight limit is %.1f kg.", (float)tmp->weight_limit / 1000.0f);
					}

					/* Has magic modifier? */
					if (tmp->weapon_speed != 1.0f)
					{
						draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);

						/* Bad */
						if (tmp->weapon_speed > 1.0f)
						{
							snprintf(buf, sizeof(buf), "It increases the weight of items inside by %.1f%%.", tmp->weapon_speed * 100.0f);
						}
						/* Good */
						else
						{
							snprintf(buf, sizeof(buf), "It decreases the weight of items inside by %.1f%%.", 100.0f - (tmp->weapon_speed * 100.0f));
						}
					}
				}

				draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);

				if (tmp->weapon_speed == 1.0f)
				{
					snprintf(buf, sizeof(buf), "It contains %3.3f kg.", (float) tmp->carrying / 1000.0f);
				}
				else if (tmp->weapon_speed > 1.0f)
				{
					snprintf(buf, sizeof(buf), "It contains %3.3f kg, increased to %3.3f kg.", (float) tmp->damage_round_tag / 1000.0f, (float) tmp->carrying / 1000.0f);
				}
				else
				{
					snprintf(buf, sizeof(buf), "It contains %3.3f kg, decreased to %3.3f kg.", (float) tmp->damage_round_tag / 1000.0f, (float) tmp->carrying / 1000.0f);
				}
			}

			break;

		case WAND:
			if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
				snprintf(buf, sizeof(buf), "It has %d charges left.", tmp->stats.food);
			}

			break;

		case POWER_CRYSTAL:
			/* Avoid division by zero... */
			if (tmp->stats.maxsp == 0)
			{
				snprintf(buf, sizeof(buf), "It has capacity of %d.", tmp->stats.maxsp);
			}
			else
			{
				/* Higher capacity crystals */
				if (tmp->stats.maxsp > 1000)
				{
					i = (tmp->stats.maxsp % 1000) / 100;

					if (i)
					{
						snprintf(tmp_buf, sizeof(tmp_buf), "It has capacity of %d.%dk and is ", tmp->stats.maxsp / 1000, i);
					}
					else
					{
						snprintf(tmp_buf, sizeof(tmp_buf), "It has capacity of %dk and is ", tmp->stats.maxsp / 1000);
					}
				}
				else
				{
					snprintf(tmp_buf, sizeof(tmp_buf), "It has capacity of %d and is ", tmp->stats.maxsp);
				}

				strcat(buf, tmp_buf);
				i = (tmp->stats.sp * 10) / tmp->stats.maxsp;

				if (tmp->stats.sp == 0)
				{
					strcat(buf, "empty.");
				}
				else if (i == 0)
				{
					strcat(buf, "almost empty.");
				}
				else if (i < 3)
				{
					strcat(buf, "partially filled.");
				}
				else if (i < 6)
				{
					strcat(buf, "half full.");
				}
				else if (i < 9)
				{
					strcat(buf, "well charged.");
				}
				else if (tmp->stats.sp == tmp->stats.maxsp)
				{
					strcat(buf, "fully charged.");
				}
				else
				{
					strcat(buf, "almost full.");
				}
			}

			break;
	}

	if (buf[0] != '\0')
	{
		draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);
	}

	if (tmp->material && (need_identify(tmp) && QUERY_FLAG(tmp, FLAG_IDENTIFIED)))
	{
		strcpy(buf, "It is made of: ");

		for (i = 0; i < NROFMATERIALS; i++)
		{
			if (tmp->material & (1 << i))
			{
				strcat(buf, materials[i].name);
				strcat(buf, " ");
			}
		}

		draw_info_full(0, COLOR_WHITE, sb_capture, op, buf);
	}

	if (tmp->weight)
	{
		float weight = (float) (tmp->nrof ? tmp->weight * (int) tmp->nrof : tmp->weight) / 1000.0f;

		if (tmp->type == MONSTER)
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight);
		}
		else if (tmp->type == PLAYER)
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s weighs %3.3f kg and is carrying %3.3f kg.", gender_subjective_upper[object_get_gender(tmp)], weight, (float) tmp->carrying / 1000.0f);
		}
		else
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, tmp->nrof > 1 ? "They weigh %3.3f kg." : "It weighs %3.3f kg.", weight);
		}
	}

	if (QUERY_FLAG(tmp, FLAG_STARTEQUIP))
	{
		/* Unpaid clone shop item */
		if (QUERY_FLAG(tmp, FLAG_UNPAID))
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", query_cost_string(tmp, op, COST_BUY));
		}
		/* God-given item */
		else
		{
			draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s god-given item%s.", tmp->nrof > 1 ? "They are" : "It is a", tmp->nrof > 1 ? "s" : "");

			if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
			{
				if (tmp->value)
				{
					draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "But %s worth %s.", tmp->nrof > 1 ? "they are" : "it is", query_cost_string(tmp, op, COST_TRUE));
				}
				else
				{
					draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s worthless.", tmp->nrof > 1 ? "They are" : "It is");
				}
			}
		}
	}
	else if (tmp->value && !IS_LIVE(tmp))
	{
		if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			if (QUERY_FLAG(tmp, FLAG_UNPAID))
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s would cost you %s.", tmp->nrof > 1 ? "They" : "It", query_cost_string(tmp, op, COST_BUY));
			}
			else
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s worth %s.", tmp->nrof > 1 ? "They are" : "It is", query_cost_string(tmp, op, COST_TRUE));
				goto dirty_little_jump1;
			}
		}
		else
		{
			object *floor_ob;
dirty_little_jump1:

			floor_ob = GET_MAP_OB_LAYER(op->map, op->x, op->y, LAYER_FLOOR, 0);

			if (floor_ob && floor_ob->type == SHOP_FLOOR && tmp->type != MONEY)
			{
				/* Used for SK_BARGAINING modification */
				int charisma = op->stats.Cha;

				/* This skill gives us a charisma boost */
				if (find_skill(op, SK_BARGAINING))
				{
					charisma += 4;

					if (charisma > MAX_STAT)
					{
						charisma = MAX_STAT;
					}
				}

				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "This shop will pay you %s (%0.1f%%).", query_cost_string(tmp, op, COST_SELL), 20.0f + 100.0f * cha_bonus[charisma]);
			}
		}
	}
	else if (!IS_LIVE(tmp))
	{
		if (QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			if (QUERY_FLAG(tmp, FLAG_UNPAID))
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s would cost nothing.", tmp->nrof > 1 ? "They" : "It");
			}
			else
			{
				draw_info_full_format(0, COLOR_WHITE, sb_capture, op, "%s worthless.", tmp->nrof > 1 ? "They are" : "It is");
			}
		}
	}

	/* Does the object have a message?  Don't show message for all object
	 * types - especially if the first entry is a match */
	if (tmp->msg && tmp->type != EXIT && tmp->type != BOOK && tmp->type != CORPSE && !QUERY_FLAG(tmp, FLAG_WALK_ON) && strncasecmp(tmp->msg, "@match", 7))
	{
		/* This is just a hack so when identifying the items, we print
		 * out the extra message */
		if ((need_identify(tmp) || QUERY_FLAG(tmp, FLAG_QUEST_ITEM)) && QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			draw_info_full(0, COLOR_WHITE, sb_capture, op, "The object has a story:");
			draw_info_full(0, COLOR_WHITE, sb_capture, op, tmp->msg);
		}
	}

	trigger_map_event(MEVENT_EXAMINE, op->map, op, tmp, NULL, NULL, 0);

	/* Blank line */
	draw_info_full(0, COLOR_WHITE, sb_capture, op, " ");

	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		StringBuffer *sb = stringbuffer_new();
		char *diff;

		stringbuffer_append_printf(sb, "count %d\n", tmp->count);
		dump_object(tmp, sb);
		diff = stringbuffer_finish(sb);
		draw_info_full(0, COLOR_WHITE, sb_capture, op, diff);
		free(diff);
	}
}

/**
 * Add a custom name to the marked object.
 * @param op Player.
 * @param params New name.
 * @return 1. */
int command_rename_item(object *op, char *params)
{
	object *tmp = find_marked_object(op);

	if (!tmp)
	{
		draw_info(COLOR_WHITE, op, "No marked item to rename.");
		return 1;
	}

	/* Will also clear unprintable chars. */
	params = cleanup_chat_string(params);

	/* Clear custom name. */
	if (!params)
	{
		if (!tmp->custom_name)
		{
			draw_info(COLOR_WHITE, op, "This item has no custom name.");
			return 1;
		}

		FREE_AND_CLEAR_HASH(tmp->custom_name);
		draw_info_format(COLOR_WHITE, op, "You stop calling your %s with weird names.", query_base_name(tmp, NULL));
	}
	else
	{
		if (tmp->type == MONEY)
		{
			draw_info(COLOR_WHITE, op, "You cannot rename that item.");
			return 1;
		}

		if (strlen(params) > 127)
		{
			draw_info(COLOR_WHITE, op, "New name is too long, maximum is 127 characters.");
			return 1;
		}

		if (tmp->custom_name && !strcmp(tmp->custom_name, params))
		{
			draw_info_format(COLOR_WHITE, op, "You keep calling your %s %s.", query_base_name(tmp, NULL), tmp->custom_name);
			return 1;
		}

		string_remove_markup(params);
		/* Set custom name. */
		FREE_AND_COPY_HASH(tmp->custom_name, params);
		draw_info_format(COLOR_WHITE, op, "Your %s will now be called %s.", query_base_name(tmp, NULL), tmp->custom_name);
		CONTR(op)->stat_renamed_items++;
	}

	esrv_update_item(UPD_NAME, tmp);
	object_merge(tmp);

	return 1;
}

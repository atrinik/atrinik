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
 * Monster memory, NPC interaction, AI, and other related functions
 * are in this file.
 * @todo Move spawn point related functions to spawnpoint.c
 * @todo Move waypoint related functions to waypoint.c */

#include <global.h>
#ifndef __CEXTRACT__
#include <sproto.h>
#endif

/** When parsing a message-struct, the msglang struct is used
 * to contain the values.
 * This struct will be expanded as new features are added.
 * When things are stable, it will be parsed only once. */
typedef struct _msglang
{
	/** An array of messages */
	char **messages;

	/** For each message, an array of strings to match */
	char ***keywords;
} msglang;


extern spell spells[NROFREALSPELLS];

/**
 * Update (or clear) an NPC's enemy. Perform most of the housekeeping
 * related to switchign enemies
 *
 * You should always use this method to set (or clear) an NPC's enemy.
 *
 * If enemy is given an aggro wp may be set up.
 * If rv is given, it will be filled out with the vector to enemy
 *
 * enemy and/or rv may be NULL.
 * @param npc The NPC object we're setting enemy for
 * @param enemy The enemy object, NULL if we're clearing the enemy
 * for this NPC
 * @param rv Range vector of the enemy */
void set_npc_enemy(object *npc, object *enemy, rv_vector *rv)
{
	object *aggro_wp;
	rv_vector rv2;

	/* Do nothing if new enemy == old enemy */
	if (enemy == npc->enemy && (enemy == NULL || enemy->count == npc->enemy_count))
	{
		return;
	}

	/* Players don't need waypoints, speed updates or aggro counters */
	if (npc->type == PLAYER)
	{
		npc->enemy = enemy;
		npc->enemy_count = enemy->count;

		return;
	}

	/* Non-aggro-waypoint related stuff */
	if (enemy)
	{
		if (rv == NULL)
		{
			rv = &rv2;
		}

		get_rangevector(npc, enemy, rv, RV_DIAGONAL_DISTANCE);
		npc->enemy_count = enemy->count;

		/* important: that's our "we lose aggro count" - reset to zero here */
		npc->last_eat = 0;

		/* Monster has changed status from normal to attack - let's hear it! */
		if (npc->enemy == NULL && !QUERY_FLAG(npc, FLAG_FRIENDLY))
		{
			play_sound_map(npc->map, npc->x, npc->y, SOUND_GROWL, SOUND_NORMAL);
		}

		if (QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
		{
			/* The unaggressives look after themselves 8) */
			CLEAR_FLAG(npc, FLAG_UNAGGRESSIVE);
			npc_call_help(npc);
		}
	}
	else
	{
		/* If mob lost aggro, let it return home */
		if (OBJECT_VALID(npc->enemy, npc->enemy_count))
		{
			object *base = insert_base_info_object(npc);
			object *wp = get_active_waypoint(npc);

			if (base && !wp && wp_archetype)
			{
				object *return_wp = get_return_waypoint(npc);

#ifdef DEBUG_PATHFINDING
				LOG(llevDebug, "set_npc_enemy(): %s lost aggro and is returning home (%s:%d,%d)\n", STRING_OBJ_NAME(npc), base->slaying, base->x, base->y);
#endif
				if (!return_wp)
				{
					return_wp = arch_to_object(wp_archetype);
					insert_ob_in_ob(return_wp, npc);
					return_wp->owner = npc;
					return_wp->ownercount = npc->count;
					FREE_AND_COPY_HASH(return_wp->name, "- home -");
					/* mark as return-home wp */
					SET_FLAG(return_wp, FLAG_REFLECTING);
					/* mark as best-effort wp */
					SET_FLAG(return_wp, FLAG_NO_ATTACK);
				}

				return_wp->stats.hp = base->x;
				return_wp->stats.sp = base->y;
				FREE_AND_ADD_REF_HASH(return_wp->slaying, base->slaying);
				/* Activate wp */
				SET_FLAG(return_wp, FLAG_CURSED);
				/* reset best-effort timer */
				return_wp->stats.Int = 0;

				/* setup move_type to use waypoints */
				return_wp->move_type = npc->move_type;
				npc->move_type = (npc->move_type & LO4) | WPOINT;

				wp = return_wp;
			}

			/* TODO: add a little pause to the active waypoint */
		}
	}

	npc->enemy = enemy;
	/* Update speed */
	set_mobile_speed(npc, 0);

	/* Setup aggro waypoint */
	if (!wp_archetype)
	{
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "set_npc_enemy(): Aggro waypoints disabled\n");
#endif
		return;
	}

	/* TODO: check intelligence against lower limit to allow pathfind */
	aggro_wp = get_aggro_waypoint(npc);

	/* Create a new aggro wp for npc? */
	if (!aggro_wp && enemy)
	{
		aggro_wp = arch_to_object(wp_archetype);
		insert_ob_in_ob(aggro_wp, npc);
		/* Mark as aggro WP */
		SET_FLAG(aggro_wp, FLAG_DAMNED);
		aggro_wp->owner = npc;
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "set_npc_enemy(): created wp for '%s'\n", STRING_OBJ_NAME(npc));
#endif
	}

	/* Set up waypoint target (if we actually got a waypoint) */
	if (aggro_wp)
	{
		if (enemy)
		{
			aggro_wp->enemy_count = npc->enemy_count;
			aggro_wp->enemy = enemy;
			FREE_AND_ADD_REF_HASH(aggro_wp->name, enemy->name);
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "set_npc_enemy(): got wp for '%s' -> '%s'\n", npc->name, enemy->name);
#endif
		}
		else
		{
			aggro_wp->enemy = NULL;
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "set_npc_enemy(): cleared aggro wp for '%s'\n", npc->name);
#endif
		}
	}
}

/**
 * Checks if NPC's enemy is still valid.
 * @param npc The NPC object
 * @param rv Range vector of the enemy
 * @return Enemy object if valid, NULL otherwise. */
object *check_enemy(object *npc, rv_vector *rv)
{
	/* if this is pet, let him attack the same enemy as his owner
	 * TODO: when there is no ower enemy, try to find a target,
	 * which CAN attack the owner. */
	if ((npc->move_type & HI4) == PETMOVE)
	{
		if (npc->owner != NULL)
		{
			/* if owner enemy != pet enemy, change it! */
			if (npc->owner->enemy && (npc->enemy != npc->owner->enemy || npc->enemy_count != npc->enemy->count))
			{
				set_npc_enemy(npc, npc->owner->enemy, NULL);
			}
		}
		else
		{
			if (npc->enemy)
			{
				set_npc_enemy(npc, NULL, NULL);
			}
		}
	}

	/*LOG(-1,"CHECK_START: %s -> %s (%x - %x)\n", query_name(npc),query_name(npc->enemy), npc->enemy?npc->enemy->count:2,npc->enemy_count);*/
	if (npc->enemy == NULL)
	{
		return NULL;
	}

	if (!OBJECT_VALID(npc->enemy, npc->enemy_count) || npc == npc->enemy)
	{
		set_npc_enemy(npc, NULL, NULL);

		return NULL;
	}

	/* check flags for friendly npc and aggressive mobs (unaggressive will not handled here -
	 * it only means it don't seek targets - its not a "no fight" flag.
	 * important: here we add our aggravate flag - then friendly will attack friendly (and attack
	 * on until flag is removed = ring of conflict. Same for aggro mobs. */
	if (QUERY_FLAG(npc, FLAG_FRIENDLY))
	{
		/* NPC should not attack players or other friendly units on purpose */
		if (npc->enemy->type == PLAYER || QUERY_FLAG(npc->enemy, FLAG_FRIENDLY))
		{
			set_npc_enemy(npc, NULL, NULL);

			return NULL;
		}
	}
	/* and the same for unfriendly */
	else
	{
		/* this is a important check - without this, a single area spell from a
		 * mob will aggravate all other mobs to him - they will slaughter themself
		 * and not the player. */
		if (!QUERY_FLAG(npc->enemy, FLAG_FRIENDLY) && npc->enemy->type != PLAYER)
		{
			set_npc_enemy(npc, NULL, NULL);

			return NULL;
		}
	}

	return can_detect_enemy(npc, npc->enemy, rv) ? npc->enemy : NULL;
}

/**
 * Tries to find an enemy for NPC. We pass the range vector since
 * our called will find the information useful.
 *
 * Currently, only move_monster() calls this function.
 * @param npc The NPC object
 * @param rv Range vector
 * @return Enemy object if found, 0 otherwise
 * @note This doesn't find an enemy - it only checks the old enemy
 * is still valid and hittable, otherwise changes target to a
 * better enemy when possible. */
object *find_enemy(object *npc, rv_vector *rv)
{
	object *tmp = NULL;

	/* BERSERK is not activated atm - like aggravation & conflict - will come */
	/* if we berserk, we don't care about others - we attack all we can find */
	if (QUERY_FLAG(npc, FLAG_BERSERK))
	{
		/* always clear the attacker entry */
		npc->attacked_by = NULL;
		tmp = find_nearest_living_creature(npc);

		if (tmp)
		{
			get_rangevector(npc, tmp, rv, 0);
		}

		return tmp;
	}

	/* Here is the main enemy selection.
	 * We want this: if there is an enemy, attack him until its not possible or
	 * one of both is dead.
	 * If we have no enemy and we are...
	 * a monster: try to find a player, a pet or a friendly monster
	 * a friendly: only target a monster which is targeting you first or targeting a player
	 * a pet: attack player enemy or a monster */

	/* pet move */
	if ((npc->move_type & HI4) == PETMOVE)
	{
		/* always clear the attacker entry */
		npc->attacked_by = NULL;
		tmp= get_pet_enemy(npc, rv);
		npc->last_eat = 0;

		if (tmp)
		{
			get_rangevector(npc, tmp, rv, 0);
		}

		return tmp;
	}

	/* we check our old enemy. */
	/* if tmp != 0, we have succesful callled get_rangevector() too */
	tmp = check_enemy(npc, rv);

	/*LOG(-1, "CHECK: mob %s -> <%s> (%s (%d - %d))\n", query_name(npc, NULL), query_name(tmp, NULL), npc->attacked_by ? npc->attacked_by->name : "xxx", npc->attacked_by_distance, (int)rv->distance); */
	if (!tmp || (npc->attacked_by && npc->attacked_by_distance < (int)rv->distance))
	{
		/* if we have an attacker, check him */
		if (OBJECT_VALID(npc->attacked_by, npc->attacked_by_count))
		{
			/* TODO: thats not finished */
			/* we don't want a fight evil vs evil or good against non evil */
			if ((QUERY_FLAG(npc, FLAG_FRIENDLY) && QUERY_FLAG(npc->attacked_by, FLAG_FRIENDLY)) || (!QUERY_FLAG(npc, FLAG_FRIENDLY) && (!QUERY_FLAG(npc->attacked_by, FLAG_FRIENDLY) && npc->attacked_by->type != PLAYER)))
			{
				/* skip it, but lets wakeup */
				CLEAR_FLAG(npc, FLAG_SLEEP);
			}
			/* thats the only thing we must know... */
			else if (on_same_map(npc, npc->attacked_by))
			{
				/* well, NOW we really should wake up! */
				CLEAR_FLAG(npc, FLAG_SLEEP);

				set_npc_enemy(npc, npc->attacked_by, rv);

				/* always clear the attacker entry */
				npc->attacked_by = NULL;

				/* yes, we face our attacker! */
				return npc->enemy;
			}
		}

		/* i think to add here a counter to determinate a mob lost a enemy or the enemy
		 * was x rounds out of range - then perhaps we should search a new one */
		/* we have no legal enemy or attacker, so we try to target a new one */
		if (!QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
		{
			if (QUERY_FLAG(npc, FLAG_FRIENDLY))
				tmp = find_nearest_living_creature(npc);
			else
				tmp = get_nearest_player(npc);

			if (tmp != npc->enemy)
				set_npc_enemy(npc, tmp, rv);
		}
		else if (npc->enemy)
		{
			/* Make sure to clear the enemy, even if FLAG_UNAGRESSIVE is true */
			set_npc_enemy(npc, NULL, NULL);
		}
	}

	/* always clear the attacker entry */
	npc->attacked_by = NULL;
	return tmp;
}

/**
 * Check if target is a possible valid enemy.
 * This includes possibility to see, reach, etc.
 * This must be used BEFORE we assign target as op enemy.
 * @param op The object
 * @param target The target to check
 * @param range Range this object can see
 * @param srange Stealth range this object can see
 * @param rv Range vector
 * @return 1 if valid enemy, 0 otherwise */
int can_detect_target(object *op, object *target, int range, int srange, rv_vector *rv)
{
	/* on_same_map() will check for legal maps too */
	if (!op || !target || !on_same_map(op, target))
	{
		return 0;
	}

	/* we check for sys_invisible and normal */
	if (IS_INVISIBLE(target, op))
	{
		return 0;
	}

	get_rangevector(op, target, rv, 0);

	if (QUERY_FLAG(target, FLAG_STEALTH) && !QUERY_FLAG(op, FLAG_XRAYS))
	{
		if (srange < (int) rv->distance)
		{
			return 0;
		}
	}
	else
	{
		if (range < (int) rv->distance)
		{
			return 0;
		}
	}

	/* at this point we should handle hide in shadows and light/shadow effects
	 * of the tile the player/npc is in.
	 * depending on infravision or blind of the mobile we can create here more
	 * tricky effects. */

	return 1;
}

/**
 * Controls if monster still can see/detect its enemy.
 * Includes visibility but also map and area control.
 * @param op The monster object
 * @param enemy Monster object's enemy
 * @param rv Range vector
 * @return 1 if can see/detect, 0 otherwise */
int can_detect_enemy(object *op, object *enemy, rv_vector *rv)
{
	/* on_same_map() will check for legal maps too */
	if (!op || !enemy || !on_same_map(op, enemy))
	{
		return 0;
	}

	/* we check for sys_invisible and normal */
	/* we should include here special parts like wild swings to
	 * invisible targets - invisibility is not added to fight
	 * system atm. */
	if (IS_INVISIBLE(enemy, op))
	{
		return 0;
	}

	/* here we have to add special effects like hide in shadows and other. */

	/* If our enemy is too far away ... */
	get_rangevector(op, enemy, rv, 0);

	if ((int) rv->distance >= MAX(MAX_AGGRO_RANGE, op->stats.Wis))
	{
		/* Then start counting until our mob loses aggro... */
		if (++op->last_eat > MAX_AGGRO_TIME)
		{
			set_npc_enemy(op, NULL, NULL);

			return 0;
		}
	}
	/* Our mob is aggroed again - because target is in range again */
	else
	{
		op->last_eat = 0;
	}

	return 1;
}

/**
 * Find a monster's currently active waypoint, if any.
 * @param op The monster object
 * @return Active waypoint of this monster, NULL if none found */
object *get_active_waypoint(object *op)
{
	object *wp = NULL;

	for (wp = op->inv; wp != NULL; wp = wp->below)
	{
		if (wp->type == TYPE_WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_CURSED))
		{
			break;
		}
	}

	return wp;
}

/**
 * Find a monster's current aggro waypoint, if any.
 * @param op The monster object
 * @return Aggro waypoint of this monster, NULL if none found */
object *get_aggro_waypoint(object *op)
{
	object *wp = NULL;

	for (wp = op->inv; wp != NULL; wp = wp->below)
	{
		if (wp->type == TYPE_WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_DAMNED))
		{
			break;
		}
	}

	return wp;
}

/**
 * Find a monster's current return-home waypoint, if any.
 * @param op The monster object
 * @return Return-home waypoint of this monster, NULL if none
 * found */
object *get_return_waypoint(object *op)
{
	object *wp = NULL;

	for (wp = op->inv; wp != NULL; wp = wp->below)
	{
		if (wp->type == TYPE_WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_REFLECTING))
		{
			break;
		}
	}

	return wp;
}

/**
 * Find a monster's waypoint by name (used for getting the next waypoint)
 * @param op The monster object
 * @param name The waypoint name to find
 * @return The waypoint object if found, NULL otherwise */
object *find_waypoint(object *op, const char *name)
{
	object *wp = NULL;

	if (name == NULL)
	{
		return NULL;
	}

	for (wp = op->inv; wp != NULL; wp = wp->below)
	{
		if (wp->type == TYPE_WAYPOINT_OBJECT && strcmp(wp->name, name) == 0)
		{
			break;
		}
	}

	return wp;
}

/**
 * Petform a path computation for the waypoint object.
 *
 * This function is called whenever our path request is dequeued.
 * @param waypoint The waypoint object
 * @todo This function is getting very messy. Clean it up some rainy day. */
void waypoint_compute_path(object *waypoint)
{
	object *op = waypoint->env;
	mapstruct *destmap = op->map;
	path_node *path;

	/* Store final path destination (used by aggro wp) */
	if (QUERY_FLAG(waypoint, FLAG_DAMNED))
	{
		if (OBJECT_VALID(waypoint->enemy, waypoint->enemy_count))
		{
			FREE_AND_COPY_HASH(waypoint->slaying, waypoint->enemy->map->path);
			waypoint->x = waypoint->stats.hp = waypoint->enemy->x;
			waypoint->y = waypoint->stats.sp = waypoint->enemy->y;
		}
		else
		{
			LOG(llevBug, "BUG: waypoint_compute_path(): dynamic waypoint without valid target:'%s'\n", waypoint->name);
			return;
		}
	}

	if (waypoint->slaying != NULL)
	{
		/* If path not normalized: normalize it */
		if (*waypoint->slaying != '/')
		{
			char temp_path[HUGE_BUF];

			if (*waypoint->slaying == '\0')
			{
				LOG(llevBug, "BUG: waypoint_compute_path(): invalid destination map '%s'\n", STRING_OBJ_SLAYING(waypoint));
				return;
			}
			normalize_path(op->map->path, waypoint->slaying, temp_path);
			FREE_AND_COPY_HASH(waypoint->slaying, temp_path);
		}

		/* TODO: handle unique maps? */
		if (waypoint->slaying == op->map->path)
			destmap = op->map;
		else
			destmap = ready_map_name(waypoint->slaying, MAP_NAME_SHARED);
	}

	if (destmap == NULL)
	{
		LOG(llevBug, "BUG: waypoint_compute_path(): invalid destination map '%s'\n", waypoint->slaying);
		return;
	}

	path = compress_path(find_path(op, op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp));
	if (path)
	{
		/* Skip the first path element (always the starting position) */
		path = path->next;
		if (!path)
		{
			SET_FLAG(waypoint, FLAG_CONFUSED);
			return;
		}

#ifdef DEBUG_PATHFINDING
		path_node *tmp;
		LOG(llevDebug, "waypoint_compute_path(): '%s' new path -> '%s': ", op->name, waypoint->name);
		for (tmp = path; tmp; tmp = tmp->next)
			LOG(llevDebug, "(%d,%d) ", tmp->x, tmp->y);
		LOG(llevDebug,"\n");
#endif
		/* textually encoded path */
		FREE_AND_CLEAR_HASH(waypoint->msg);
		/* map file for last local path step */
		FREE_AND_CLEAR_HASH(waypoint->race);

		waypoint->msg = encode_path(path);
		/* path offset */
		waypoint->stats.food = 0;
		/* Msg boundary */
		waypoint->attacked_by_distance = strlen(waypoint->msg);

		/* number of fails */
		waypoint->stats.Int = 0;
		/* number of fails */
		waypoint->stats.Str = 0;
		/* best distance */
		waypoint->stats.dam = 30000;
		CLEAR_FLAG(waypoint, FLAG_CONFUSED);
	}
	else
		LOG(llevBug, "BUG: waypoint_compute_path(): no path to destination ('%s' -> '%s')\n", op->name, waypoint->name);
}

/**
 * Move towards waypoint target.
 * @param op The monster object to move
 * @param waypoint The waypoint object */
void waypoint_move(object *op, object *waypoint)
{
	mapstruct *destmap = op->map;
	rv_vector local_rv, global_rv, *dest_rv;
	int dir;
	sint16 new_offset = 0, success = 0;

	if (waypoint == NULL || op == NULL || op->map == NULL)
		return;

	/* Aggro or static waypoint? */
	if (QUERY_FLAG(waypoint, FLAG_DAMNED))
	{
		/* Verify enemy */
		if (waypoint->enemy == op->enemy && waypoint->enemy_count == op->enemy_count && OBJECT_VALID(waypoint->enemy, waypoint->enemy_count))
		{
			destmap = waypoint->enemy->map;
			waypoint->stats.hp = waypoint->enemy->x;
			waypoint->stats.sp = waypoint->enemy->y;
		}
		else
		{
			/* owner has either switched or lost enemy. This should work for both cases
			 * switched -> similar to if target moved
			 * lost -> we shouldn't be called again without new data */
			waypoint->enemy = op->enemy;
			waypoint->enemy_count = op->enemy_count;
			return;
		}
	}
	else
	{
		/* Find the destination map if specified in waypoint (otherwise use current map) */
		if (waypoint->slaying != NULL)
		{
			/* If path not normalized: normalize it */
			if (*waypoint->slaying != '/')
			{
				char temp_path[HUGE_BUF];

				if (*waypoint->slaying == '\0')
				{
					LOG(llevBug, "BUG: waypoint_move(): invalid destination map '%s' for '%s' -> '%s'\n", STRING_OBJ_SLAYING(waypoint), query_name(op, NULL), query_name(waypoint, NULL));
					return;
				}

				normalize_path(op->map->path, waypoint->slaying, temp_path);
				FREE_AND_COPY_HASH(waypoint->slaying, temp_path);
			}

			/* TODO: handle unique maps? */
			/* check we are on the map */
			if (waypoint->slaying == op->map->path)
				destmap = op->map;
			else
				destmap = ready_map_name(waypoint->slaying, MAP_NAME_SHARED);
		}
	}

	if (destmap == NULL)
	{
		LOG(llevBug, "BUG: waypoint_move(): invalid destination map '%s' for '%s' -> '%s'\n", waypoint->slaying, op->name, waypoint->name);
		return;
	}

	if (!get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp, &global_rv, RV_RECURSIVE_SEARCH | RV_DIAGONAL_DISTANCE))
	{
		LOG(llevBug, "BUG: waypoint_move(): Maps are not connected: '%s' and '%s'\n", destmap->path, op->map->path);
		/* disable this waypoint */
		CLEAR_FLAG(waypoint, FLAG_CURSED);
		return;
	}

	dest_rv = &global_rv;

	/* Reached the final destination? */
	if ((int)global_rv.distance <= waypoint->stats.grace)
	{
		object *nextwp = NULL;

		/* Just arrived? */
		if (waypoint->stats.ac == 0)
		{
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "move_waypoint(): '%s' reached destination '%s'\n", op->name, waypoint->name);
#endif

			/* Trigger the TRIGGER event*/
			if (trigger_event(EVENT_TRIGGER, op, waypoint, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
			{
				return;
			}
		}

		/* Waiting at this WP? */
		/* TODO: use timers instead? */
		if (waypoint->stats.ac < waypoint->stats.wc)
		{
			waypoint->stats.ac++;
			return;
		}

		/* clear timer */
		waypoint->stats.ac = 0;
		/* set inactive */
		CLEAR_FLAG(waypoint, FLAG_CURSED);
		/* remove precomputed path */
		FREE_AND_CLEAR_HASH(waypoint->msg);
		/* remove precomputed path data */
		FREE_AND_CLEAR_HASH(waypoint->race);

		/* Is it a return-home waypoint? */
		if (QUERY_FLAG(waypoint, FLAG_REFLECTING))
			op->move_type = waypoint->move_type;

		/* Start over with the new waypoint, if any*/
		if (!QUERY_FLAG(waypoint, FLAG_DAMNED))
		{
			nextwp = find_waypoint(op, waypoint->title);
			if (nextwp)
			{
#ifdef DEBUG_PATHFINDING
				LOG(llevDebug, "waypoint_move(): '%s' next WP: '%s'\n", op->name, waypoint->title);
#endif
				SET_FLAG(nextwp, FLAG_CURSED);
				waypoint_move(op, get_active_waypoint(op));
			}
			else
			{
#ifdef DEBUG_PATHFINDING
				LOG(llevDebug, "waypoint_move(): '%s' no next WP\n", op->name);
#endif
			}
		}

		waypoint->enemy = NULL;

		return;
	}

	/* Handle precomputed paths */

	/* If we finished our current path. Clear it so that we can get a new one. */
	if (waypoint->msg != NULL && (waypoint->msg[waypoint->stats.food] == '\0' || global_rv.distance <= 0))
		FREE_AND_CLEAR_HASH(waypoint->msg);

	/* Get new path if target has moved much since the path was created */
	if (QUERY_FLAG(waypoint, FLAG_DAMNED) && waypoint->msg != NULL && (waypoint->stats.hp != waypoint->x || waypoint->stats.sp != waypoint->y))
	{
		rv_vector rv;
		/* TODO: unique maps */
		mapstruct *path_destmap = ready_map_name(waypoint->slaying, MAP_NAME_SHARED);
		get_rangevector_from_mapcoords(destmap, waypoint->stats.hp, waypoint->stats.sp, path_destmap, waypoint->x, waypoint->y, &rv, 8);

		if (rv.distance > 1 && rv.distance > global_rv.distance)
		{
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "waypoint_move(): path_distance = %d for '%s' -> '%s'. Discarding old path.\n", rv.distance, op->name, op->enemy->name);
#endif
			FREE_AND_CLEAR_HASH(waypoint->msg);
		}
	}

	/* Are we far enough from the target to require a path? */
	if (global_rv.distance > 1)
	{
		if (waypoint->msg == NULL)
		{
			/* Request a path if we don't have one */
			request_new_path(waypoint);
		}
		else
		{
			/* If we have precalculated path, take direction to next subwaypoint */
			int destx = waypoint->stats.hp, desty = waypoint->stats.sp;
			new_offset = waypoint->stats.food;

			if (new_offset < waypoint->attacked_by_distance && get_path_next(waypoint->msg, &new_offset, &waypoint->race, &destmap, &destx, &desty))
			{
				get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, destx, desty, &local_rv, 2 | 8);
				dest_rv = &local_rv;
			}
			else
			{
				/* We seem to have an invalid path string or offset. */
				/* this bug message was spaming when the target has moved to a
				 * swaped out map i think */
				/* LOG(llevBug,"BUG: waypoint_move(): invalid path string or offset '%s':%d in '%s' -> '%s'\n",
				        waypoint->msg, new_offset, op->name, waypoint->name); */
				FREE_AND_CLEAR_HASH(waypoint->msg);
				request_new_path(waypoint);
			}
		}
	}

	/* Did we get closer to our goal last time? */
	if ((int)dest_rv->distance < waypoint->stats.dam)
	{
		waypoint->stats.dam = dest_rv->distance;
		/* Number of times we failed getting closer to (sub)goal */
		waypoint->stats.Str = 0;
	}
	else if (waypoint->stats.Str++ > 4)
	{
		/* Discard the current path, so that we can get a new one */
		FREE_AND_CLEAR_HASH(waypoint->msg);

		/* For best-effort waypoints don't try too many times. */
		if (QUERY_FLAG(waypoint, FLAG_NO_ATTACK) && waypoint->stats.Int++ > 10)
		{
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "Stuck with a best-effort waypoint (%s). Accepting current position\n", waypoint->name);
#endif
			/* A bit ugly, but will work for now (we want to trigger the "reached goal" above) */
			waypoint->stats.hp = op->x;
			waypoint->stats.sp = op->y;
			return;
		}
	}

	if (global_rv.distance > 1 && waypoint->msg == NULL && QUERY_FLAG(waypoint, FLAG_CONFUSED))
	{
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "waypoint_move(): no path found. '%s' standing still\n", op->name);
#endif
		return;
	}

	/* Perform the actual move */

	dir = dest_rv->direction;

	if (QUERY_FLAG(op, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	if (dir && !QUERY_FLAG(op, FLAG_STAND_STILL))
	{
		/* Can the monster move directly toward waypoint? */
		if (move_object(op, dir))
			success = 1;
		else
		{
			int diff;
			/* Try move around corners otherwise */
			for (diff = 1; diff <= 2; diff++)
			{
				/* try different detours */
				/* Try left or right first? */
				int m = 1 - (RANDOM() & 2);
				if (move_object(op, absdir(dir + diff * m)) || move_object(op, absdir(dir - diff * m)))
				{
					success = 1;
					break;
				}
			}
		}

		/* If we had a local destination and we got close enough to it, accept it...
		 * (re distance check: dest_rv->distance is distance before the step.
		 *  if the move was succesful, we got closer to the dest, otherwise
		 *  we can accept a small distance from it) */
		if (dest_rv == &local_rv && dest_rv->distance == 1)
		{
			waypoint->stats.food = new_offset;
			/* number of fails */
			waypoint->stats.Str = 0;
			/* best distance */
			waypoint->stats.dam = 30000;
		}
	}
}

/**
 * Move a monster object.
 * @param op The monster object to be moved
 * @return 1 if the object has been freed, 0 otherwise */
int move_monster(object *op)
{
	int dir, special_dir = 0, diff;
	object *owner, *enemy, *part, *tmp;
	rv_vector rv;

	if (op->head)
	{
		LOG(llevBug, "BUG: move_monster(): called from tail part. (%s -- %s)\n", query_name(op, NULL), op->arch->name);
		return 0;
	}

	/* Monsters not on maps don't do anything.  These monsters are things
	 * Like royal guards in city dwellers inventories. */
	if (!op->map)
		return 0;

	/* if we are here, we never paralyzed anymore */
	CLEAR_FLAG(op, FLAG_PARALYZED);

	/* for target facing, we copy this value here for fast access */
	/* for some reason, rv is not set right for targeted enemy all times */
	/* so i call it here direct again */
	op->anim_enemy_dir = -1;
	op->anim_moving_dir = -1;

	/* Here is the heart of the mob attack & target area.
	 * find_enemy() checks the old enemy or get us a new one. */
	tmp = op->enemy;
	/* we never ever attack */
	if (QUERY_FLAG(op, FLAG_NO_ATTACK))
	{
		if (op->enemy)
			set_npc_enemy(op, NULL, NULL);
		enemy = NULL;
	}
	else if ((enemy = find_enemy(op, &rv)))
	{
		CLEAR_FLAG(op, FLAG_SLEEP);
		op->anim_enemy_dir = rv.direction;
		if (!enemy->attacked_by || (enemy->attacked_by && enemy->attacked_by_distance > (int)rv.distance))
		{
			/* we have an enemy, just tell him we want him dead */
			/* our ptr */
			enemy->attacked_by = op;
			/* our tag */
			enemy->attacked_by_count = op->count;
			/* NOW the attacked foe knows how near we are */
			enemy->attacked_by_distance = (sint16) rv.distance;
		}
	}

	/*  generate hp, if applicable */
	if (op->stats.Con && op->stats.hp < op->stats.maxhp)
	{
		if (++op->last_heal > 5)
		{
			op->last_heal = 0;
			op->stats.hp += op->stats.Con;

			if (op->stats.hp > op->stats.maxhp)
				op->stats.hp = op->stats.maxhp;
		}

		/* So if the monster has gained enough HP that they are no longer afraid */
		if (QUERY_FLAG(op,FLAG_RUN_AWAY) && op->stats.hp >= (signed short)(((float)op->run_away / (float)100) * (float)op->stats.maxhp))
			CLEAR_FLAG(op, FLAG_RUN_AWAY);
	}

	/* generate sp, if applicable */
	if (op->stats.Pow && op->stats.sp < op->stats.maxsp)
	{
		op->last_sp += (int)((float)(8 * op->stats.Pow) / FABS(op->speed));
		/* causes Pow/16 sp/tick */
		op->stats.sp += op->last_sp / 128;
		op->last_sp %= 128;
		if (op->stats.sp > op->stats.maxsp)
			op->stats.sp = op->stats.maxsp;
	}

	/* Time to regain some "guts"... */
	if (QUERY_FLAG(op, FLAG_SCARED) && !(RANDOM() % 20))
		CLEAR_FLAG(op, FLAG_SCARED);

	/* check if monster pops out of hidden spot */
	/*if(op->hide) do_hidden_move(op);*/

	/* I disabled automatically pick & and apply of monsters.
	 * we should not do it in that generic way - taking and using
	 * items is a AI action - and thats something we still need to
	 * add in daimonin.
	if(op->pick_up)
		monster_check_pickup(op);
	if(op->will_apply)
		monster_apply_below(op);
	*/

	/* If we don't have an enemy, do special movement or the like */
	if (!enemy)
	{
		if (QUERY_FLAG(op, FLAG_ONLY_ATTACK))
		{
			remove_ob(op);
			check_walk_off(op, NULL, MOVE_APPLY_DEFAULT);
			return 1;
		}

		if (!QUERY_FLAG(op, FLAG_STAND_STILL))
		{
			if (op->move_type & HI4)
			{
				switch (op->move_type & HI4)
				{
					case PETMOVE:
						pet_move(op);
						break;
					case CIRCLE1:
						circ1_move(op);
						break;
					case CIRCLE2:
						circ2_move(op);
						break;
					case PACEV:
						pace_movev(op);
						break;
					case PACEH:
						pace_moveh(op);
						break;
					case PACEV2:
						pace2_movev(op);
						break;
					case PACEH2:
						pace2_moveh(op);
						break;
					case RANDO:
						rand_move(op);
						break;
					case RANDO2:
						move_randomly(op);
						break;
					case WPOINT:
						waypoint_move(op, get_active_waypoint(op));
						break;
				}

				/*if(OBJECT_FREE(op)) return 1; */ /* hm, when freed dont lets move him anymore */
				return 0;
			}
			else if (QUERY_FLAG(op, FLAG_RANDOM_MOVE))
				(void) move_randomly(op);

		}

		return 0;
	}

	/* We have an enemy.  Block immediately below is for pets */
	if ((op->type & HI4) == PETMOVE && (owner = get_owner(op)) != NULL && !on_same_map(op, owner))
	{
		follow_owner(op, owner);
		/* Gecko: The following block seems buggy, but I'm not sure... */
		if (QUERY_FLAG(op, FLAG_REMOVED) && FABS(op->speed) > MIN_ACTIVE_SPEED)
		{
			remove_friendly_object(op);
			return 1;
		}
		return 0;
	}

	/* Move the check for scared up here - if the monster was scared,
	 * we were not doing any of the logic below, so might as well save
	 * a few cpu cycles. */
	if (!QUERY_FLAG(op, FLAG_SCARED))
	{
		rv_vector rv1;

		/* now we test every part of an object .... this is a real ugly piece of code */
		for (part = op; part != NULL; part = part->more)
		{
			get_rangevector(part, enemy, &rv1, 0x1);
			dir = rv1.direction;

			/* hm, not sure about this part - in original was a scared flag here too
			 * but that we test above... so can be old code here */
			if (QUERY_FLAG(op, FLAG_RUN_AWAY))
				dir = absdir(dir + 4);

			if (QUERY_FLAG(op, FLAG_CONFUSED))
				dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

			if (op->last_grace)
				op->last_grace--;

			if (op->stats.Dex && !(RANDOM() % op->stats.Dex))
			{
				if (QUERY_FLAG(op, FLAG_CAST_SPELL) && !op->last_grace)
				{
					if (monster_cast_spell(op, part, enemy, dir, &rv1))
					{
						/* add also monster casting delay */
						op->last_grace += op->magic;
						return 0;
					}
				}

				if (QUERY_FLAG(op, FLAG_READY_RANGE))
				{
					if (!(RANDOM() % 3))
					{
						if (monster_use_wand(op, part, enemy, dir))
							return 0;
					}

					if (!(RANDOM() % 4))
					{
						if (monster_use_rod(op, part, enemy, dir))
							return 0;
					}

					if (!(RANDOM() % 5))
					{
						if (monster_use_horn(op, part, enemy, dir))
							return 0;
					}
				}

				if (QUERY_FLAG(op, FLAG_READY_SKILL) && !(RANDOM() % 3))
				{
#if 0
					if (monster_use_skill(op, part, enemy, dir))
						return 0;
#endif
					/* allow skill use AND melee attack! */
					monster_use_skill(op, part, enemy, dir);
				}

				if (QUERY_FLAG(op, FLAG_READY_BOW) && !(RANDOM() % 4))
				{
					if (monster_use_bow(op, part, enemy, dir) && !(RANDOM() % 2))
						return 0;
				}
			}
		}
	}

	/* TODO: haven't we already done this in check_enemy? */
	get_rangevector(op, enemy, &rv, 0);
	part = rv.part;
	/* dir is now direction towards enemy */
	dir = rv.direction;

	if (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY))
		dir = absdir(dir + 4);

	if (QUERY_FLAG(op, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	if (!QUERY_FLAG(op, FLAG_SCARED))
	{
		if (op->attack_move_type & LO4)
		{
			switch (op->attack_move_type & LO4)
			{
				case DISTATT:
					special_dir = dist_att(dir, op, enemy, part, &rv);
					break;
				case RUNATT:
					special_dir = run_att(dir, op, enemy, part, &rv);
					break;
				case HITRUN:
					special_dir = hitrun_att(dir, op, enemy);
					break;
				case WAITATT:
					special_dir = wait_att(dir, op, enemy, part, &rv);
					break;
					/* why is here non ? */
				case RUSH:
				case ALLRUN:
					break;
				case DISTHIT:
					special_dir = disthit_att(dir, op, enemy, part, &rv);
					break;
				case WAIT2:
					special_dir = wait_att2(dir, op, enemy, part, &rv);
					break;
				default:
					LOG(llevDebug, "Illegal low mon-move: %d\n", op->attack_move_type & LO4);
			}

			if (!special_dir)
				return 0;
		}
	}

	/* try to move closer to enemy, or follow whatever special attack behaviour is */
	if (!QUERY_FLAG(op, FLAG_STAND_STILL) && (QUERY_FLAG(op, FLAG_SCARED) || QUERY_FLAG(op, FLAG_RUN_AWAY) || !can_hit(part, enemy, &rv) || ((op->attack_move_type & LO4) && special_dir != dir)))
	{
		object *aggro_wp = get_aggro_waypoint(op);

		/* TODO: make (intelligent) monsters go to last known position of enemy if out of range/sight */

		/* If special attack move -> follow it instead of going towards enemy */
		if (((op->attack_move_type & LO4) && special_dir != dir))
		{
			aggro_wp = NULL;
			dir = special_dir;
		}

		/* If valid aggro wp (and no special attack), and not scared, use it for movement */
		if (aggro_wp && aggro_wp->enemy && aggro_wp->enemy == op->enemy && rv.distance > 1 && !QUERY_FLAG(op, FLAG_SCARED) && !QUERY_FLAG(op, FLAG_RUN_AWAY))
		{
			waypoint_move(op, aggro_wp);
			return 0;
		}
		else
		{
			int maxdiff = (QUERY_FLAG(op, FLAG_ONLY_ATTACK) || RANDOM() & 1) ? 1 : 2;

			/* Can the monster move directly toward player? */
			if (move_object(op, dir))
				return 0;

			/* Try move around corners if !close */
			for (diff = 1; diff <= maxdiff; diff++)
			{
				/* try different detours */
				/* Try left or right first? */
				int m = 1 - (RANDOM() & 2);
				if (move_object(op, absdir(dir + diff * m)) || move_object(op, absdir(dir - diff * m)))
					return 0;
			}
		}
	}

	/* Eneq(@csd.uu.se): Patch to make RUN_AWAY or SCARED monsters move a random
	 * direction if they can't move away. */
	if (!QUERY_FLAG(op, FLAG_ONLY_ATTACK) && (QUERY_FLAG(op, FLAG_RUN_AWAY) || QUERY_FLAG(op, FLAG_SCARED)))
		if (move_randomly(op))
			return 0;

	/* Hit enemy if possible */
	if (!QUERY_FLAG(op, FLAG_SCARED) && can_hit(part, enemy, &rv))
	{
		if (QUERY_FLAG(op, FLAG_RUN_AWAY))
		{

			part->stats.wc -= 10;
			/* as long we are >0, we are not ready to swing */
			if (op->weapon_speed_left <= 0)
			{
				(void)skill_attack(enemy, part, 0, NULL);
				op->weapon_speed_left += FABS((int)op->weapon_speed_left) + 1;
			}
			part->stats.wc += 10;
		}
		else
		{
			/* as long we are >0, we are not ready to swing */
			if (op->weapon_speed_left <= 0)
			{
				(void)skill_attack(enemy, part, 0, NULL);
				op->weapon_speed_left += FABS((int)op->weapon_speed_left) + 1;
			}
		}
	}

	/* Might be freed by ghost-attack or hit-back */
	if (OBJECT_FREE(part))
		return 1;

	if (QUERY_FLAG(op, FLAG_ONLY_ATTACK))
	{
		destruct_ob(op);
		return 1;
	}

	return 0;
}

/**
 * Returns the nearest living creature (monster or generator).
 * Modified to deal with tiled maps properly.
 * Also fixed logic so that monsters in the lower directions were more
 * likely to be skipped - instead of just skipping the 'start' number
 * of direction, revisit them after looking at all the other spaces.
 *
 * Note that being this may skip some number of spaces, it will
 * not necessarily find the nearest living creature - it basically
 * chooses one from within a 3 space radius, and since it skips
 * the first few directions, it could very well choose something
 * 3 spaces away even though something directly north is closer.
 * @param npc The NPC object looking for nearest living creature
 * @return Nearest living creature, NULL if none nearby
 * @todo can_see_monsterP() is pathfinding function, it does
 * not check visibility. Use obj_in_line_of_sight()? */
object *find_nearest_living_creature(object *npc)
{
	int i, j = 0, start;
	int nx, ny, friendly_attack = TRUE;
	mapstruct *m;
	object *tmp;

	/* must add pet check here too soon */
	/* friendly non berserk unit only attack mobs - not other friendly or players */
	if (!QUERY_FLAG(npc, FLAG_BERSERK) && QUERY_FLAG(npc, FLAG_FRIENDLY))
	{
		friendly_attack = FALSE;
	}

	start = (RANDOM() % 8) + 1;

	for (i = start; j < SIZEOFFREE; j++, i = (i + 1) % SIZEOFFREE)
	{
		nx = npc->x + freearr_x[i];
		ny = npc->y + freearr_y[i];

		if (!(m = out_of_map(npc->map, &nx, &ny)))
		{
			continue;
		}

		/* quick check - if nothing alive or player skip test for targets */
		if (!(GET_MAP_FLAGS(m, nx, ny) & (P_IS_ALIVE | P_IS_PLAYER)))
		{
			continue;
		}

		tmp = get_map_ob(m, nx, ny);

		/* attack player & friendly */
		if (friendly_attack)
		{
			/* attack all - monster, player & friendly - loop more is not monster AND not player */
			while (tmp != NULL && !QUERY_FLAG(tmp, FLAG_MONSTER) && tmp->type != PLAYER)
			{
				tmp = tmp->above;
			}
		}
		else
		{
			/* loop on when not monster or player or friendly */
			while (tmp != NULL && (!QUERY_FLAG(tmp, FLAG_MONSTER) || tmp->type == PLAYER || (QUERY_FLAG(tmp, FLAG_FRIENDLY))))
			{
				tmp = tmp->above;
			}
		}

		/* can see monster is a path finding function! it not checks visibility! */
		if (tmp && can_see_monsterP(m, nx, ny, i))
		{
			return tmp;
		}
	}

	/* nothing found */
	return NULL;
}

/**
 * Randomly move a monster.
 * @param op The monster object to move
 * @return 1 if the monster was moved, 0 otherwise */
int move_randomly(object *op)
{
	int i, r, xt, yt;
	object *base = find_base_info_object(op);

	/* Give up to 8 chances for a monster to move randomly */
	for (i = 0; i < 8; i++)
	{
		r = RANDOM() % 8 + 1;

		/* check x direction of possible move */
		if (op->item_race != 255)
		{
			if (!op->item_race && freearr_x[r])
			{
				continue;
			}

			xt = op->x + freearr_x[r];

			if (abs(xt - base->x) >op->item_race)
			{
				continue;
			}
		}

		/* check x direction of possible move */
		if (op->item_level != 255)
		{
			if (!op->item_level && freearr_y[r])
			{
				continue;
			}

			yt = op->y + freearr_y[r];

			if (abs(yt - base->y) > op->item_level)
			{
				continue;
			}
		}

		if (move_object(op, r))
			return 1;
	}

	return 0;
}

/**
 * Check if object can hit another object.
 * @param ob1 Monster object
 * @param ob2 The object to check if monster can hit
 * @param rv Range vector
 * @return 1 if can hit, 0 otherwise */
int can_hit(object *ob1, object *ob2, rv_vector *rv)
{
	(void) ob2;

	if (QUERY_FLAG(ob1, FLAG_CONFUSED) && !(RANDOM() % 3))
	{
		return 0;
	}

	return abs(rv->distance_x) < 2 && abs(rv->distance_y) < 2;
}

/**
 * Check if monster can apply an item.
 *
 * Someday we may need this check.
 * @param who The monster object
 * @param item The item to apply
 * @return Always returns 1 */
int can_apply(object *who, object *item)
{
	(void) who;
	(void) item;

	return 1;
}

#define MAX_KNOWN_SPELLS 20

/**
 * Choose a random spell this monster could cast.
 * @param monster The monster object
 * @return Random spell object, NULL if no spell found */
object *monster_choose_random_spell(object *monster)
{
	object *altern[MAX_KNOWN_SPELLS];
	object *tmp;
	spell *spell;
	int i = 0, j;

	for (tmp = monster->inv; tmp != NULL; tmp = tmp->below)
	{
		if (tmp->type == ABILITY || tmp->type == SPELLBOOK)
		{
			/* Check and see if it's actually a useful spell */
			if ((spell = find_spell(tmp->stats.sp)) != NULL && !(spell->path & (PATH_INFO | PATH_TRANSMUTE | PATH_TRANSFER | PATH_LIGHT)))
			{
				if (tmp->stats.maxsp)
				{
					for (j = 0; i < MAX_KNOWN_SPELLS && j < tmp->stats.maxsp; j++)
					{
						altern[i++] = tmp;
					}
				}
				else
				{
					altern[i++] = tmp;
				}

				if (i == MAX_KNOWN_SPELLS)
				{
					break;
				}
			}

		}
	}

	if (!i)
	{
		return NULL;
	}

	return altern[RANDOM() % i];
}

int monster_cast_spell(object *head, object *part, object *pl, int dir, rv_vector *rv)
{
	object *spell_item;
	spell *sp;
	int sp_typ, ability;

	(void) pl;

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	if ((spell_item = monster_choose_random_spell(head)) == NULL)
	{
		LOG(llevDebug, "DEBUG: monster_cast_spell: No spell found! Turned off spells in %s (%s) (%d,%d)\n", query_name(head, NULL), head->map ? (head->map->name ? head->map->name : "<no map name>") : "<no map!>", head->x, head->y );
		/* Will be turned on when picking up book */
		CLEAR_FLAG(head, FLAG_CAST_SPELL);
		return 0;
	}

	if (spell_item->stats.hp)
	{
		/* Alternate long-range spell: check how far away enemy is */
		if (rv->distance > 6)
			sp_typ = spell_item->stats.hp;
		else
			sp_typ = spell_item->stats.sp;
	}
	else
		sp_typ = spell_item->stats.sp;

	if ((sp = find_spell(sp_typ)) == NULL)
	{
		LOG(llevDebug,"DEBUG: monster_cast_spell: Can't find spell #%d for mob %s (%s) (%d,%d)\n", sp_typ, query_name(head, NULL), head->map ? (head->map->name ? head->map->name : "<no map name>") : "<no map!>", head->x, head->y);
		return 0;
	}

	/* Spell should be cast on caster (ie, heal, strength) */
	if (sp->flags & SPELL_DESC_SELF)
		dir = 0;

	/* Monster doesn't have enough spell-points */
	if (head->stats.sp < SP_level_spellpoint_cost(head, head, sp_typ))
		return 0;

	/* Note: in cf is possible to give the mob a spellbook - this will be used
	 * as "spell source" (aka ability object like) too. I will remove this -
	 * using special prepared stuff like this is more useful.
	 * Also i noticed tha "long range stuff" - can this be handled from
	 * spellbooks too??? */
	ability = (spell_item->type == ABILITY && QUERY_FLAG(spell_item, FLAG_IS_MAGICAL));

	/* If we cast a spell, only use up casting_time speed */
	/*head->speed_left += (float)1.0 - (float) spells[sp_typ].time / (float)20.0 * (float)FABS(head->speed); */

	head->stats.sp -= SP_level_spellpoint_cost(head, head, sp_typ);

	/* add default cast time from spell force to monster */
	head->last_grace += spell_item->last_grace;

	/*LOG(-1,"CAST2: dir:%d (%d)- target:%s\n", dir, rv->direction, query_name(head->enemy) );*/
	return cast_spell(part, part, dir, sp_typ, ability, spellNormal, NULL);
}

/* monster_use_skill()-implemented 95-04-28 to allow monster skill use.
 * Note that monsters do not need the skills SK_MELEE_WEAPON and
 * SK_MISSILE_WEAPON to make those respective attacks, if we
 * required that we would drastically increase the memory
 * requirements of CF!!
 *
 * The skills we are treating here are all but those. -b.t.
 *
 * At the moment this is only useful for throwing, perhaps for
 * stealing. TODO: This should be more integrated in the game. -MT, 25.11.01 */
int monster_use_skill(object *head, object *part, object *pl, int dir)
{
	object *skill, *owner;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
		return 0;

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with skill - thrown rocks for example? */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 1)
			return 0;
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	/* skill selection - monster will use the next unused skill.
	 * well...the following scenario will allow the monster to
	 * toggle between 2 skills. One day it would be nice to make
	 * more skills available to monsters. */

	for (skill = head->inv; skill != NULL; skill = skill->below)
	{
		if (skill->type == SKILL && skill != head->chosen_skill)
		{
			head->chosen_skill = skill;
			break;
		}
	}

	if (!skill && !head->chosen_skill)
	{
		LOG(llevDebug, "Error: Monster %s (%d) has FLAG_READY_SKILL without skill.\n", head->name, head->count);
		CLEAR_FLAG(head, FLAG_READY_SKILL);
		return 0;
	}

	/* use skill */
	return do_skill(head, dir, NULL);
}


/* For the future: Move this function together with case 3: in fire() */
int monster_use_wand(object *head, object *part, object *pl, int dir)
{
	object *wand, *owner;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
		return 0;

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with spell */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 2)
			return 0;
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	for (wand = head->inv; wand != NULL; wand = wand->below)
		if (wand->type == WAND && QUERY_FLAG(wand, FLAG_APPLIED))
			break;

	if (wand == NULL)
	{
		LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_WAND() without wand.\n", query_name(head, NULL), head->count);
		CLEAR_FLAG(head, FLAG_READY_RANGE);
		return 0;
	}

	if (wand->stats.food <= 0)
	{
		manual_apply(head, wand, 0);
		CLEAR_FLAG(head, FLAG_READY_RANGE);

		if (wand->arch)
		{
			CLEAR_FLAG(wand, FLAG_ANIMATE);
			wand->face = wand->arch->clone.face;
			wand->speed = 0;
			update_ob_speed(wand);
		}
		return 0;
	}

	if (cast_spell(part, wand, dir, wand->stats.sp, 0, spellWand, NULL))
	{
		wand->stats.food--;
		return 1;
	}
	return 0;
}

int monster_use_rod(object *head, object *part, object *pl, int dir)
{
	object *rod, *owner;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
		return 0;

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with spell */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 2)
			return 0;
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	for (rod = head->inv; rod != NULL; rod = rod->below)
		if (rod->type == ROD && QUERY_FLAG(rod, FLAG_APPLIED))
			break;

	if (rod == NULL)
	{
		LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_ROD() without rod.\n", query_name(head, NULL), head->count);
		CLEAR_FLAG(head, FLAG_READY_RANGE);
		return 0;
	}

	/* Not recharged enough yet */
	if (rod->stats.hp < spells[rod->stats.sp].sp)
		return 0;

	if (cast_spell(part, rod, dir, rod->stats.sp, 0, spellRod, NULL))
	{
		drain_rod_charge(rod);
		return 1;
	}

	return 0;
}

int monster_use_horn(object *head, object *part, object *pl, int dir)
{
	object *horn, *owner;
	rv_vector rv;

	if (!(dir = path_to_player(part, pl, 0)))
		return 0;

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		/* Might hit owner with spell */
		if (get_rangevector(head, owner, &rv, 0) && dirdiff(dir, rv.direction) < 2)
			return 0;
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	for (horn = head->inv; horn != NULL; horn = horn->below)
		if (horn->type == ROD && QUERY_FLAG(horn, FLAG_APPLIED))
			break;

	if (horn == NULL)
	{
		LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_HORN() without horn.\n", query_name(head, NULL), head->count);
		CLEAR_FLAG(head, FLAG_READY_RANGE);
		return 0;
	}

	/* Not recharged enough yet */
	if (horn->stats.hp < spells[horn->stats.sp].sp)
		return 0;

	if (cast_spell(part, horn, dir, horn->stats.sp, 0, spellHorn, NULL))
	{
		drain_rod_charge(horn);
		return 1;
	}

	return 0;
}

int monster_use_bow(object *head, object *part, object *pl, int dir)
{
	object *bow, *arrow;
	int tag;

	(void) pl;

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

	for (bow = head->inv; bow != NULL; bow = bow->below)
		if (bow->type==BOW && QUERY_FLAG(bow, FLAG_APPLIED))
			break;

	if (bow == NULL)
	{
		LOG(llevBug, "BUG: Monster %s (%d) HAS_READY_BOW() without bow.\n", query_name(head, NULL), head->count);
		CLEAR_FLAG(head, FLAG_READY_BOW);
		return 0;
	}

	if ((arrow = find_arrow(head, bow->race)) == NULL)
	{
		/* Out of arrows */
		manual_apply(head, bow, 0);
		CLEAR_FLAG(head, FLAG_READY_BOW);
		return 0;
	}

	/* thats an infinitve arrow! dupe it. */
	if (QUERY_FLAG(arrow, FLAG_SYS_OBJECT))
	{
		object *new_arrow = get_object();
		copy_object(arrow, new_arrow);
		CLEAR_FLAG(new_arrow, FLAG_SYS_OBJECT);
		new_arrow->nrof = 0;

		/* now setup the self destruction */
		new_arrow->stats.food = 20;
		arrow = new_arrow;
	}
	else
		arrow = get_split_ob(arrow, 1);

	set_owner(arrow, head);
	arrow->direction = dir;
	arrow->x = part->x, arrow->y = part->y;
	arrow->speed = 1;
	update_ob_speed(arrow);
	arrow->speed_left = 0;
	SET_ANIMATION(arrow, (NUM_ANIMATIONS(arrow) / NUM_FACINGS(arrow)) * dir);
	arrow->level = head->level;
	/* save original wc and dam */
	arrow->last_heal = arrow->stats.wc;
	arrow->stats.hp = arrow->stats.dam;
	/* NO_STRENGTH */
	arrow->stats.dam += bow->stats.dam + bow->magic + arrow->magic;
	arrow->stats.dam = FABS((int)((float)(arrow->stats.dam * lev_damage[head->level])));
	arrow->stats.wc = 10 + (bow->magic + bow->stats.wc + arrow->magic + arrow->stats.wc-head->level);
	arrow->stats.wc_range = bow->stats.wc_range;
	arrow->map = head->map;
	/* we use fixed value for mobs */
	arrow->last_sp = 12;
	SET_FLAG(arrow, FLAG_FLYING);
	SET_FLAG(arrow, FLAG_IS_MISSILE);
	SET_FLAG(arrow, FLAG_FLY_ON);
	SET_FLAG(arrow, FLAG_WALK_ON);
	tag = arrow->count;
	arrow->stats.grace = arrow->last_sp;
	arrow->stats.maxgrace = 60 + (RANDOM() % 12);
	play_sound_map(arrow->map, arrow->x, arrow->y, SOUND_THROW, SOUND_NORMAL);

	if (!insert_ob_in_map(arrow, head->map, head, 0))
		return 1;

	if (!was_destroyed(arrow, tag))
		move_arrow(arrow);

	return 1;
}

int check_good_weapon(object *who, object *item)
{
	object *other_weap;
	int prev_dam = who->stats.dam;

	for (other_weap = who->inv; other_weap != NULL; other_weap = other_weap->below)
		if (other_weap->type == item->type && QUERY_FLAG(other_weap, FLAG_APPLIED))
			break;

	/* No other weapons */
	if (other_weap == NULL)
		return 1;

	if (monster_apply_special(who, item, 0))
	{
		LOG(llevMonster, "Can't wield %s(%d).\n", item->name, item->count);
		return 0;
	}

	if (who->stats.dam < prev_dam && !OBJECT_FREE(other_weap))
	{
		/* New weapon was worse.  (Note ^: Could have been freed by merging) */
		if (monster_apply_special(who, other_weap, 0))
			LOG(llevMonster, "Can't rewield %s(%d).\n", item->name, item->count);

		return 0;
	}

	return 1;
}

int check_good_armour(object *who, object *item)
{
	object *other_armour;
	int prev_ac = who->stats.ac;

	for (other_armour = who->inv; other_armour != NULL; other_armour = other_armour->below)
		if (other_armour->type == item->type && QUERY_FLAG(other_armour, FLAG_APPLIED))
			break;

	/* No other armour, use the new */
	if (other_armour == NULL)
		return 1;

	if (monster_apply_special(who, item, 0))
	{
		LOG(llevMonster, "Can't take off %s(%d).\n", item->name, item->count);
		return 0;
	}

	if (who->stats.ac > prev_ac && !OBJECT_FREE(other_armour))
	{
		/* New armour was worse. *Note ^: Could have been freed by merging) */
		if (monster_apply_special(who, other_armour, 0))
			LOG(llevMonster, "Can't rewear %s(%d).\n", item->name, item->count);

		return 0;
	}

	return 1;
}

/* monster_check_pickup(): checks for items that monster can pick up.
 *
 * Vick's (vick@bern.docs.uu.se) fix 921030 for the sweeper blob.
 * Each time the blob passes over some treasure, it will
 * grab it a.s.a.p.
 *
 * Eneq(@csd.uu.se): This can now be defined in the archetypes, added code
 * to handle this.
 *
 * This function was seen be continueing looping at one point (tmp->below
 * became a recursive loop.  It may be better to call monster_check_apply
 * after we pick everything up, since that function may call others which
 * affect stacking on this space. */

void monster_check_pickup(object *monster)
{
	object *tmp,*next;
	int next_tag = 0;

	for (tmp = monster->below; tmp != NULL; tmp = next)
	{
		next = tmp->below;
		if (next)
			next_tag = next->count;

		if (monster_can_pick(monster, tmp))
		{
			remove_ob(tmp);
			if (check_walk_off(tmp, NULL, MOVE_APPLY_DEFAULT) != CHECK_WALK_OK)
				return;

			tmp = insert_ob_in_ob(tmp, monster);

			/* Trigger the PICKUP event */
			if (trigger_event(EVENT_PICKUP, monster, tmp, monster, NULL, (int *) tmp->nrof, 0, 0, SCRIPT_FIX_ALL))
			{
				return;
			}

			(void) monster_check_apply(monster, tmp);
		}
		/* We could try to re-establish the cycling, of the space, but probably
		 * not a big deal to just bail out. */
		if (next && was_destroyed(next, next_tag))
			return;
	}
}

/* monster_can_pick(): If the monster is interested in picking up
 * the item, then return 0.  Otherwise 0.
 * Instead of pick_up, flags for "greed", etc, should be used.
 * I've already utilized flags for bows, wands, rings, etc, etc. -Frank. */

int monster_can_pick(object *monster, object *item)
{
	int flag = 0;

	if (!can_pick(monster, item))
		return 0;

	if (QUERY_FLAG(item, FLAG_UNPAID))
		return 0;

	/* All */
	if (monster->pick_up & 64)
		flag = 1;
	else
	{
		switch (item->type)
		{
			case MONEY:
			case GEM:
			case TYPE_JEWEL:
			case TYPE_NUGGET:
				flag = monster->pick_up & 2;
				break;

			case FOOD:
				flag = monster->pick_up & 4;
				break;

			case WEAPON:
				flag = (monster->pick_up & 8) || QUERY_FLAG(monster, FLAG_USE_WEAPON);
				break;

			case ARMOUR:
			case SHIELD:
			case HELMET:
			case BOOTS:
			case GLOVES:
			case GIRDLE:
				flag = (monster->pick_up & 16) || QUERY_FLAG(monster, FLAG_USE_ARMOUR);
				break;

			case SKILL:
				flag = QUERY_FLAG(monster, FLAG_CAN_USE_SKILL);
				break;

			case RING:
				flag = QUERY_FLAG(monster, FLAG_USE_RING);
				break;

			case WAND:
				flag = QUERY_FLAG(monster, FLAG_USE_RANGE);
				break;

			case SPELLBOOK:
				flag = (monster->arch != NULL && QUERY_FLAG((&monster->arch->clone), FLAG_CAST_SPELL));
				break;

			case BOW:
			case ARROW:
				flag = QUERY_FLAG(monster, FLAG_USE_BOW);
				break;
		}
	}

	if (((!(monster->pick_up & 32)) && flag) || ((monster->pick_up & 32) && (!flag)))
		return 1;

	return 0;
}

/* monster_apply_below():
 * Vick's (vick@bern.docs.uu.se) @921107 -> If a monster who's
 * eager to apply things, encounters something apply-able,
 * then make him apply it */

void monster_apply_below(object *monster)
{
	object *tmp, *next;

	for (tmp = monster->below; tmp != NULL; tmp = next)
	{
		next = tmp->below;
		switch (tmp->type)
		{
			case CF_HANDLE:
			case TRIGGER:
				if (monster->will_apply & 1)
					manual_apply(monster, tmp, 0);
				break;
			case TREASURE:
				if (monster->will_apply & 2)
					manual_apply(monster, tmp, 0);
				break;
				/* Ideally, they should wait until they meet a player */
			case SCROLL:
				if (QUERY_FLAG(monster, FLAG_USE_SCROLL))
					manual_apply(monster, tmp, 0);
				break;
		}

		if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
			break;
	}
}

/* monster_check_apply() is meant to be called after an item is
 * inserted in a monster.
 * If an item becomes outdated (monster found a better item),
 * a pointer to that object is returned, so it can be dropped.
 * (so that other monsters can pick it up and use it) */

/* Sept 96, fixed this so skills will be readied -b.t.*/

/* scary function - need rework. even in crossfire its changed now */
void monster_check_apply(object *mon, object *item)
{
	/* and because its scary, we stop here... */
	/* this function is simply to bad - for example will potions applied
	 * not depending on the situation... why applying a heal potion when
	 * full hp? firestorm potion when standing next to own people?
	 * IF we do some AI stuff here like using items we must FIRST
	 * add a AI - then doing the things. Think first, act later! */
	if (1)
		return;

	if (item->type == SPELLBOOK && mon->arch != NULL && (QUERY_FLAG((&mon->arch->clone), FLAG_CAST_SPELL)))
	{
		SET_FLAG(mon, FLAG_CAST_SPELL);
		return;
	}

	/* Check for the right kind of bow */
	if (QUERY_FLAG(mon, FLAG_USE_BOW) && item->type == ARROW)
	{
		object *bow;
		for (bow = mon->inv; bow != NULL; bow = bow->below)
		{
			if (bow->type == BOW && bow->race == item->race)
			{
				SET_FLAG(mon, FLAG_READY_BOW);
				LOG(llevMonster, "Found correct bow for arrows.\n");

				if (!QUERY_FLAG(bow, FLAG_APPLIED))
					manual_apply(mon, bow, 0);

				break;
			}
		}
	}

	/* hm, this should all handled in can_apply... need rework later MT-11-2002 */
	if (can_apply(mon, item))
	{
		int flag = 0;
		switch (item->type)
		{
			case TREASURE:
				flag = 0;
				break;

			case POTION:
				flag = 0;
				break;

				/* Can a monster eat food ?  Yes! (it heals) */
			case FOOD:
				flag = 0;
				break;

			case WEAPON:
				/* Apply only if it's a better weapon than the used one.
				 * All "standard" monsters need to adjust their wc to use the can_apply on
				 * weapons. */
				flag = QUERY_FLAG(mon, FLAG_USE_WEAPON) && check_good_weapon(mon, item);
				break;

			case ARMOUR:
			case HELMET:
			case SHIELD:
				flag = (QUERY_FLAG(mon, FLAG_USE_ARMOUR) && check_good_armour(mon, item));
				break;

			case SKILL:
				if ((flag = QUERY_FLAG(mon, FLAG_CAN_USE_SKILL)))
				{
					if (!QUERY_FLAG(item, FLAG_APPLIED))
						manual_apply(mon, item, 0);

					if (item->type == SKILL && present_in_ob(SKILL, mon) != NULL)
						SET_FLAG(mon, FLAG_READY_SKILL);
				}
				break;

			case RING:
				flag = QUERY_FLAG(mon, FLAG_USE_RING);
				break;

			case WAND:
				flag = QUERY_FLAG(mon, FLAG_USE_RANGE);
				break;

			case BOW:
				flag = QUERY_FLAG(mon, FLAG_USE_BOW);
		}

		if (flag)
		{
			if (!QUERY_FLAG(item, FLAG_APPLIED))
				manual_apply(mon, item, 0);

			if (item->type == BOW && present_in_ob((unsigned char) item->stats.maxsp, mon) != NULL)
				SET_FLAG(mon, FLAG_READY_BOW);
		}
		return;
	}

	return;
}

void npc_call_help(object *op)
{
	int x, y, xt, yt;
	mapstruct *m;
	object *npc;

	for (x = -3; x < 4; x++)
	{
		for (y = -3; y < 4; y++)
		{
			xt = op->x + x;
			yt = op->y + y;

			if (!(m = out_of_map(op->map, &xt, &yt)))
				continue;

			for (npc = get_map_ob(m, xt, yt); npc != NULL; npc = npc->above)
				if (QUERY_FLAG(npc, FLAG_ALIVE) && QUERY_FLAG(npc, FLAG_UNAGGRESSIVE))
					set_npc_enemy(npc, op->enemy, NULL);
		}
	}
}


int dist_att(int dir, object *ob, object *enemy, object *part, rv_vector *rv)
{
	(void) ob;

	if (can_hit(part, enemy, rv))
		return dir;

	if (rv->distance < 10)
		return absdir(dir + 4);
	else if (rv->distance > 18)
		return dir;

	return 0;
}

int run_att(int dir, object *ob, object *enemy, object *part, rv_vector *rv)
{
	if ((can_hit(part, enemy, rv) && ob->move_status < 20) || ob->move_status < 20)
	{
		ob->move_status++;
		return (dir);
	}
	else if (ob->move_status > 20)
		ob->move_status = 0;

	return absdir(dir + 4);
}

int hitrun_att (int dir, object *ob, object *enemy)
{
	(void) enemy;

	if (ob->move_status++ < 25)
		return dir;
	else if (ob->move_status < 50)
		return absdir(dir + 4);
	else
		ob->move_status = 0;

	return absdir(dir + 4);
}

int wait_att(int dir, object *ob, object *enemy, object *part, rv_vector *rv)
{
	int inrange = can_hit(part, enemy, rv);

	if (ob->move_status || inrange)
		ob->move_status++;

	if (ob->move_status == 0)
		return 0;
	else if (ob->move_status < 10)
		return dir;
	else if (ob->move_status < 15)
		return absdir(dir + 4);

	ob->move_status = 0;
	return 0;
}

int disthit_att(int dir, object *ob, object *enemy, object *part, rv_vector *rv)
{
	/* The logic below here looked plain wrong before.  Basically, what should
	 * happen is that if the creatures hp percentage falls below run_away,
	 * the creature should run away (dir + 4)
	 * I think its wrong for a creature to have a zero maxhp value, but
	 * at least one map has this set, and whatever the map contains, the
	 * server should try to be resilant enough to avoid the problem */
	if (ob->stats.maxhp && (ob->stats.hp * 100) / ob->stats.maxhp < ob->run_away)
		return absdir(dir + 4);

	return dist_att(dir, ob,enemy, part, rv);
}

int wait_att2(int dir, object *ob, object *enemy, object *part, rv_vector *rv)
{
	(void) ob;
	(void) enemy;
	(void) part;

	if (rv->distance < 9)
		return absdir(dir + 4);

	return 0;
}

void circ1_move (object *ob)
{
	static int circle[12] = {3, 3, 4, 5, 5, 6, 7, 7, 8, 1, 1, 2};

	if (++ob->move_status > 11)
		ob->move_status = 0;

	if (!(move_object(ob, circle[ob->move_status])))
		(void) move_object(ob, RANDOM() % 8 + 1);
}

void circ2_move (object *ob)
{
	static int circle[20] = {3, 3, 3, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 1, 1, 1, 2, 2};

	if (++ob->move_status > 19)
		ob->move_status = 0;

	if (!(move_object(ob, circle[ob->move_status])))
		(void) move_object(ob, RANDOM() % 8 + 1);
}

void pace_movev(object *ob)
{
	if (ob->move_status++ > 6)
		ob->move_status = 0;

	if (ob->move_status < 4)
		(void) move_object(ob, 5);
	else
		(void) move_object(ob, 1);
}

void pace_moveh(object *ob)
{
	if (ob->move_status++ > 6)
		ob->move_status = 0;

	if (ob->move_status < 4)
		(void) move_object(ob, 3);
	else
		(void) move_object(ob, 7);
}

void pace2_movev(object *ob)
{
	if (ob->move_status++ > 16)
		ob->move_status = 0;

	if (ob->move_status < 6)
		(void) move_object(ob, 5);
	else if (ob->move_status < 8)
		return;
	else if (ob->move_status < 13)
		(void) move_object(ob, 1);
	else
		return;
}

void pace2_moveh(object *ob)
{
	if (ob->move_status++ > 16)
		ob->move_status = 0;

	if (ob->move_status < 6)
		(void) move_object(ob, 3);
	else if (ob->move_status < 8)
		return;
	else if (ob->move_status < 13)
		(void) move_object(ob, 7);
	else
		return;
}

void rand_move(object *ob)
{
	int i;
	if (ob->move_status < 1 || ob->move_status > 8 || !(move_object(ob, ob->move_status || !(RANDOM() % 9))))
	{
		for (i = 0; i < 5; i++)
			if (move_object(ob, ob->move_status = RANDOM() % 8 + 1))
				return;
	}
}

static void free_messages(msglang *msgs)
{
	int messages, keywords;

	if (!msgs)
		return;

	for (messages = 0; msgs->messages[messages]; messages++)
	{
		if (msgs->keywords[messages])
		{
			for (keywords = 0; msgs->keywords[messages][keywords]; keywords++)
				free(msgs->keywords[messages][keywords]);

			free(msgs->keywords[messages]);
		}
		free(msgs->messages[messages]);
	}

	free(msgs->messages);
	free(msgs->keywords);
	free(msgs);
}

static msglang *parse_message(const char *msg)
{
	msglang *msgs;
	int nrofmsgs, msgnr, i;
	char *cp, *line, *last, *tmp;
	char *buf = strdup_local(msg);

	/* First find out how many messages there are.  A @ for each. */
	for (nrofmsgs = 0, cp = buf; *cp; cp++)
		if (*cp == '@')
			nrofmsgs++;

	if (!nrofmsgs)
	{
		free(buf);
		return NULL;
	}

	msgs = (msglang *) malloc(sizeof(msglang));
	msgs->messages = (char **) malloc(sizeof(char *) * (nrofmsgs + 1));
	msgs->keywords = (char ***) malloc(sizeof(char **) * (nrofmsgs + 1));
	for (i = 0; i <= nrofmsgs; i++)
	{
		msgs->messages[i] = NULL;
		msgs->keywords[i] = NULL;
	}

	for (last = NULL, cp = buf, msgnr = 0;*cp; cp++)
	{
		if (*cp == '@')
		{
			int nrofkeywords, keywordnr;
			*cp = '\0';
			cp++;
			if (last != NULL)
			{
				msgs->messages[msgnr++] = strdup_local(last);
				tmp = msgs->messages[msgnr - 1];
				for (i = (int)strlen(tmp); i; i--)
				{
					if (*(tmp + i) && *(tmp + i) != 0x0a && *(tmp + i) != 0x0d)
						break;

					*(tmp + i) = 0;
				}
			}

			if (strncmp(cp, "match", 5))
			{
				LOG(llevBug, "BUG: parse_message(): Unsupported command in message.\n");
				free(buf);
				return NULL;
			}

			for (line = cp + 6, nrofkeywords = 0; *line != '\n' && *line; line++)
				if (*line == '|')
					nrofkeywords++;

			if (line > cp + 6)
				nrofkeywords++;

			if (nrofkeywords < 1)
			{
				LOG(llevBug, "BUG: parse_message(): Too few keywords in message.\n");
				free(buf);
				free_messages(msgs);
				return NULL;
			}

			msgs->keywords[msgnr] = (char **) malloc(sizeof(char **) * (nrofkeywords +1));
			msgs->keywords[msgnr][nrofkeywords] = NULL;
			last = cp + 6;
			cp = strchr(cp, '\n');
			if (cp != NULL)
				cp++;

			for (line = last, keywordnr = 0;line<cp && *line;line++)
			{
				if (*line == '\n' || *line == '|')
				{
					*line = '\0';
					if (last != line)
						msgs->keywords[msgnr][keywordnr++] = strdup_local(last);
					else
					{
						if (keywordnr < nrofkeywords)
						{
							/* Whoops, Either got || or |\n in @match. Not good */
							msgs->keywords[msgnr][keywordnr++] = strdup_local("xxxx");
							/* We need to set the string to something sensible to
							 * prevent crashes later. Unfortunately, we can't set to
							 * NULL, as that's used to terminate the for loop in
							 * talk_to_npc.  Using xxxx should also help map
							 * developers track down the problem cases. */
							LOG(llevBug, "BUG: parse_message(): Tried to set a zero length message in parse_message\n");
							/* I think this is a error worth reporting at a reasonably
							 * high level. When logging gets redone, this should
							 * be something like MAP_ERROR, or whatever gets put in
							 * place. */
							if (keywordnr > 1)
								/* This is purely addtional information, should only be gieb if asked */
								LOG(llevDebug, "Msgnr %d, after keyword %s\n", msgnr + 1, msgs->keywords[msgnr][keywordnr - 2]);
							else
								LOG(llevDebug, "Msgnr %d, first keyword\n", msgnr + 1);
						}
					}

					last = line + 1;
				}
			}

			/* your eyes aren't decieving you, this is code repetition.  However,
			 * the above code doesn't catch the case where line<cp going into the
			 * for loop, skipping the above code completely, and leaving undefined
			 * data in the keywords array.  This patches it up and solves a crash
			 * bug.  garbled 2001-10-20 */

			if (keywordnr < nrofkeywords)
			{
				LOG(llevBug, "BUG: parse_message(): Map developer screwed up match statement in parse_message\n");
				if (keywordnr > 1)
					/* This is purely addtional information, should  only be gieb if asked */
					LOG(llevDebug, "Msgnr %d, after keyword %s\n", msgnr + 1, msgs->keywords[msgnr][keywordnr - 2]);
				else
					LOG(llevDebug, "Msgnr %d, first keyword\n", msgnr + 1);
			}
			last = cp;
		}
	}

	if (last != NULL)
		msgs->messages[msgnr++] = strdup_local(last);

	tmp = msgs->messages[msgnr - 1];

	for (i = (int)strlen(tmp); i; i--)
	{
		if (*(tmp + i) && *(tmp + i) != 0x0a && *(tmp + i) != 0x0d)
			break;

		*(tmp + i) = 0;
	}
	free(buf);
	return msgs;
}

/* i changed this... This function is not to understimate when player talk alot
 * in areas which alot if map objects... This is one of this little extra cpu eaters
 * which adds cput time here and there.
 * i added P_MAGIC_EAR as map flag - later we should use a chained list in the map headers
 * perhaps. I also removed the npcs from the map search and use the target system.
 * This IS needed because in alot of cases in the past you was not able to target the
 * npc you want - if the search routine find another npc first, the other was silenced.
 * MT-2003 */
void communicate(object *op, char *txt)
{
	object *npc;
	mapstruct *m;
	int i, xt, yt;

	char buf[HUGE_BUF];

	if (!txt)
		return;


	/* with target, only player can talk to npc... for npc to npc talk we need so or so a script,
	 * and there we have then to add the extra interface. */

	/* thats the whisper code - i will add a /whisper for it and remove it from here */
#if 0
	if (op->type == PLAYER)
	{
		if (op->contr->target_object && op->contr->target_object_count == op->contr->target_object->count)
		{
			if (op->contr->target_object->type == PLAYER)
			{
				if (op != op->contr->target_object)
				{
					sprintf(buf, "%s whispers to you: ", query_name(op));
					strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
					buf[MAX_BUF - 1] = 0;
					new_draw_info(NDI_WHITE | NDI_GREEN, 0, op->contr->target_object, buf);
					sprintf(buf, "you whisper to %s: ", query_name(op->contr->target_object));
					strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
					buf[MAX_BUF - 1] = 0;
					new_draw_info(NDI_WHITE | NDI_GREEN, 0, op, buf);
					sprintf(buf, "%s whispers something to %s.", query_name(op), query_name(op->contr->target_object));
					new_info_map_except2(NDI_WHITE, op->map, op, op->contr->target_object, buf);
					if (op->contr->target_object->map && op->contr->target_object->map != op->map)
						new_info_map_except2(NDI_WHITE, op->contr->target_object->map, op, op->contr->target_object, buf);
				}
				else
				{
					sprintf(buf, "%s says: ", query_name(op));
					strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
					buf[MAX_BUF - 1] = 0;
					new_info_map(NDI_WHITE, op->map, buf);
				}
			}
			else
			{
				sprintf(buf, "%s says to %s: ", query_name(op), query_name(op->contr->target_object));
				strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
				buf[MAX_BUF - 1] = 0;
				new_info_map_except(NDI_WHITE, op->map, op, buf);
				if (op->contr->target_object->map && op->contr->target_object->map != op->map)
					new_info_map_except(NDI_WHITE, op->contr->target_object->map, op, buf);

				sprintf(buf, "you say to %s: ", query_name(op->contr->target_object));
				strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
				buf[MAX_BUF - 1] = 0;
				new_draw_info(NDI_WHITE, 0, op, buf);
				talk_to_npc(op, op->contr->target_object, txt);
			}
		}
		else
		{
			sprintf(buf, "%s says: ", query_name(op));
			strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
			buf[MAX_BUF - 1] = 0;
			new_info_map(NDI_WHITE, op->map, buf);
		}
	}
	else
	{
		sprintf(buf, "%s says: ", query_name(op));
		strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
		buf[MAX_BUF - 1] = 0;
		new_info_map(NDI_WHITE, op->map, buf);
	}
#endif

	/* npc chars can hook in here with
	 * monster.Communicate("/kiss Fritz")
	 * we need to catch the emote here. */
	if (*txt == '/' && op->type != PLAYER)
	{
		CommArray_s *csp;
		char *cp = NULL;

		/* remove the command from the parameters */
		strncpy(buf,txt,HUGE_BUF - 1);
		buf[HUGE_BUF-1] = '\0';

		cp = strchr(buf, ' ');

		if (cp)
		{
			*(cp++) = '\0';
			cp = cleanup_string(cp);
			if (cp && *cp == '\0')
				cp = NULL;
		}

		csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);
		if (csp)
		{
			csp->func(op, cp);
			return;
		}
		return;
	}

	sprintf(buf, "%s says: ", query_name(op, NULL));
	strncat(buf, txt, MAX_BUF - strlen(buf) - 1);
	buf[MAX_BUF - 1] = 0;
	if (op->type == PLAYER)
		new_info_map(NDI_WHITE | NDI_PLAYER, op->map, op->x, op->y, MAP_INFO_NORMAL, buf);
	else
		new_info_map(NDI_WHITE, op->map, op->x, op->y, MAP_INFO_NORMAL, buf);

	for (i = 0; i <= SIZEOFFREE2; i++)
	{
		xt = op->x + freearr_x[i];
		yt = op->y + freearr_y[i];
		if ((m = out_of_map(op->map, &xt, &yt)))
		{
			/* quick check we have a magic ear */
			if (GET_MAP_FLAGS(m, xt, yt) & (P_MAGIC_EAR | P_IS_ALIVE))
			{
				/* ok, browse now only on demand */
				for (npc = get_map_ob(m, xt, yt); npc != NULL; npc = npc->above)
				{
					/* avoid talking to self */
					if (op != npc)
					{
						/* the ear ... don't break because it can be mutiple on a node or more in the area */
						if (npc->type == MAGIC_EAR)
							(void) talk_to_wall(npc, txt);
						else if (QUERY_FLAG(npc, FLAG_ALIVE))
							talk_to_npc(op, npc, txt);
					}
				}
			}
		}
	}
}

/* this communication thingy is ugly - ugly in the way it use malloc for getting
 * buffers for storing parts of the msg text??? There should be many smarter ways
 * to handle it */
int talk_to_npc(object *op, object *npc, char *txt)
{
	msglang *msgs;
	int i, j;
	object *cobj;

	if (npc->event_flags & EVENT_FLAG_SAY)
	{
		/* Trigger the SAY event */
		trigger_event(EVENT_SAY, op, npc, NULL, txt, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);

		return 0;
	}

	/* Here we let the objects inside inventories hear and answer, too.
	 * This allows the existence of "intelligent" weapons you can discuss with. */
	for (cobj = npc->inv; cobj != NULL; )
	{
		if (cobj->event_flags & EVENT_FLAG_SAY)
		{
			/* Trigger the SAY event */
			trigger_event(EVENT_SAY, op, cobj, npc, txt, 0, 0, 0, SCRIPT_FIX_ACTIVATOR);

			return 0;
		}

		cobj = cobj->below;
	}

	if (npc->msg == NULL || *npc->msg != '@')
	{
		/*new_draw_info_format(NDI_UNIQUE, 0, op, "%s has nothing to say.", query_name(npc));*/
		return 0;
	}

	if ((msgs = parse_message(npc->msg)) == NULL)
		return 0;

	/* Turn this on again when enhancing parse_message() */
#if 0
	if (debug)
		dump_messages(msgs);
#endif

	for (i = 0; msgs->messages[i]; i++)
	{
		for (j = 0; msgs->keywords[i][j]; j++)
		{
			if (msgs->keywords[i][j][0] == '*' || re_cmp(txt, msgs->keywords[i][j]))
			{
				char buf[MAX_BUF];
				/* a npc talks to another one - show both in white */
				if (op->type != PLAYER)
				{
					/* if a message starts with '/', we assume a emote */
					/* we simply hook here in the emote msg list */
					if (*msgs->messages[i] == '/')
					{
						CommArray_s *csp;
						char *cp = NULL;
						char buf[MAX_BUF];

						strncpy(buf, msgs->messages[i], MAX_BUF - 1);
						buf[MAX_BUF - 1] = '\0';

						cp = strchr(buf, ' ');
						if (cp)
						{
							*(cp++) = '\0';
							cp = cleanup_string(cp);
							if (cp && *cp == '\0')
								cp = NULL;

							if (cp && *cp == '%')
								cp = (char *)op->name;
						}

						csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);
						if (csp)
							csp->func(npc, cp);
					}
					else
					{
						sprintf(buf, "\n%s says: %s", query_name(npc, NULL), msgs->messages[i]);
						new_info_map_except(NDI_UNIQUE, op->map, op->x, op->y, MAP_INFO_NORMAL,op, op, buf);
					}
				}
				/* if a npc is talking to a player, is shown navy and with a seperate "xx says:" line */
				else
				{
					/* if a message starts with '/', we assume a emote */
					/* we simply hook here in the emote msg list */
					if (*msgs->messages[i] == '/')
					{
						CommArray_s *csp;
						char *cp = NULL;
						char buf[MAX_BUF];

						strncpy(buf, msgs->messages[i], MAX_BUF - 1);
						buf[MAX_BUF - 1] = '\0';

						cp = strchr(buf, ' ');
						if (cp)
						{
							*(cp++) = '\0';
							cp = cleanup_string(cp);
							if (cp && *cp == '\0')
								cp = NULL;

							if (cp && *cp == '%')
								cp = (char*)op->name;
						}

						csp = find_command_element(buf, CommunicationCommands, CommunicationCommandSize);
						if (csp)
							csp->func(npc, cp);
					}
					else
					{
						sprintf(buf, "\n%s says:", query_name(npc, NULL));
						new_draw_info(NDI_NAVY | NDI_UNIQUE, 0, op, buf);
						new_draw_info(NDI_NAVY | NDI_UNIQUE, 0, op, msgs->messages[i]);
						sprintf(buf, "%s talks to %s.", query_name(npc, NULL), query_name(op, NULL));
						new_info_map_except(NDI_UNIQUE, op->map, op->x, op->y, MAP_INFO_NORMAL, op, op, buf);
					}
				}
				free_messages(msgs);
				return 1;
			}
		}
	}

	free_messages(msgs);
	return 0;
}

int talk_to_wall(object *npc, char *txt)
{
	msglang *msgs;
	int i, j;

	if (npc->msg == NULL || *npc->msg != '@')
		return 0;

	if ((msgs = parse_message(npc->msg)) == NULL)
		return 0;

	/* Turn this on again when enhancing parse_message() */
#if 0
	if (settings.debug >= llevDebug)
		dump_messages(msgs);
#endif

	for (i = 0; msgs->messages[i]; i++)
	{
		for (j = 0; msgs->keywords[i][j]; j++)
		{
			if (msgs->keywords[i][j][0] == '*' || re_cmp(txt, msgs->keywords[i][j]))
			{
				if (msgs->messages[i] && *msgs->messages[i] != 0)
					new_info_map(NDI_NAVY | NDI_UNIQUE, npc->map, npc->x, npc->y, MAP_INFO_NORMAL, msgs->messages[i]);

				free_messages(msgs);
				use_trigger(npc);
				return 1;
			}
		}
	}

	free_messages(msgs);
	return 0;
}

/* find_mon_throw_ob() - modeled on find_throw_ob
 * This is probably overly simplistic as it is now - We want
 * monsters to throw things like chairs and other pieces of
 * furniture, even if they are not good throwable objects.
 * Probably better to have the monster throw a throwable object
 * first, then throw any non equipped weapon. */
/* no, i don't want monster throwing chairs and something.
 * i want monster picking up throwing weapons and ammo and using it. MT. */
object *find_mon_throw_ob(object *op)
{
	object *tmp = NULL;

	if (op->head)
		tmp = op->head;
	else
		tmp = op;

	/* New throw code: look through the inventory. Grap the first legal is_thrown
	 * marked item and throw it to the enemy. */
	for (tmp = op->inv; tmp; tmp = tmp->below)
	{
		/* Can't throw invisible objects or items that are applied */
		if (IS_SYS_INVISIBLE(tmp) || QUERY_FLAG(tmp, FLAG_APPLIED))
			continue;

		if (QUERY_FLAG(tmp, FLAG_IS_THROWN))
			break;
	}

#ifdef DEBUG_THROW
	LOG(llevDebug, "%s chooses to throw: %s (%d)\n", op->name, !(tmp) ? "(nothing)" : query_name(tmp), tmp ? tmp->count : -1);
#endif

	return tmp;
}

/* Copied from CF, attach this after the attack system rework */
int monster_use_scroll(object *head, object *part, object *pl, int dir, rv_vector *rv)
{
	object *scroll = NULL;
	object *owner;
	rv_vector rv1;

	(void) rv;

	/* If you want monsters to cast spells over friends, this spell should
	 * be removed.  It probably should be in most cases, since monsters still
	 * don't care about residual effects (ie, casting a cone which may have a
	 * clear path to the player, the side aspects of the code will still hit
	 * other monsters) */
	if (!(dir = path_to_player(part, pl, 0)))
		return 0;

	if (QUERY_FLAG(head, FLAG_FRIENDLY) && (owner = get_owner(head)) != NULL)
	{
		get_rangevector(head, owner, &rv1, 0x1);

		/* Might hit owner with spell */
		if (dirdiff(dir, rv1.direction) < 2)
			return 0;
	}

	if (QUERY_FLAG(head, FLAG_CONFUSED))
		dir = absdir(dir + RANDOM() % 3 + RANDOM() % 3 - 2);

#if 0
	for (scroll = head->inv; scroll; scroll=scroll->below)
		if (scroll->type == SCROLL && monster_should_cast_spell(head, scroll->stats.sp))
			break;
#endif
	/* Used up all his scrolls, so nothing do to */
	if (!scroll)
	{
#if 0
		CLEAR_FLAG(head, FLAG_READY_SCROLL);
#endif
		return 0;
	}

	/* Spell should be cast on caster (ie, heal, strength) */
	if (spells[scroll->stats.sp].flags & SPELL_DESC_SELF)
		dir = 0;

#if 0
	apply_scroll(part, scroll, dir);
#endif
	return 1;
}

/* spawn point releated function
 * perhaps its time to make a new module? */

/* drop a monster on the map, by copying a monster object or
 * monster object head. Add treasures. */
static object *spawn_monster(object *gen, object *orig, int range)
{
	int i;
	object *op, *head = NULL, *prev = NULL, *ret = NULL;
	archetype *at = gen->arch;

	i = find_first_free_spot2(at, orig->map, orig->x, orig->y, 0, range);

	if (i == -1)
		return NULL;

	while (at != NULL)
	{
		op = get_object();
		/* copy single/head from spawn inventory */
		if (head == NULL)
		{
			gen->type = MONSTER;
			copy_object(gen, op);
			gen->type = SPAWN_POINT_MOB;
			ret = op;
		}
		/* but the tails for multi arch from the clones */
		else
		{
			copy_object(&at->clone, op);
		}
		op->x = orig->x + freearr_x[i] + at->clone.x;
		op->y = orig->y + freearr_y[i] + at->clone.y;
		op->map = orig->map;

		if (head != NULL)
			op->head = head, prev->more = op;

		if (OBJECT_FREE(op))
			return NULL;

		if (op->randomitems != NULL)
			create_treasure(op->randomitems, op, 0, op->level ? op->level : orig->map->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

		if (head == NULL)
			head = op;

		prev = op;
		at = at->more;
	}

	/* return object ptr to our spawn */
	return ret;
}

/* check the current darkness on this map allows to spawn
 * 0: not allowed, 1: allowed */
static inline int spawn_point_darkness(object *spoint, int darkness)
{
	int map_light;

	if (!spoint->map)
		return 0;

	/* outdoor map */
	if (MAP_OUTDOORS(spoint->map))
		map_light = world_darkness;
	else
	{
		if (MAP_DARKNESS(spoint->map) == -1)
			map_light = MAX_DARKNESS;
		else
			map_light = MAP_DARKNESS(spoint->map);
	}

	if (darkness < 0)
	{
		if (map_light < -darkness)
			return 1;
	}
	else
	{
		if (map_light > darkness)
			return 1;
	}
	return 0;
}

/**
 * Insert a copy of all items the spawn point mob has to the new monster.
 * Takes care about random drop objects.
 *
 * This will recursively call itself if the item to put to the new
 * monster has an inventory.
 *
 * Usually these items are put from the map maker inside the spawn
 * monster inventory.
 * Remember that these are additional items to the treasures list ones.
 * @param op The spawn point object
 * @param monster The object where to put the copy of items to
 * @param tmp The inventory pointer where we are copying the items from */
static void insert_spawn_monster_loot(object *op, object *monster, object *tmp)
{
	object *tmp2, *next, *next2, *item;

	for (; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type == TYPE_RANDOM_DROP)
		{
			/* skip this container - drop the ->inv */
			if (!tmp->weight_limit || !(RANDOM() % (tmp->weight_limit + 1)))
			{
				for (tmp2 = tmp->inv; tmp2; tmp2 = next2)
				{
					next2 = tmp2->below;

					if (tmp2->type == TYPE_RANDOM_DROP)
					{
						LOG(llevDebug,"DEBUG:: Spawn:: RANDOM_DROP (102) not allowed inside RANDOM_DROP.mob:>%s< map:%s (%d,%d)\n", query_name(monster, NULL), op->map ? op->map->path : "BUG: S-Point without map!", op->x, op->y);
					}
					else
					{
						item = get_object();
						copy_object(tmp2, item);
						/* and put it in the mob */
						insert_ob_in_ob(item, monster);

						if (tmp2->inv)
						{
							insert_spawn_monster_loot(op, item, tmp2->inv);
						}
					}
				}
			}
		}
		/* remember this can be sys_objects too! */
		else
		{
			item = get_object();
			copy_object(tmp, item);
			/* and put it in the mob */
			insert_ob_in_ob(item, monster);

			if (tmp->inv)
			{
				insert_spawn_monster_loot(op, item, tmp->inv);
			}
		}
	}
}


/* central spawn point function.
 * Control, generate or remove the generated object. */
void spawn_point(object *op)
{
	int rmt;
	object *tmp, *mob, *next;

	if (op->enemy)
	{
		/* all ok, our spawn have fun */
		if (OBJECT_VALID(op->enemy, op->enemy_count))
		{
			/* check darkness if needed */
			if (op->last_eat)
			{
				/* 1 = darkness is ok */
				if (spawn_point_darkness(op, op->last_eat))
					return;

				/* darkness has changed - now remove the spawned monster */
				remove_ob(op->enemy);
				check_walk_off(op->enemy, NULL, MOVE_APPLY_VANISHED);
			}
			else
				return;
		}
		/* spawn point has nothing spawned */
		op->enemy = NULL;
	}

	/* a set sp value will override the spawn chance.
	* with that "trick" we force for map loading the
	* default spawns of the map because sp is 0 as default. */
	/*LOG(-1,"SPAWN...(%d,%d)",op->x, op->y);*/
	if (op->stats.sp == -1)
	{
		int gg;
		/* now lets decide we will have a spawn event */
		/* never */
		if (op->last_grace <= -1)
		{
			/*LOG(-1," closed (-1)\n");*/
			return;
		}
		/* if >0 and random%x is NOT null ... */
		if (op->last_grace && (gg = (RANDOM() % (op->last_grace + 1))))
		{
			/*LOG(-1," chance: %d (%d)\n",gg,op->last_grace);*/
			return;
		}

		op->stats.sp = (RANDOM() % SPAWN_RANDOM_RANGE);
	}
	/*LOG(-1," hit!: %d\n",op->stats.sp);*/

	/* spawn point without inventory! */
	if (!op->inv)
	{
		LOG(llevBug, "BUG: Spawn point without inventory!! --> map %s (%d,%d)\n", op->map ? (op->map->path ? op->map->path : ">no path<") : ">no map<", op->x, op->y);
		/* kill this spawn point - its useless and need to fixed from the map maker/generator */
		remove_ob(op);
		check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
		return;
	}
	/* now we move through the spawn point inventory and
	 * get the mob with a number under this value AND nearest. */
	for (rmt = 0, mob = NULL, tmp = op->inv; tmp; tmp = next)
	{
		next = tmp->below;

		if (tmp->type != SPAWN_POINT_MOB)
			LOG(llevBug, "BUG: spawn point in map %s (%d,%d) with wrong type object (%d) in inv: %s\n", op->map ? op->map->path : "<no map>", op->x, op->y, tmp->type, query_name(tmp, NULL));
		else if ((int)tmp->enemy_count <= op->stats.sp && (int)tmp->enemy_count >= rmt)
		{
			/* we have a possible hit - control special settings now */
			/* darkness */
			if (tmp->last_eat)
			{
				/* 1: darkness on map of spawn point is ok */
				if (!spawn_point_darkness(op, tmp->last_eat))
					continue;
			}

			rmt = (int)tmp->enemy_count;
			mob = tmp;
		}
		/*LOG(llevInfo,"inv -> %s (%d :: %d - %f)\n", tmp->name, op->stats.sp, tmp->enemy_count, tmp->speed_left);*/
	}

	/* we try only ONE time a respawn of a pre setting - so careful! */
	rmt = op->stats.sp;
	op->stats.sp = -1;
	/* well, this time we spawn nothing */
	if (!mob)
		return;

	/* quick save the def mob inventory */
	tmp = mob->inv;
	/* that happens when we have no free spot....*/
	if (!(mob = spawn_monster(mob, op, op->last_heal)))
		return;

	/* setup special monster -> spawn point values */
	op->last_eat = 0;

	/* darkness controled spawns */
	if (mob->last_eat)
	{
		op->last_eat = mob->last_eat;
		mob->last_eat = 0;
	}

	insert_spawn_monster_loot(op, mob, tmp);

	/* this is the last rand() for what we have spawned! */
	op->last_sp = rmt;

	/* chain the mob to our spawn point */
	op->enemy = mob;
	op->enemy_count = mob->count;

	/* perhaps we have later more unique mob setting - then we can store it here too. */

	/* create spawn info */
	tmp = arch_to_object(op->other_arch);
	/* chain spawn point to our mob */
	tmp->owner = op;
	/* and put it in the mob */
	insert_ob_in_ob(tmp, mob);

	/* FINISH: now mark our mob as a spawn */
	SET_MULTI_FLAG(mob, FLAG_SPAWN_MOB);
	/* fix all the values and add in possible abilities or forces ... */
	fix_monster(mob);

	/* *now* all is done - *now* put it on map */
	if (!insert_ob_in_map(mob, mob->map, op, 0))
		return;

	if (QUERY_FLAG(mob, FLAG_FRIENDLY))
		add_friendly_object(mob);
}

/**
 * Check if object op is friend of obj.
 * @param op The first object
 * @param obj The second object to check against the first one
 * @return 1 if both objects are friends, 0 otherwise */
int is_friend_of(object *op, object *obj)
{
	/* TODO: Add a few other odd types here, such as god and golem */
	if (!obj->type == PLAYER || !obj->type == MONSTER || !op->type == PLAYER || !op->type == MONSTER || op == obj)
	{
		return 0;
	}

	/* If on PVP area, they won't be friendly */
	if (pvp_area(op, obj))
	{
		return 0;
	}

	/* TODO: This needs to be sorted out better */
	if (QUERY_FLAG(op, FLAG_FRIENDLY) || op->type == PLAYER)
	{
		if (!QUERY_FLAG(obj, FLAG_MONSTER) || QUERY_FLAG(obj, FLAG_FRIENDLY) || obj->type == PLAYER)
		{
			return 1;
		}
	}
	else
	{
		if (!QUERY_FLAG(obj, FLAG_FRIENDLY) && obj->type != PLAYER)
		{
			return 1;
		}
	}

	return 0;
}

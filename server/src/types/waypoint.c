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
 * Handles the code for @ref TYPE_WAYPOINT_OBJECT "waypoint objects". */

#include <global.h>
#include <sproto.h>

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


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
 * Handles the code for @ref WAYPOINT_OBJECT "waypoint objects". */

#include <global.h>

/**
 * Find a monster's currently active waypoint, if any.
 * @param op The monster.
 * @return Active waypoint of this monster, NULL if none found. */
object *get_active_waypoint(object *op)
{
	object *wp;

	for (wp = op->inv; wp; wp = wp->below)
	{
		if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_CURSED))
		{
			return wp;
		}
	}

	return NULL;
}

/**
 * Find a monster's current aggro waypoint, if any.
 * @param op The monster.
 * @return Aggro waypoint of this monster, NULL if none found. */
object *get_aggro_waypoint(object *op)
{
	object *wp;

	for (wp = op->inv; wp; wp = wp->below)
	{
		if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_DAMNED))
		{
			return wp;
		}
	}

	return NULL;
}

/**
 * Find a monster's current return-home waypoint, if any.
 * @param op The monster.
 * @return Return-home waypoint of this monster, NULL if none
 * found. */
object *get_return_waypoint(object *op)
{
	object *wp;

	for (wp = op->inv; wp; wp = wp->below)
	{
		if (wp->type == WAYPOINT_OBJECT && QUERY_FLAG(wp, FLAG_REFLECTING))
		{
			return wp;
		}
	}

	return NULL;
}

/**
 * Find a monster's waypoint by name (used for getting the next waypoint).
 * @param op The monster.
 * @param name The waypoint name to find.
 * @return The waypoint object if found, NULL otherwise. */
static object *find_waypoint(object *op, shstr *name)
{
	object *wp;

	if (name == NULL)
	{
		return NULL;
	}

	for (wp = op->inv; wp; wp = wp->below)
	{
		if (wp->type == WAYPOINT_OBJECT && wp->name == name)
		{
			return wp;
		}
	}

	return NULL;
}

/**
 * Find the destination map if specified in waypoint, otherwise use current
 * map.
 * @param op Monster.
 * @param waypoint Waypoint.
 * @return Destination map. */
static mapstruct *waypoint_load_dest(object *op, object *waypoint)
{
	mapstruct *destmap;

	/* If path is not normalized, normalize it */
	if (*waypoint->slaying != '/')
	{
		char temp_path[HUGE_BUF];

		if (*waypoint->slaying == '\0')
		{
			return NULL;
		}

		normalize_path(op->map->path, waypoint->slaying, temp_path);
		FREE_AND_COPY_HASH(waypoint->slaying, temp_path);
	}

	if (waypoint->slaying == op->map->path)
	{
		destmap = op->map;
	}
	else
	{
		destmap = ready_map_name(waypoint->slaying, MAP_NAME_SHARED);
	}

	return destmap;
}

/**
 * Perform a path computation for the waypoint object.
 *
 * This function is called whenever our path request is dequeued.
 * @param waypoint The waypoint object. */
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
			LOG(llevBug, "BUG: waypoint_compute_path(): Dynamic waypoint without valid target: '%s'\n", waypoint->name);
			return;
		}
	}

	if (waypoint->slaying)
	{
		destmap = waypoint_load_dest(op, waypoint);
	}

	if (!destmap)
	{
		LOG(llevBug, "BUG: waypoint_compute_path(): Invalid destination map '%s'\n", waypoint->slaying);
		return;
	}

	path = compress_path(find_path(op, op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp));

	if (!path)
	{
		LOG(llevBug, "BUG: waypoint_compute_path(): No path to destination ('%s' -> '%s')\n", op->name, waypoint->name);
		return;
	}

	/* Skip the first path element (always the starting position) */
	path = path->next;

	if (!path)
	{
		return;
	}

#ifdef DEBUG_PATHFINDING
	{
		path_node *tmp;

		LOG(llevDebug, "DEBUG: waypoint_compute_path(): '%s' new path -> '%s': ", op->name, waypoint->name);

		for (tmp = path; tmp; tmp = tmp->next)
		{
			LOG(llevDebug, "(%d, %d) ", tmp->x, tmp->y);
		}

		LOG(llevDebug, "\n");
	}
#endif

	/* Textually encoded path */
	FREE_AND_CLEAR_HASH(waypoint->msg);
	/* Map file for last local path step */
	FREE_AND_CLEAR_HASH(waypoint->race);

	waypoint->msg = encode_path(path);
	/* Path offset */
	waypoint->stats.food = 0;
	/* Msg boundary */
	waypoint->attacked_by_distance = strlen(waypoint->msg);

	/* Number of fails */
	waypoint->stats.Int = 0;
	/* Number of fails */
	waypoint->stats.Str = 0;
	/* Best distance */
	waypoint->stats.dam = 30000;
	CLEAR_FLAG(waypoint, FLAG_CONFUSED);
}

/**
 * Move towards waypoint target.
 * @param op Object to move.
 * @param waypoint The waypoint object. */
void waypoint_move(object *op, object *waypoint)
{
	mapstruct *destmap = op->map;
	rv_vector local_rv, global_rv, *dest_rv;
	int dir;
	sint16 new_offset = 0, success = 0;

	if (!waypoint || !op || !op->map)
	{
		return;
	}

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
			/* Owner has either switched or lost enemy. This should work for both cases.
			 * switched -> similar to if target moved
			 * lost -> we shouldn't be called again without new data */
			waypoint->enemy = op->enemy;
			waypoint->enemy_count = op->enemy_count;
			return;
		}
	}
	else if (waypoint->slaying)
	{
		destmap = waypoint_load_dest(op, waypoint);
	}

	if (!destmap)
	{
		LOG(llevBug, "BUG: waypoint_move(): Invalid destination map '%s' for '%s' -> '%s'\n", waypoint->slaying, op->name, waypoint->name);
		return;
	}

	if (!get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, waypoint->stats.hp, waypoint->stats.sp, &global_rv, RV_RECURSIVE_SEARCH | RV_DIAGONAL_DISTANCE))
	{
		/* Disable this waypoint */
		CLEAR_FLAG(waypoint, FLAG_CURSED);
		return;
	}

	dest_rv = &global_rv;

	/* Reached the final destination? */
	if ((int) global_rv.distance <= waypoint->stats.grace)
	{
		object *nextwp = NULL;

		/* Just arrived? */
		if (waypoint->stats.ac == 0)
		{
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "move_waypoint(): '%s' reached destination '%s'\n", op->name, waypoint->name);
#endif

			/* Trigger the TRIGGER event */
			if (trigger_event(EVENT_TRIGGER, op, waypoint, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
			{
				return;
			}
		}

		/* Waiting at this waypoint? */
		if (waypoint->stats.ac < waypoint->stats.wc)
		{
			waypoint->stats.ac++;
			return;
		}

		/* Clear timer */
		waypoint->stats.ac = 0;
		/* Set as inactive */
		CLEAR_FLAG(waypoint, FLAG_CURSED);
		/* Remove precomputed path */
		FREE_AND_CLEAR_HASH(waypoint->msg);
		/* Remove precomputed path data */
		FREE_AND_CLEAR_HASH(waypoint->race);

		/* Is it a return-home waypoint? */
		if (QUERY_FLAG(waypoint, FLAG_REFLECTING))
		{
			op->move_type = waypoint->move_type;
		}

		/* Start over with the new waypoint, if any */
		if (!QUERY_FLAG(waypoint, FLAG_DAMNED))
		{
			nextwp = find_waypoint(op, waypoint->title);

			if (nextwp)
			{
#ifdef DEBUG_PATHFINDING
				LOG(llevDebug, "waypoint_move(): '%s' next waypoint: '%s'\n", op->name, waypoint->title);
#endif
				SET_FLAG(nextwp, FLAG_CURSED);
				waypoint_move(op, get_active_waypoint(op));
			}
#ifdef DEBUG_PATHFINDING
			else
			{
				LOG(llevDebug, "BUG: waypoint_move(): '%s' is missing next waypoint.\n", op->name);
			}
#endif
		}

		waypoint->enemy = NULL;
		return;
	}

	if (HAS_EVENT(waypoint, EVENT_CLOSE) && trigger_event(EVENT_CLOSE, op, waypoint, NULL, NULL, 0, 0, 0, SCRIPT_FIX_NOTHING))
	{
		return;
	}

	/* Handle precomputed paths */

	/* If we finished our current path, clear it so that we can get a new one. */
	if (waypoint->msg && (waypoint->msg[waypoint->stats.food] == '\0' || global_rv.distance <= 0))
	{
		FREE_AND_CLEAR_HASH(waypoint->msg);
	}

	/* Get new path if target has moved much since the path was created */
	if (QUERY_FLAG(waypoint, FLAG_DAMNED) && waypoint->msg && (waypoint->stats.hp != waypoint->x || waypoint->stats.sp != waypoint->y))
	{
		rv_vector rv;
		mapstruct *path_destmap = ready_map_name(waypoint->slaying, MAP_NAME_SHARED);

		get_rangevector_from_mapcoords(destmap, waypoint->stats.hp, waypoint->stats.sp, path_destmap, waypoint->x, waypoint->y, &rv, RV_DIAGONAL_DISTANCE);

		if (rv.distance > 1 && rv.distance > global_rv.distance)
		{
#ifdef DEBUG_PATHFINDING
			LOG(llevDebug, "waypoint_move(): Path distance = %d for '%s' -> '%s'. Discarding old path.\n", rv.distance, op->name, op->enemy->name);
#endif
			FREE_AND_CLEAR_HASH(waypoint->msg);
		}
	}

	/* Are we far enough from the target to require a path? */
	if (global_rv.distance > 1)
	{
		if (!waypoint->msg)
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
				get_rangevector_from_mapcoords(op->map, op->x, op->y, destmap, destx, desty, &local_rv, RV_RECURSIVE_SEARCH | RV_DIAGONAL_DISTANCE);
				dest_rv = &local_rv;
			}
			else
			{
				/* We seem to have an invalid path string or offset. */
				FREE_AND_CLEAR_HASH(waypoint->msg);
				request_new_path(waypoint);
			}
		}
	}

	/* Did we get closer to our goal last time? */
	if ((int) dest_rv->distance < waypoint->stats.dam)
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

	if (global_rv.distance > 1 && !waypoint->msg && QUERY_FLAG(waypoint, FLAG_CONFUSED))
	{
#ifdef DEBUG_PATHFINDING
		LOG(llevDebug, "waypoint_move(): No path found. '%s' standing still.\n", op->name);
#endif
		return;
	}

	/* Perform the actual move */
	dir = dest_rv->direction;

	if (QUERY_FLAG(op, FLAG_CONFUSED))
	{
		dir = get_randomized_dir(dir);
	}

	if (dir && !QUERY_FLAG(op, FLAG_STAND_STILL))
	{
		/* Can the monster move directly toward waypoint? */
		if (move_object(op, dir))
		{
			success = 1;
		}
		else
		{
			int diff;

			/* Try move around corners otherwise */
			for (diff = 1; diff <= 2; diff++)
			{
				/* Try left or right first? */
				int m = 1 - (RANDOM() & 2);

				if (move_object(op, absdir(dir + diff * m)) || move_object(op, absdir(dir - diff * m)))
				{
					success = 1;
					break;
				}
			}
		}

		/* If we had a local destination and we got close enough to it, accept it. */
		if (dest_rv == &local_rv && dest_rv->distance == 1)
		{
			waypoint->stats.food = new_offset;
			/* Number of fails */
			waypoint->stats.Str = 0;
			/* Best distance */
			waypoint->stats.dam = 30000;
		}
	}
}

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
 * Controls map swap functions. */

#include <global.h>
#include <sproto.h>

static mapstruct *map_least_timeout(const char *except_level);

#ifdef RECYCLE_TMP_MAPS
/**
 * Write maps log. */
static void write_map_log()
{
	FILE *fp;
	mapstruct *map;
	char buf[MAX_BUF];
	long current_time = time(NULL);

	snprintf(buf, sizeof(buf), "%s/temp.maps", settings.localdir);

	if (!(fp = fopen(buf, "w")))
	{
		LOG(llevBug, "BUG: Could not open %s for writing\n", buf);
		return;
	}

	for (map = first_map; map != NULL; map = map->next)
	{
		/* If tmpname is null, it is probably a unique player map,
		 * so don't save information on it. */
		if (map->in_memory != MAP_IN_MEMORY && (map->tmpname != NULL) && (strncmp(map->path, "/random", 7)))
		{
			/* the 0 written out is a leftover from the lock number for
			 * unique items and second one is from encounter maps.
			 * Keep using it so that old temp files continue
			 * to work. */
			fprintf(fp, "%s:%s:%ld:0:0:%d:0:%d\n", map->path, map->tmpname, (map->reset_time == -1 ? -1: map->reset_time - current_time), map->difficulty, map->darkness);
		}
	}

	fclose(fp);
}
#endif

/**
 * Read map log. */
void read_map_log()
{
	FILE *fp;
	mapstruct *map;
	char buf[MAX_BUF],*cp, *cp1;
	int do_los, darkness, lock;

	snprintf(buf, sizeof(buf), "%s/temp.maps", settings.localdir);

	if (!(fp = fopen(buf, "r")))
	{
		LOG(llevDebug, "Could not open %s for reading\n", buf);
		return;
	}

	while (fgets(buf, MAX_BUF, fp) != NULL)
	{
		map = get_linked_map();

		/* scanf doesn't work all that great on strings, so we break
		 * out that manually.  strdup is used for tmpname, since other
		 * routines will try to free that pointer. */
		cp = strchr(buf, ':');
		*cp++ = '\0';
		FREE_AND_COPY_HASH(map->path, buf);
		cp1 = strchr(cp, ':');
		*cp1++ = '\0';
		map->tmpname = strdup_local(cp);

		/* Lock is left over from the lock items - we just toss it now.
		 * We use it twice - second one is from encounter, but as we
		 * don't care about the value, this works fine */
		sscanf(cp1, "%ud:%d:%d:%d:%d:%d\n", &map->reset_time, &lock, &lock, &map->difficulty, &do_los, &darkness);

		map->in_memory = MAP_SWAPPED;
		map->darkness = darkness;

		if (darkness == -1)
		{
			darkness = MAX_DARKNESS;
		}

		map->light_value = global_darkness_table[MAX_DARKNESS];
	}

	fclose(fp);
}

/**
 * Swap out a map.
 * @param map The map structure to swap.
 * @param force_flag Force flag. */
void swap_map(mapstruct *map, int force_flag)
{
	int i;

	/* lets check some legal things... */
	if (map->in_memory != MAP_IN_MEMORY)
	{
		LOG(llevBug, "BUG: Tried to swap out map which was not in memory (%s).\n", map->path);
		return;
	}

	/* test for players! */
	if (!force_flag)
	{
		/* player nor perm_loaded marked */
		if (map->player_first || map->perm_load)
			return;

		for (i = 0; i < TILED_MAPS; i++)
		{
			/* if there is a map, is load AND in memory and players on OR perm_load flag set, then no swap */
			if (map->tile_map[i] && map->tile_map[i]->in_memory == MAP_IN_MEMORY && (map->tile_map[i]->player_first || map->tile_map[i]->perm_load))
			{
				return;
			}
		}
	}

	/* when we are here, map is save to swap! */

	/* Give them a chance to follow */
	remove_all_pets();

	/* Update the reset time.  Only do this is STAND_STILL is not set */
	if (!MAP_FIXED_RESETTIME(map))
	{
		set_map_reset_time(map);
	}

	/* If it is immediate reset time, don't bother saving it - just get
	 * rid of it right away. */
	if (map->reset_time <= (uint32) seconds())
	{
		mapstruct *oldmap = map;

		LOG(llevDebug, "Resetting1 map %s.\n", map->path);

		if (MAP_PLUGINS(map))
		{
			/* Trigger the global MAPRESET event */
			trigger_global_event(EVENT_MAPRESET, (void *) map->path, NULL);
		}

		map = map->next;
		delete_map(oldmap);
		return;
	}

	if (new_save_map(map, 0) == -1)
	{
		LOG(llevBug, "BUG: Failed to swap map %s.\n", map->path);
		/* need to reset the in_memory flag so that delete map will also
		 * free the objects with it. */
		map->in_memory = MAP_IN_MEMORY;
		delete_map(map);
	}
	else
	{
		free_map(map, 1);
	}

#ifdef RECYCLE_TMP_MAPS
	write_map_log();
#endif
}

/**
 * Check active maps and swap them out. */
void check_active_maps()
{
	mapstruct *map, *next;

	for (map = first_map; map != NULL; map = next)
	{
		next = map->next;

		if (map->in_memory != MAP_IN_MEMORY)
		{
			continue;
		}

		if (!map->timeout)
		{
			continue;
		}

		if (--(map->timeout) > 0)
		{
			continue;
		}

		/* This is called when MAX_OBJECTS_LWM is *NOT* defined.
		 * If LWM is set, we only swap maps out when we run out of objects */
#ifndef MAX_OBJECTS_LWM
		swap_map(map, 0);
#endif
	}
}

/**
 * Returns the map with the lowest timeout variable (not 0).
 * @param except_level Path of map to ignore for reset. Musn't be NULL.
 * @return Map, or NULL if no map is ready for reset. */
static mapstruct *map_least_timeout(const char *except_level)
{
	mapstruct *map, *chosen = NULL;
	int timeout = MAP_MAXTIMEOUT + 1;

	for (map = first_map; map != NULL; map = map->next)
	{
		if (map->in_memory == MAP_IN_MEMORY && strcmp (map->path, except_level) && map->timeout && map->timeout < timeout)
		{
			chosen = map, timeout = map->timeout;
		}
	}

	return chosen;
}

/**
 * Tries to swap out maps which are still in memory, because of
 * MAP_TIMEOUT until used objects is below MAX_OBJECTS or there are no
 * more maps to swap.
 * @param except_level Path of map to ignore for reset. Musn't be NULL. */
void swap_below_max(const char *except_level)
{
	mapstruct *map;

	if ((pool_object->nrof_allocated - pool_object->nrof_free) < (uint32) MAX_OBJECTS)
	{
		return;
	}

	for (; ;)
	{
#ifdef MAX_OBJECTS_LWM
		if ((pool_object->nrof_allocated - pool_object->nrof_free) < (uint32) MAX_OBJECTS_LWM)
		{
			return;
		}
#else
		if ((pool_object->nrof_allocated - pool_object->nrof_free) < (uint32) MAX_OBJECTS)
		{
			return;
		}
#endif

		if ((map = map_least_timeout(except_level)) == NULL)
		{
			return;
		}

		LOG(llevDebug, "Trying to swap out %s before its time.\n", map->path);
		map->timeout = 0;
		swap_map(map, 0);
	}
}

/**
 * Removes temporary files of maps which are going to be reset next time
 * they are visited.
 * This is very useful if the tmp-disk is very full. */
void flush_old_maps()
{
	mapstruct *m = first_map, *oldmap;
	long sec = seconds();

	while (m)
	{
		/* There can be cases (ie death) where a player leaves a map and
		 * the timeout is not set so it isn't swapped out. */
		if ((m->in_memory == MAP_IN_MEMORY) && (m->timeout == 0) && !m->player_first)
		{
			set_map_timeout(m);
		}

		/* per player unique maps are never really reset.  However, we do want
		 * to perdiocially remove the entries in the list of active maps - this
		 * generates a cleaner listing if a player issues the map commands, and
		 * keeping all those swapped out per player unique maps also has some
		 * memory and cpu consumption.
		 * We do the cleanup here because there are lots of places that call
		 * swap map, and doing it within swap map may cause problems as
		 * the functions calling it may not expect the map list to change
		 * underneath them. */
		if (MAP_UNIQUE(m) && m->in_memory == MAP_SWAPPED)
		{
			LOG(llevDebug, "Resetting2 map %s.\n", m->path);
			oldmap = m;
			m = m->next;
			delete_map(oldmap);
		}
		/* No need to flush them if there are no resets */
#ifdef MAP_RESET
		else if (m->in_memory != MAP_SWAPPED || m->tmpname == NULL || (uint32) sec < m->reset_time)
		{
			m = m->next;
		}
		else
		{
			LOG(llevDebug, "Resetting3 map %s.\n", m->path);

			if (MAP_PLUGINS(m))
			{
				/* Trigger the global MAPRESET event */
				trigger_global_event(EVENT_MAPRESET, (void *) m->path, NULL);
			}

			clean_tmp_map(m);
			oldmap = m;
			m = m->next;
			delete_map(oldmap);
		}
#endif
	}
}

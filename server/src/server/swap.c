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

#if RECYCLE_TMP_MAPS
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

	for (map = first_map; map; map = map->next)
	{
		/* If tmpname is null, it is probably a unique player map,
		 * so don't save information on it. */
		if (map->in_memory != MAP_IN_MEMORY && map->tmpname && strncmp(map->path, "/random", 7))
		{
			/* the 0 written out is a leftover from the lock number for
			 * unique items and second one is from encounter maps.
			 * Keep using it so that old temp files continue
			 * to work. */
			fprintf(fp, "%s:%s:%ld:%d:%d\n", map->path, map->tmpname, (map->reset_time == -1 ? -1: map->reset_time - current_time), map->difficulty, map->darkness);
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
	char buf[MAX_BUF];
	int darkness;

	snprintf(buf, sizeof(buf), "%s/temp.maps", settings.localdir);

	if (!(fp = fopen(buf, "r")))
	{
		LOG(llevDebug, "Could not open %s for reading\n", buf);
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		char *tmp[3];

		map = get_linked_map();

		if (split_string(buf, tmp, sizeof(tmp) / sizeof(*tmp), ':') != 3)
		{
			LOG(llevDebug, "DEBUG: %s/temp.maps: ignoring invalid line: %s\n", settings.localdir, buf);
			continue;
		}

		FREE_AND_COPY_HASH(map->path, tmp[0]);
		map->tmpname = strdup_local(tmp[1]);

		sscanf(tmp[2], "%ud:%d:%d\n", &map->reset_time, &map->difficulty, &darkness);

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
 * Swaps a map to disk.
 * @param map Map to swap.
 * @param force_flag Force flag. If set, will not check for players. */
void swap_map(mapstruct *map, int force_flag)
{
	int i;

	if (map->in_memory != MAP_IN_MEMORY)
	{
		LOG(llevBug, "BUG: Tried to swap out map which was not in memory (%s).\n", map->path);
		return;
	}

	/* Test for players. */
	if (!force_flag)
	{
		if (map->player_first)
		{
			return;
		}

		for (i = 0; i < TILED_MAPS; i++)
		{
			/* If there is a map, is loaded and in memory, has players, then no swap */
			if (map->tile_map[i] && map->tile_map[i]->in_memory == MAP_IN_MEMORY && map->tile_map[i]->player_first)
			{
				return;
			}
		}
	}

	/* Update the reset time. */
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

		if (map->events)
		{
			/* Trigger the map reset event */
			trigger_map_event(MEVENT_RESET, map, NULL, NULL, NULL, (void *) map->path, 0);
		}

		map = map->next;
		delete_map(oldmap);
		return;
	}

	if (new_save_map(map, 0) == -1)
	{
		LOG(llevBug, "BUG: Failed to swap map %s.\n", map->path);
		/* Need to reset the in_memory flag so that delete map will also
		 * free the objects with it. */
		map->in_memory = MAP_IN_MEMORY;
		delete_map(map);
	}
	else
	{
		free_map(map, 1);
	}

#if RECYCLE_TMP_MAPS
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
			if (!map->player_first)
			{
				set_map_timeout(map);
			}

			continue;
		}

		if (--(map->timeout) > 0)
		{
			continue;
		}

		swap_map(map, 0);
	}
}

/**
 * Removes temporary files of maps which are going to be reset next time
 * they are visited.
 *
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

		/* Per player unique maps are never really reset. */
		if (MAP_UNIQUE(m) && m->in_memory == MAP_SWAPPED)
		{
			LOG(llevDebug, "Resetting2 map %s.\n", m->path);
			oldmap = m;
			m = m->next;
			delete_map(oldmap);
		}
		/* No need to flush them if there are no resets */
		else if (m->in_memory != MAP_SWAPPED || m->tmpname == NULL || (uint32) sec < m->reset_time)
		{
			m = m->next;
		}
		else
		{
			LOG(llevDebug, "Resetting3 map %s.\n", m->path);

			if (m->events)
			{
				/* Trigger the map reset event */
				trigger_map_event(MEVENT_RESET, m, NULL, NULL, NULL, (void *) m->path, 0);
			}

			clean_tmp_map(m);
			oldmap = m;
			m = m->next;
			delete_map(oldmap);
		}
	}
}

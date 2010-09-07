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
 * Handles code for @ref MAGIC_MIRROR "magic mirrors".
 *
 * Magic mirrors are objects that mirror contents of another tile,
 * effectively creating a map stacking effect. It is also possible to
 * make the magic mirrors zoom out/in mirrored objects to create a depth
 * effect. */

#include <global.h>

/**
 * Initializes a magic mirror object after it has been placed on map.
 * @param mirror The magic mirror to initialize. */
void magic_mirror_init(object *mirror)
{
	sint16 mirror_x, mirror_y;

	if (!mirror->map)
	{
		LOG(llevBug, "BUG: magic_mirror_init(): Magic mirror not on map.\n");
		return;
	}

	mirror_x = (mirror->stats.hp == -1 ? mirror->x : mirror->stats.hp);
	mirror_y = (mirror->stats.sp == -1 ? mirror->y : mirror->stats.sp);

	/* X/Y adjust. */
	if (mirror->stats.maxhp)
	{
		mirror_x += mirror->stats.maxhp;
	}

	if (mirror->stats.maxsp)
	{
		mirror_y += mirror->stats.maxsp;
	}

	/* No point in doing anything special if we're mirroring the same map. */
	if (!mirror->slaying && mirror_x == mirror->x && mirror_y == mirror->y)
	{
		return;
	}

	/* No map path specified, use mirror's map path. */
	if (!mirror->slaying)
	{
		FREE_AND_ADD_REF_HASH(mirror->slaying, mirror->map->path);
	}
	/* Map path was specified, so try to normalize it. */
	else
	{
		char tmp_path[HUGE_BUF];

		FREE_AND_COPY_HASH(mirror->slaying, normalize_path(mirror->map->path, mirror->slaying, tmp_path));
	}

	/* Initialize custom_attrset. */
	mirror->custom_attrset = malloc(sizeof(magic_mirror_struct));
	/* Save x/y and clear map. */
	MMIRROR(mirror)->x = mirror_x;
	MMIRROR(mirror)->y = mirror_y;
	MMIRROR(mirror)->map = NULL;
}

/**
 * Deinitialize a magic mirror object.
 *
 * Mostly used to free object::custom_attrset of the mirror.
 * @param mirror Magic mirror to deinitialize. */
void magic_mirror_deinit(object *mirror)
{
	free(mirror->custom_attrset);
}

/**
 * Get map to which a magic mirror is pointing to. Almost always this
 * should be used instead of accessing magic_mirror_struct::map directly,
 * as it will make sure the map is loaded and will reset the swap timeout.
 * @param mirror Magic mirror to get map of.
 * @return The map. Can be NULL in case of loading error. */
mapstruct *magic_mirror_get_map(object *mirror)
{
	magic_mirror_struct *data = MMIRROR(mirror);

	/* Map good to go? */
	if (data->map && data->map->in_memory == MAP_IN_MEMORY)
	{
		/* Reset timeout so the mirrored map doesn't get swapped out. */
		MAP_TIMEOUT(data->map) = MAP_DEFAULTTIMEOUT;
		return data->map;
	}

	/* Try to load the map. */
	data->map = ready_map_name(mirror->slaying, MAP_NAME_SHARED);

	if (!data->map)
	{
		LOG(llevBug, "BUG: magic_mirror_get_map(): Could not load map '%s'.\n", mirror->slaying);
		return NULL;
	}

	return data->map;
}

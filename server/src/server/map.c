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
 * Map related functions. */

#include <global.h>
#include <loader.h>

#ifndef WIN32
#include <unistd.h>
#endif

int	global_darkness_table[MAX_DARKNESS + 1] =
{
	0, 20, 40, 80, 160, 320, 640, 1280
};

/** To get the reverse direction for all 8 tiled map index */
int map_tiled_reverse[TILED_MAPS] =
{
	2, 3, 0, 1, 6, 7, 4, 5
};

#define DEBUG_OLDFLAGS 1

static void load_objects(mapstruct *m, FILE *fp, int mapflags);
static void save_objects(mapstruct *m, FILE *fp, FILE *fp2);
static void allocate_map(mapstruct *m);
static void free_all_objects(mapstruct *m);

/**
 * Try loading the connected map tile with the given number.
 * @param orig_map Base map.
 * @param tile_num Tile number to connect to.
 * @return NULL if loading or tiling fails, loaded neighbor map otherwise. */
static inline mapstruct *load_and_link_tiled_map(mapstruct *orig_map, int tile_num)
{
	mapstruct *map = ready_map_name(orig_map->tile_path[tile_num], MAP_NAME_SHARED | (MAP_UNIQUE(orig_map) ? 1 : 0));

	if (!map || map != orig_map->tile_map[tile_num])
	{
		LOG(llevBug, "BUG: Failed to connect map %s with tile #%d (%s).\n", STRING_SAFE(orig_map->path), tile_num, STRING_SAFE(orig_map->tile_path[tile_num]));
		FREE_AND_CLEAR_HASH(orig_map->tile_path[tile_num]);
		return NULL;
	}

	return map;
}

/**
 * Recursive part of the relative_tile_position() function.
 * @param map1
 * @param map2
 * @param x
 * @param y
 * @param id
 * @return
 * @todo A bidirectional breadth-first search would be more efficient. */
static int relative_tile_position_rec(mapstruct *map1, mapstruct *map2, int *x, int *y, uint32 id)
{
	int i;

	if (map1 == map2)
	{
		return 1;
	}

	map1->traversed = id;

	/* Depth-first search for the destination map */
	for (i = 0; i < TILED_MAPS; i++)
	{
		if (map1->tile_path[i])
		{
			if (!map1->tile_map[i] || map1->tile_map[i]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(map1, i))
				{
					continue;
				}
			}

			if (map1->tile_map[i]->traversed != id && ((map1->tile_map[i] == map2) || relative_tile_position_rec(map1->tile_map[i], map2, x, y, id)))
			{
				switch (i)
				{
					/* North */
					case 0:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						return 1;

					/* East */
					case 1:
						*x += MAP_WIDTH(map1);
						return 1;

					/* South */
					case 2:
						*y += MAP_HEIGHT(map1);
						return 1;

					/* West */
					case 3:
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;

					/* Northest */
					case 4:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x += MAP_WIDTH(map1);
						return 1;

					/* Southest */
					case 5:
						*y += MAP_HEIGHT(map1);
						*x += MAP_WIDTH(map1);
						return 1;

					/* Southwest */
					case 6:
						*y += MAP_HEIGHT(map1);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;

					/* Northwest */
					case 7:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;
				}
			}
		}
	}

	return 0;
}

/**
 * Find the distance between two map tiles on a tiled map.
 *
 * The distance from the topleft (0, 0) corner of map1 to the topleft
 * corner of map2 will be added to x and y.
 *
 * This function does not work well with asymmetrically tiled maps.
 *
 * It will also (naturally) perform bad on very large tilesets such as
 * the world map as it may need to load all tiles into memory before
 * finding a path between two tiles.
 *
 * We probably want to handle the world map as a special case,
 * considering that all tiles are of equal size, and that we might be
 * able to parse their coordinates from their names...
 * @param map1
 * @param map2
 * @param x
 * @param y
 * @return 1 if the two tiles are part of the same map, 0 otherwise. */
static int relative_tile_position(mapstruct *map1, mapstruct *map2, int *x, int *y)
{
	int i;
	static uint32 traversal_id = 0;

	/* Save some time in the simplest cases ( very similar to on_same_map() )*/
	if (map1 == NULL || map2 == NULL)
	{
		return 0;
	}

	if (map1 == map2)
	{
		return 1;
	}

	for (i = 0; i < TILED_MAPS; i++)
	{
		if (map1->tile_path[i])
		{
			if (!map1->tile_map[i] || map1->tile_map[i]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(map1, i))
				{
					continue;
				}
			}

			if (map1->tile_map[i] == map2)
			{
				switch (i)
				{
					/* North */
					case 0:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						return 1;

					/* East */
					case 1:
						*x += MAP_WIDTH(map1);
						return 1;

					/* South */
					case 2:
						*y += MAP_HEIGHT(map1);
						return 1;

					/* West */
					case 3:
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;

					/* Northest */
					case 4:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x += MAP_WIDTH(map1);
						return 1;

					/* Southest */
					case 5:
						*y += MAP_HEIGHT(map1);
						*x += MAP_WIDTH(map1);
						return 1;

					/* Southwest */
					case 6:
						*y += MAP_HEIGHT(map1);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;

					/* Northwest */
					case 7:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return 1;
				}
			}
		}
	}

	/* Avoid overflow of traversal_id */
	if (traversal_id == 4294967295U)
	{
		mapstruct *m;

		LOG(llevDebug, "DEBUG: relative_tile_position(): resetting traversal id\n");

		for (m = first_map; m != NULL; m = m->next)
		{
			m->traversed = 0;
		}

		traversal_id = 0;
	}

	/* Recursive search */
	return relative_tile_position_rec(map1, map2, x, y, ++traversal_id);
}

/**
 * Check whether a specified map has been loaded already.
 * Returns the mapstruct which has a name matching the given argument.
 * @param name Shared string of the map path name.
 * @return ::mapstruct which has a name matching the given argument,
 * NULL if no such map. */
mapstruct *has_been_loaded_sh(shstr *name)
{
	mapstruct *map;

	if (!name || !*name)
	{
		return NULL;
	}

	if (*name != '/' && *name != '.')
	{
		LOG(llevDebug, "DEBUG: has_been_loaded_sh(): Found map name without starting '/' or '.' (%s)\n", name);
		return NULL;
	}

	for (map = first_map; map; map = map->next)
	{
		if (name == map->path)
		{
			break;
		}
	}

	return map;
}

/**
 * Makes a path absolute outside the world of Atrinik.
 *
 * In other words, it prepends LIBDIR/MAPDIR/ to the given path and
 * returns the pointer to a static array containing the result.
 * @param name Path of the map.
 * @return The full path. */
char *create_pathname(const char *name)
{
	static char buf[MAX_BUF];

	if (*name == '/')
	{
		snprintf(buf, sizeof(buf), "%s%s", settings.mapdir, name);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s/%s", settings.mapdir, name);
	}

	return buf;
}

/**
 * This makes absolute path to the itemfile where unique objects will be
 * saved.
 *
 * Converts '/' to '@'.
 * @param s Path of the map for the items.
 * @return The absolute path. */
static char *create_items_path(shstr *s)
{
	static char buf[MAX_BUF];
	char *t;

	if (*s == '/')
	{
		s++;
	}

	snprintf(buf, sizeof(buf), "%s/%s/", settings.localdir, settings.uniquedir);

	for (t = buf + strlen(buf); *s; s++, t++)
	{
		if (*s == '/')
		{
			*t = '@';
		}
		else
		{
			*t = *s;
		}
	}

	*t = '\0';
	return buf;
}

/**
 * This function checks if a file with the given path exists.
 *
 * It tries out all the compression suffixes listed in the @ref uncomp
 * array.
 * @param name Name of the file to check.
 * @param prepend_dir If set, then we call create_pathname(), which
 * prepends libdir and mapdir. Otherwise, we assume the name given is
 * fully complete.
 * @return -1 if it fails, otherwise the mode of the file. */
int check_path(const char *name, int prepend_dir)
{
	char buf[MAX_BUF];
#ifndef WIN32
	char *endbuf;
	struct stat statbuf;
	int mode = 0, i;
#endif

	if (prepend_dir)
	{
		strcpy(buf, create_pathname(name));
	}
	else
	{
		strcpy(buf, name);
	}

#ifdef WIN32
	return _access(buf, 0);
#else
	endbuf = buf + strlen(buf);

	for (i = 0; i < NROF_COMPRESS_METHODS; i++)
	{
		if (uncomp[i][0])
		{
			strcpy(endbuf, uncomp[i][0]);
		}
		else
		{
			*endbuf = '\0';
		}

		if (!stat(buf, &statbuf))
		{
			break;
		}
	}

	if (i == NROF_COMPRESS_METHODS)
	{
		return -1;
	}

	if (!S_ISREG(statbuf.st_mode))
	{
		return -1;
	}

	if (((statbuf.st_mode & S_IRGRP) && getegid() == statbuf.st_gid) || ((statbuf.st_mode & S_IRUSR) && geteuid() == statbuf.st_uid) || (statbuf.st_mode & S_IROTH))
	{
		mode |= 4;
	}

	if ((statbuf.st_mode & S_IWGRP && getegid() == statbuf.st_gid) || (statbuf.st_mode & S_IWUSR && geteuid() == statbuf.st_uid) || (statbuf.st_mode & S_IWOTH))
	{
		mode |= 2;
	}

	return mode;
#endif
}

/**
 * Make path absolute and remove ".." and "." entries.
 *
 * path will become a normalized (absolute) version of the path in dst,
 * with all relative path references (".." and "." - parent directory and
 * same directory) resolved (path will not contain any ".." or "."
 * elements, even if dst did).
 *
 * If dst was not already absolute, the directory part of src will be
 * used as the base path and dst will be added to it.
 * @param src Already normalized file name for finding absolute path.
 * @param dst Path to normalize. Should be either an absolute path or a
 * path relative to src.
 * @param path Buffer for normalized path.
 * @return Pointer to path. */
char *normalize_path(const char *src, const char *dst, char *path)
{
	char *p;
	char buf[HUGE_BUF];

	if (*dst == '/')
	{
		strcpy(buf, dst);
	}
	else
	{
		strcpy(buf, src);

		if ((p = strrchr(buf, '/')))
		{
			p[1] = '\0';
		}
		else
		{
			strcpy(buf, "/");
		}

		strcat(buf, dst);
	}

	p = buf;

	if (strstr(p, "//"))
	{
		LOG(llevBug, "BUG: Map path with unhandled '//' element: %s\n", buf);
	}

	*path = '\0';
	p = strtok(p, "/");

	while (p)
	{
		/* Ignore "./" path elements */
		if (!strcmp(p, "."))
		{
		}
		else if (!strcmp(p, ".."))
		{
			/* Remove last inserted path element from 'path' */
			char *separator = strrchr(path, '/');

			if (separator)
			{
				*separator = '\0';
			}
			else
			{
				LOG(llevBug, "BUG: Illegal path (too many \"..\" entries): %s\n", dst);
				*path = '\0';
				return path;
			}
		}
		else
		{
			strcat(path, "/");
			strcat(path, p);
		}

		p = strtok(NULL, "/");
	}

	return path;
}

/**
 * Prints out debug-information about a map.
 * @param m Map to dump. */
void dump_map(mapstruct *m)
{
	LOG(llevSystem, "Map %s status: %d.\n", m->path, m->in_memory);
	LOG(llevSystem, "Size: %dx%d Start: %d, %d\n", MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m));

	if (m->msg != NULL)
	{
		LOG(llevSystem, "Message:\n%s", m->msg);
	}

	if (m->tmpname != NULL)
	{
		LOG(llevSystem, "Tmpname: %s\n", m->tmpname);
	}

	LOG(llevSystem, "Difficulty: %d\n", m->difficulty);
	LOG(llevSystem, "Darkness: %d\n", m->darkness);
	LOG(llevSystem, "Light: %d\n", m->light_value);
	LOG(llevSystem, "Outdoor: %d\n", MAP_OUTDOORS(m));
}

/**
 * Prints out debug information about all maps.
 *
 * This basically just goes through all the maps and calls dump_map() on
 * each one. */
void dump_all_maps()
{
	mapstruct *m;

	for (m = first_map; m != NULL; m = m->next)
	{
		dump_map(m);
	}
}

/**
 * Check if there is a wall on specified map at x, y.
 *
 * Caller should check for @ref P_PASS_THRU in the return value to see if
 * it can cross here.
 *
 * The @ref P_PLAYER_ONLY flag here is analyzed without checking the
 * caller type. That is possible because player movement related
 * functions should always used blocked().
 * @param m Map we're checking for.
 * @param x X position where to check.
 * @param y Y position where to check.
 * @return 1 if a wall is present at the given location. */
int wall(mapstruct *m, int x, int y)
{
	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);
	}

	return (GET_MAP_FLAGS(m, x, y) & (P_DOOR_CLOSED | P_PLAYER_ONLY | P_NO_PASS | P_PASS_THRU));
}

/**
 * Check if it's impossible to see through the given coordinate on the
 * given map.
 * @param m Map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return 1 if the given location blocks view. */
int blocks_view(mapstruct *m, int x, int y)
{
	mapstruct *nm;

	if (!(nm = get_map_from_coord(m, &x, &y)))
	{
		return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);
	}

	return (GET_MAP_FLAGS(nm, x, y) & P_BLOCKSVIEW);
}

/**
 * Check if given coordinates on the given map block magic.
 * @param m Map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return 1 if the given location blocks magic. */
int blocks_magic(mapstruct *m, int x, int y)
{
	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return (P_BLOCKSVIEW | P_NO_PASS | P_NO_MAGIC | P_OUT_OF_MAP);
	}

	return (GET_MAP_FLAGS(m, x, y) & P_NO_MAGIC);
}

/**
 * Check if clerical spells do not work on given coordinates on the given
 * map.
 * @param m Map.
 * @param x X position on the map.
 * @param y Y position on the map.
 * @return 1 if the given location blocks prayers. */
int blocks_cleric(mapstruct *m, int x, int y)
{
	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return (P_BLOCKSVIEW | P_NO_PASS | P_NO_CLERIC | P_OUT_OF_MAP);
	}

	return (GET_MAP_FLAGS(m, x, y) & P_NO_CLERIC);
}

/**
 * Check if specified object cannot move onto x, y on the given map and
 * terrain.
 * @param op Object.
 * @param m The map.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param terrain Terrain type.
 * @return 0 if not blocked by anything, combination of
 * @ref map_look_flags otherwise. */
int blocked(object *op, mapstruct *m, int x, int y, int terrain)
{
	int flags;
	MapSpace *msp;

	flags = (msp = GET_MAP_SPACE_PTR(m, x, y))->flags;

	/* First, look at the terrain. If we don't have a valid terrain flag,
	 * this is forbidden to enter. */
	if (msp->move_flags & ~terrain)
	{
		return ((flags & (P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU)) | P_NO_TERRAIN);
	}

	if (flags & P_IS_ALIVE)
	{
		return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));
	}

	if (flags & P_NO_PASS)
	{
		if (!(flags & P_PASS_THRU) || !op || !QUERY_FLAG(op, FLAG_CAN_PASS_THRU))
		{
			return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));
		}
	}

	if (flags & P_IS_PLAYER)
	{
		if (!op || (m->map_flags & MAP_FLAG_PVP && !(flags & P_NO_PVP)))
		{
			return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));
		}

		if (op->type != PLAYER)
		{
			return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));
		}
	}

	/* We have an object pointer - do some last checks */
	if (op)
	{
		/* Player only space and not a player - no pass and possible checker here */
		if ((flags & P_PLAYER_ONLY) && op->type != PLAYER)
		{
			return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV | P_PLAYER_ONLY));
		}

		if (flags & P_CHECK_INV)
		{
			if (blocked_tile(op, m, x, y))
			{
				return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV));
			}
		}
	}

	return (flags & (P_DOOR_CLOSED));
}

/**
 * Returns true if the given coordinate is blocked except by the object
 * passed is not blocking. This is used with multipart monsters - if we
 * want to see if a 2x2 monster can move 1 space to the left, we don't
 * want its own area to block it from moving there.
 * @param op The monster object.
 * @param xoff X position offset.
 * @param yoff Y position offset.
 * @return 0 if the space to check is not blocked by anything other than
 * the monster, return value of blocked() otherwise. */
int blocked_link(object *op, int xoff, int yoff)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int xtemp, ytemp, flags;

	for (tmp = op; tmp; tmp = tmp->more)
	{
		/* We search for this new position */
		xtemp = tmp->arch->clone.x + xoff;
		ytemp = tmp->arch->clone.y + yoff;

		/* Check if match is a different part of us */
		for (tmp2 = op; tmp2; tmp2 = tmp2->more)
		{
			/* If this is true, we can be sure this position is valid */
			if (xtemp == tmp2->arch->clone.x && ytemp == tmp2->arch->clone.y)
			{
				break;
			}
		}

		/* If this is NULL, tmp will move in a new node */
		if (!tmp2)
		{
			xtemp = tmp->x + xoff;
			ytemp = tmp->y + yoff;

			/* If this new node is illegal, we can skip all */
			if (!(m = get_map_from_coord(tmp->map, &xtemp, &ytemp)))
			{
				return -1;
			}

			/* We use always head for tests - no need to copy any flags to the tail */
			if ((flags = blocked(op, m, xtemp, ytemp, op->terrain_flag)))
			{
				if ((flags & P_DOOR_CLOSED) && (op->behavior & BEHAVIOR_OPEN_DOORS))
				{
					if (open_door(op, m, xtemp, ytemp, 1))
					{
						continue;
					}
				}

				return flags;
			}
		}
	}

	return 0;
}

/**
 * Same as blocked_link(), but using an absolute coordinate (map, x, y).
 * @param op The monster object.
 * @param map The map.
 * @param x X coordinate on the map.
 * @param y Y coordinate on the map.
 * @return 0 if the space to check is not blocked by anything other than
 * the monster, return value of blocked() otherwise.
 * @todo This function should really be combined with the above to reduce
 * code duplication. */
int blocked_link_2(object *op, mapstruct *map, int x, int y)
{
	object *tmp, *tmp2;
	int xtemp, ytemp, flags;
	mapstruct *m;

	for (tmp = op; tmp; tmp = tmp->more)
	{
		/* We search for this new position */
		xtemp = x + tmp->arch->clone.x;
		ytemp = y + tmp->arch->clone.y;

		/* Check if match is a different part of us */
		for (tmp2 = op; tmp2; tmp2 = tmp2->more)
		{
			/* If this is true, we can be sure this position is valid */
			if (xtemp == tmp2->x && ytemp == tmp2->y)
			{
				break;
			}
		}

		/* If this is NULL, tmp will move in a new node */
		if (!tmp2)
		{
			/* If this new node is illegal, we can skip all */
			if (!(m = get_map_from_coord(map, &xtemp, &ytemp)))
			{
				return -1;
			}

			/* We use always head for tests - no need to copy any flags to the tail */
			if ((flags = blocked(op, m, xtemp, ytemp, op->terrain_flag)))
			{
				if ((flags & P_DOOR_CLOSED) && (op->behavior & BEHAVIOR_OPEN_DOORS))
				{
					if (open_door(op, m, xtemp, ytemp, 0))
					{
						continue;
					}
				}

				return flags;
			}
		}
	}

	return 0;
}

/**
 * This is used for any action which needs to browse through the objects
 * of the tile node, for special objects like inventory checkers.
 * @param op Object trying to move to map at x, y.
 * @param m Map we want to check.
 * @param x X position to check for.
 * @param y Y position to check for.
 * @return 1 if the tile is blocked, 0 otherwise. */
int blocked_tile(object *op, mapstruct *m, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
	{
		/* This must be before the checks below. Code for inventory checkers. */
		if (tmp->type == CHECK_INV && tmp->last_grace)
		{
			/* If last_sp is set, the player/monster needs an object,
			 * so we check for it. If they don't have it, they can't
			 * pass through this space. */
			if (tmp->last_sp)
			{
				if (check_inv_recursive(op, tmp) == NULL)
				{
					return 1;
				}

				continue;
			}
			/* In this case, the player must not have the object -
			 * if they do, they can't pass through. */
			else
			{
				if (check_inv_recursive(op, tmp) != NULL)
				{
					return 1;
				}

				continue;
			}
		}
	}

	return 0;
}

/**
 * Check if an archetype can fit to a position.
 * @param at Archetype to check.
 * @param op Object. If not NULL, will check for terrain as well.
 * @param m Map.
 * @param x X position.
 * @param y Y position.
 * @retval 0 No block.
 * @retval -1 Out of map.
 * @retval other Blocking flags from blocked(). */
int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y)
{
	archetype *tmp;
	mapstruct *mt;
	int xt, yt, t;

	if (op)
	{
		t = op->terrain_flag;
	}
	else
	{
		t = TERRAIN_ALL;
	}

	if (at == NULL)
	{
		if (!(m = get_map_from_coord(m, &x, &y)))
		{
			return -1;
		}

		return blocked(op, m, x, y, t);
	}

	for (tmp = at; tmp; tmp = tmp->more)
	{
		xt = x + tmp->clone.x;
		yt = y + tmp->clone.y;

		if (!(mt = get_map_from_coord(m, &xt, &yt)))
		{
			return -1;
		}

		if ((xt = blocked(op, mt, xt, yt, t)))
		{
			return xt;
		}
	}

	return 0;
}

/**
 * Loads (and parses) the objects into a given map from the specified
 * file pointer.
 * @param m Map being loaded.
 * @param fp File to read from.
 * @param mapflags The same as we get with load_original_map(). */
static void load_objects(mapstruct *m, FILE *fp, int mapflags)
{
	int i;
	archetype *tail;
	void *mybuffer;
	object *op, *prev = NULL, *last_more = NULL, *tmp;

	op = get_object();

	/* To handle buttons correctly */
	op->map = m;
	mybuffer = create_loader_buffer(fp);

	while ((i = load_object(fp, op, mybuffer, LO_REPEAT, mapflags)))
	{
		if (i == LL_MORE)
		{
			LOG(llevDebug, "BUG: load_objects(%s): object %s - its a tail!\n", m->path ? m->path : ">no map<", query_short_name(op, NULL));
			continue;
		}

		/* If the archetype for the object is null, means that we
		 * got an invalid object. Don't do anythign with it - the game
		 * will not be able to do anything with it either. */
		if (op->arch == NULL)
		{
			LOG(llevDebug, "BUG:load_objects(%s): object %s (%d)- invalid archetype. (pos:%d,%d)\n", m->path ? m->path : ">no map<", query_short_name(op, NULL), op->type, op->x, op->y);
			continue;
		}

		/* Do some safety for containers */
		if (op->type == CONTAINER)
		{
			/* Used for containers as link to players viewing it */
			op->attacked_by = NULL;
			op->attacked_by_count = 0;
			sum_weight(op);
		}

		if (op->type == MONSTER)
		{
			fix_monster(op);
		}

		/* Important pre set for the animation/face of a object */
		if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE))
		{
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);
		}

		/* We have a multi arch head? */
		if (op->arch->more)
		{
			tail = op->arch->more;
			prev = op, last_more = op;

			/* Clone the tail using the default arch */
			do
			{
				tmp = get_object();
				copy_object(&tail->clone, tmp, 0);

				tmp->x += op->x;
				tmp->y += op->y;
				tmp->map = op->map;

				/* Adjust the single object specific data except flags. */
				tmp->type = op->type;
				tmp->layer = op->layer;

				/* Link the tail object... */
				tmp->head = prev, last_more->more = tmp, last_more = tmp;

			}
			while ((tail = tail->more));

			/* To speed up some core functions like moving or remove_ob()/insert_ob
			 * and because there are some "arch depending and not object depending"
			 * flags, we init the tails with some of the head settings. */

			if (QUERY_FLAG(op, FLAG_SYS_OBJECT))
			{
				SET_MULTI_FLAG(op->more, FLAG_SYS_OBJECT);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_SYS_OBJECT);
			}

			if (QUERY_FLAG(op, FLAG_NO_APPLY))
			{
				SET_MULTI_FLAG(op->more, FLAG_NO_APPLY);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_NO_APPLY);
			}

			if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
			{
				SET_MULTI_FLAG(op->more, FLAG_IS_INVISIBLE);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_IS_INVISIBLE);
			}

			if (QUERY_FLAG(op, FLAG_IS_ETHEREAL))
			{
				SET_MULTI_FLAG(op->more, FLAG_IS_ETHEREAL);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_IS_ETHEREAL);
			}

			if (QUERY_FLAG(op, FLAG_CAN_PASS_THRU))
			{
				SET_MULTI_FLAG(op->more, FLAG_CAN_PASS_THRU);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_CAN_PASS_THRU);
			}

			if (QUERY_FLAG(op, FLAG_FLYING))
			{
				SET_MULTI_FLAG(op->more, FLAG_FLYING);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_FLYING);
			}

			if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
			{
				SET_MULTI_FLAG(op->more, FLAG_BLOCKSVIEW);
			}
			else
			{
				CLEAR_MULTI_FLAG(tmp->more, FLAG_BLOCKSVIEW);
			}
		}

		insert_ob_in_map(op, m, op, INS_NO_MERGE | INS_NO_WALK_ON);

		/* auto_apply() will remove FLAG_AUTO_APPLY after first use */
		if (QUERY_FLAG(op, FLAG_AUTO_APPLY))
		{
			auto_apply(op);
		}
		/* For fresh maps, create treasures */
		else if ((mapflags & MAP_ORIGINAL) && op->randomitems)
		{
			create_treasure(op->randomitems, op, op->type != TREASURE ? GT_APPLY : 0, op->level ? op->level : m->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);
		}

		op = get_object();
		op->map = m;
	}

	delete_loader_buffer(mybuffer);

	m->in_memory = MAP_IN_MEMORY;
	check_light_source_list(m);
}

/**
 * This saves all the objects on the map in a non destructive fashion.
 * @param m Map to save.
 * @param fp File where regular objects are saved.
 * @param fp2 File to save unique objects. */
static void save_objects(mapstruct *m, FILE *fp, FILE *fp2)
{
	int i, j = 0, unique = 0;
	object *head, *op, *otmp, *tmp, *last_valid;

	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			for (op = get_map_ob (m, i, j); op; op = otmp)
			{
				otmp = op->above;

				last_valid = op->below;

				if (op->type == PLAYER)
				{
					continue;
				}

				head = op->head ? op->head : op;

				if (QUERY_FLAG(head, FLAG_NO_SAVE))
				{
					remove_ob(head);
					check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

					/* Invalid next pointer */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
						{
							otmp = op->above;
						}
						else if (last_valid)
						{
							otmp = last_valid->above;
						}
						/* Should be really rare */
						else
						{
							otmp = get_map_ob(m, i, j);
						}
					}

					continue;
				}
				/* Don't save spawn point monsters; instead, get the spawn point
				 * they came from, and reset that spawn point so it will generate
				 * a new monster when the map loads again. */
				else if (QUERY_FLAG(op, FLAG_SPAWN_MOB))
				{
					/* Browse the inventory for the spawn info */
					for (tmp = head->inv; tmp; tmp = tmp->below)
					{
						if (tmp->type == SPAWN_POINT_INFO)
						{
							if (tmp->owner && tmp->owner->type == SPAWN_POINT)
							{
								/* Force a pre spawn setting */
								tmp->owner->stats.sp = tmp->owner->last_sp;
								/* We force active spawn point */
								tmp->owner->speed_left += 1.0f;
								tmp->owner->enemy = NULL;
							}
							else
							{
								LOG(llevBug, "BUG: Spawn mob (%s (%s)) has SPAWN INFO without or illegal owner set (%s)!\n", op->arch->name, query_name(head, NULL), query_name(tmp->owner, NULL));
							}

							remove_ob(head);
							check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

							goto save_objects_jump1;
						}
					}

					LOG(llevBug, "BUG: Spawn mob (%s %s) without SPAWN INFO.\n", head->arch->name, query_name(head, NULL));
					remove_ob(head);
					check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

					if (!OBJECT_FREE(tmp) && tmp->owner && tmp->owner->type == SPAWN_POINT)
					{
						tmp->owner->enemy = NULL;
					}

save_objects_jump1:

					/* Invalid next pointer */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
						{
							otmp = op->above;
						}
						else if (last_valid)
						{
							otmp = last_valid->above;
						}
						/* Should be really rare */
						else
						{
							otmp = get_map_ob(m, i, j);
						}
					}

					continue;
				}
				else if (op->type == SPAWN_POINT)
				{
					if (op->enemy)
					{
						if (op->enemy_count == op->enemy->count && !QUERY_FLAG(op->enemy, FLAG_REMOVED) && !OBJECT_FREE(op->enemy))
						{
							/* Force a pre spawn setting */
							op->stats.sp = op->last_sp;
							op->speed_left += 1.0f;
							/* And delete the spawn */
							remove_ob(op->enemy);
							check_walk_off (op->enemy, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);
							op->enemy = NULL;

							/* Invalid next pointer */
							if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
							{
								if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
								{
									otmp = op->above;
								}
								else if (last_valid)
								{
									otmp = last_valid->above;
								}
								/* Should be really rare */
								else
								{
									otmp = get_map_ob(m, i, j);
								}
							}
						}
					}
				}

				if (head->owner)
				{
					LOG(llevDebug, "WARNING (only debug): save_obj(): obj w. owner. map:%s obj:%s (%s) (%d,%d)\n", m->path, query_name(op, NULL), op->arch && op->arch->name ? op->arch->name : "<no arch name>", op->x, op->y);
					head->owner = NULL;
					continue;
				}
			}
		}
	}

	/* The map is now cleared from non-static objects on this or other maps
	 * (when the source was from this map). Now all can be saved as a legal
	 * snapshot of the map. */
	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			unique = 0;

			for (op = get_map_ob(m, i, j); op; op = otmp)
			{
				otmp = op->above;

				last_valid = op->below;

				if (QUERY_FLAG(op, FLAG_IS_FLOOR) && QUERY_FLAG(op, FLAG_UNIQUE))
				{
					unique = 1;
				}

				/* Do some testing... */
				if (op->type == PLAYER)
				{
					continue;
				}

				if (op->head)
				{
					int xt, yt;

					tmp = op->head;
					xt = tmp->x;
					yt = tmp->y;
					tmp->x = op->x - op->arch->clone.x;
					tmp->y = op->y - op->arch->clone.y;

					if (unique || QUERY_FLAG(tmp, FLAG_UNIQUE))
					{
						save_object(fp2 , tmp, 3);
					}
					else
					{
						save_object(fp, tmp, 3);
					}

					tmp->x = xt;
					tmp->y = yt;
					/* Technical remove, no walk off check */
					remove_ob(tmp);

					/* Invalid next pointer */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
						{
							otmp = op->above;
						}
						else if (last_valid)
						{
							otmp = last_valid->above;
						}
						/* Should be really rare */
						else
						{
							otmp = get_map_ob(m, i, j);
						}
					}

					continue;
				}

				if (unique || QUERY_FLAG(op, FLAG_UNIQUE))
				{
					save_object(fp2, op, 3);
				}
				else
				{
					save_object(fp, op, 3);
				}

				/* It's a head */
				if (op->more)
				{
					remove_ob(op);

					/* Invalid next pointer */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
						{
							otmp = op->above;
						}
						else if (last_valid)
						{
							otmp = last_valid->above;
						}
						/* Should be really rare */
						else
						{
							otmp = get_map_ob(m, i, j);
						}
					}
				}
			}
		}
	}
}

/**
 * Sets darkness for map, both light_value and darkness.
 *
 * Takes care of checking the passed value.
 * @param m Pointer to the map's structure.
 * @param value The darkness value. */
void set_map_darkness(mapstruct *m, int value)
{
	if (value < 0 || value > MAX_DARKNESS)
	{
		value = MAX_DARKNESS;
	}

	MAP_DARKNESS(m) = (sint32) value;
	m->light_value = (sint32) global_darkness_table[value];
}

/**
 * Allocates, initialises, and returns a pointer to a mapstruct.
 * @return The new map structure. */
mapstruct *get_linked_map()
{
	mapstruct *map = (mapstruct *) calloc(1, sizeof(mapstruct));

	if (map == NULL)
	{
		LOG(llevError, "ERROR: get_linked_map(): Out of memory.\n");
	}

	if (first_map)
	{
		first_map->previous = map;
	}

	map->next = first_map;
	first_map = map;

	map->in_memory = MAP_SWAPPED;

	/* The maps used to pick up default x and y values from the
	 * map archetype. Mimic that behaviour. */
	MAP_WIDTH(map) = 16;
	MAP_HEIGHT(map) = 16;
	MAP_RESET_TIMEOUT(map) = 0;
	MAP_TIMEOUT(map) = MAP_DEFAULTTIMEOUT;
	set_map_darkness(map, MAP_DEFAULT_DARKNESS);

	MAP_ENTER_X(map) = 0;
	MAP_ENTER_Y(map) = 0;

	return map;
}

/**
 * This basically allocates the dynamic array of spaces for the
 * map.
 * @param m Map to allocate spaces for. */
static void allocate_map(mapstruct *m)
{
	m->in_memory = MAP_LOADING;

	if (m->spaces || m->bitmap)
	{
		LOG(llevError, "ERROR: allocate_map(): Callled with already allocated map (%s)\n", m->path);
	}

	if (m->buttons)
	{
		LOG(llevBug, "BUG: allocate_map(): Callled with already set buttons (%s)\n", m->path);
	}

	m->spaces = calloc(1, MAP_WIDTH(m) * MAP_HEIGHT(m) * sizeof(MapSpace));

	m->bitmap = malloc(((MAP_WIDTH(m) + 31) / 32) * MAP_HEIGHT(m) * sizeof(uint32));

	if (m->spaces == NULL || m->bitmap == NULL)
	{
		LOG(llevError, "ERROR: allocate_map(): Out of memory.\n");
	}
}

/**
 * Creates and returns a map of the specified size.
 *
 * Used in random maps code.
 * @param sizex X size of the map.
 * @param sizey Y size of the map.
 * @return The new map structure. */
mapstruct *get_empty_map(int sizex, int sizey)
{
	mapstruct *m = get_linked_map();
	m->width = sizex;
	m->height = sizey;
	allocate_map(m);

	m->in_memory = MAP_IN_MEMORY;

	return m;
}

/**
 * Opens the file "filename" and reads information about the map
 * from the given file, and stores it in a newly allocated
 * mapstruct.
 * @param filename Map path.
 * @param flags One of (or combination of):
 * - @ref MAP_PLAYER_UNIQUE: We don't do any name changes.
 * - @ref MAP_BLOCK: We block on this load. This happens in all cases, no
 *   matter if this flag is set or not.
 * - @ref MAP_STYLE: Style map - don't add active objects, don't add to
 *   server managed map list.
 * @return The loaded map structure, NULL on failure. */
mapstruct *load_original_map(const char *filename, int flags)
{
	FILE *fp;
	mapstruct *m;
	int comp;
	char pathname[MAX_BUF], tmp_fname[MAX_BUF];

	/* No sense in doing this all for random maps, it will all fail anyways. */
	if (!strncmp(filename, "/random/", 8))
	{
		return NULL;
	}

	if (*filename != '/' && *filename != '.')
	{
		LOG(llevDebug, "DEBUG: load_original_map(): Filename without starting '/' - fixed. %s\n", filename);
		tmp_fname[0] = '/';
		strcpy(tmp_fname + 1, filename);
		filename = tmp_fname;
	}

	if (flags & MAP_PLAYER_UNIQUE)
	{
		LOG(llevDebug, "load_original_map unique: %s (%x)\n", filename, flags);
		strcpy(pathname, filename);
	}
	else
	{
		LOG(llevDebug, "load_original_map: %s (%x) ", filename, flags);
		strcpy(pathname, create_pathname(filename));
	}

	if ((fp = open_and_uncompress(pathname, 0, &comp)) == NULL)
	{
		if (!(flags & MAP_PLAYER_UNIQUE))
		{
			LOG(llevBug, "BUG: Can't open map file %s\n", pathname);
		}

		return NULL;
	}

	LOG(llevDebug, "link map. ");
	m = get_linked_map();

	LOG(llevDebug, "header: ");
	FREE_AND_COPY_HASH(m->path, filename);

	if (!load_map_header(m, fp))
	{
		LOG(llevBug, "BUG: Failure loading map header for %s, flags=%d\n", filename, flags);
		delete_map(m);
		return NULL;
	}

	LOG(llevDebug, "alloc. ");
	allocate_map(m);
	m->compressed = comp;

	m->in_memory = MAP_LOADING;
	LOG(llevDebug, "load objs:");
	load_objects(m, fp, (flags & (MAP_BLOCK | MAP_STYLE)) | MAP_ORIGINAL);
	LOG(llevDebug, "close. ");
	close_and_delete(fp, comp);
	LOG(llevDebug, "post set. ");

	if (!MAP_DIFFICULTY(m))
	{
		LOG(llevBug, "\nBUG: Map %s has difficulty 0. Changing to 1.\n", filename);
		MAP_DIFFICULTY(m) = 1;
	}

	set_map_reset_time(m);
	LOG(llevDebug, "done!\n");
	return m;
}

/**
 * Loads a map, which has been loaded earlier, from file.
 * @param m Map we want to reload.
 * @return The map object we load into (this can change from the passed
 * option if we can't find the original map). */
static mapstruct *load_temporary_map(mapstruct *m)
{
	FILE *fp;
	int comp;
	char buf[MAX_BUF];

	if (!m->tmpname)
	{
		LOG(llevBug, "BUG: No temporary filename for map %s! Fallback to original!\n", m->path);
		strcpy(buf, m->path);
		delete_map(m);
		m = load_original_map(buf, 0);

		if (m == NULL)
		{
			return NULL;
		}

		return m;
	}

	LOG(llevDebug, "load_temporary_map: %s (%s) ", m->tmpname, m->path);

	if ((fp = open_and_uncompress(m->tmpname, 0, &comp)) == NULL)
	{
		if (!strncmp(m->path, "/random/", 8))
		{
			return NULL;
		}

		LOG(llevBug, "BUG: Can't open temporary map %s! Fallback to original!\n", m->tmpname);
		strcpy(buf, m->path);
		delete_map(m);
		m = load_original_map(buf, 0);

		if (m == NULL)
		{
			return NULL;
		}

		return m;
	}

	LOG(llevDebug, "header: ");

	if (!load_map_header(m, fp))
	{
		LOG(llevBug, "BUG: Error loading map header for %s (%s)! Fallback to original!\n", m->path, m->tmpname);
		delete_map(m);
		m = load_original_map(m->path, 0);

		if (m == NULL)
		{
			return NULL;
		}

		return m;
	}

	LOG(llevDebug, "alloc. ");
	m->compressed = comp;
	allocate_map(m);

	m->in_memory = MAP_LOADING;
	LOG(llevDebug, "load objs:");
	load_objects (m, fp, 0);
	LOG(llevDebug, "close. ");
	close_and_delete(fp, comp);
	LOG(llevDebug, "done!\n");
	return m;
}

/**
 * Goes through a map and removes any unique items on the map.
 * @param m The map to go through. */
static void delete_unique_items(mapstruct *m)
{
	int i, j, unique = 0;
	object *op, *next;

	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			unique = 0;

			for (op = get_map_ob(m, i, j); op; op = next)
			{
				next = op->above;

				if (QUERY_FLAG(op, FLAG_IS_FLOOR) && QUERY_FLAG(op, FLAG_UNIQUE))
				{
					unique = 1;
				}

				if (op->head == NULL && (QUERY_FLAG(op, FLAG_UNIQUE) || unique))
				{
					if (QUERY_FLAG(op, FLAG_IS_LINKED))
					{
						remove_button_link(op);
					}

					remove_ob(op);
					check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				}
			}
		}
	}
}

/**
 * Loads unique objects from file(s) into the map which is in memory.
 * @param m The map to load unique items into. */
static void load_unique_objects(mapstruct *m)
{
	FILE *fp;
	int comp, count;
	char firstname[MAX_BUF];

	for (count = 0; count < 10; count++)
	{
		sprintf(firstname, "%s.v%02d", create_items_path(m->path), count);

		if (!access(firstname, R_OK))
		{
			break;
		}
	}

	/* If we get here, we did not find any map */
	if (count == 10)
	{
		return;
	}

	LOG(llevDebug, "open unique items file for %s\n", create_items_path(m->path));

	if ((fp = open_and_uncompress(firstname, 0, &comp)) == NULL)
	{
		/* There is no expectation that every map will have unique items, but this
		 * is debug output, so leave it in. */
		LOG(llevDebug, "Can't open unique items file for %s\n", create_items_path(m->path));
		return;
	}

	m->in_memory = MAP_LOADING;

	/* If we have loaded unique items from */
	if (m->tmpname == NULL)
	{
		delete_unique_items(m);
	}

	load_objects(m, fp, 0);
	close_and_delete(fp, comp);
}

/**
 * Saves a map to file.  If flag is set, it is saved into the same
 * file it was (originally) loaded from.  Otherwise a temporary
 * filename will be genarated, and the file will be stored there.
 * The temporary filename will be stored in the mapstructure.
 * If the map is unique, we also save to the filename in the map
 * (this should have been updated when first loaded).
 * @param m The map to save.
 * @param flag Save flag.
 * @return  */
int new_save_map(mapstruct *m, int flag)
{
	FILE *fp, *fp2;
	char filename[MAX_BUF], buf[MAX_BUF];

	if (flag && !*m->path)
	{
		LOG(llevBug, "BUG: Tried to save map without path.\n");
		return -1;
	}

	if (flag || MAP_UNIQUE(m))
	{
		if (!MAP_UNIQUE(m))
		{
			strcpy(filename, create_pathname(m->path));
		}
		else
		{
			/* This ensures we always reload from original maps */
			if (MAP_NOSAVE(m))
			{
				LOG(llevDebug, "skip map %s (no_save flag)\n", m->path);
				return 0;
			}

			strcpy(filename, m->path);
		}

		/* If the compression suffix already exists on the filename, don't
		 * put it on again.  This nasty looking strcmp checks to see if the
		 * compression suffix is at the end of the filename already.
		 * i don't checked them - perhaps weneed compression in the future
		 * even i can't see it - the if is harmless because self terminating
		 * after the m->compressed fails. */
		if (m->compressed && strcmp((filename + strlen(filename) - strlen(uncomp[m->compressed][0])), uncomp[m->compressed][0]))
		{
			strcat(filename, uncomp[m->compressed][0]);
		}

		make_path_to_file(filename);
	}
	else
	{
		if (m->tmpname == NULL)
		{
			m->tmpname = tempnam_local(settings.tmpdir, NULL);
		}

		strcpy(filename, m->tmpname);
	}

	LOG(llevDebug, "Saving map %s to %s\n", m->path, filename);

	m->in_memory = MAP_SAVING;

	/* Compress if it isn't a temporary save.  Do compress if unique */
	if (m->compressed && (MAP_UNIQUE(m) || flag))
	{
		strcpy(buf, uncomp[m->compressed][2]);
		strcat(buf, " > ");
		strcat(buf, filename);
		fp = popen(buf, "w");
	}
	else
	{
		fp = fopen(filename, "w");
	}

	if (!fp)
	{
		LOG(llevError, "ERROR: Can't open file %s for saving.\n", filename);
		return -1;
	}

	save_map_header(m, fp, flag);

	/* Save unique items into fp2 */
	fp2 = fp;

	if (!MAP_UNIQUE(m))
	{
		snprintf(buf, sizeof(buf), "%s.v00", create_items_path(m->path));

		if ((fp2 = fopen(buf, "w")) == NULL)
		{
			LOG(llevBug, "BUG: Can't open unique items file %s\n", buf);
		}

		save_objects(m, fp, fp2);

		if (fp2)
		{
			if (ftell(fp2) == 0)
			{
				fclose(fp2);
				unlink(buf);
			}
			else
			{
				LOG(llevDebug, "Saving unique items map to %s\n", buf);
				fclose(fp2);
				chmod(buf, SAVE_MODE);
			}
		}
	}
	/* Otherwise to the same file, like apartments */
	else
	{
		save_objects(m, fp, fp);
	}

	if (fp)
	{
		if (m->compressed && !flag)
		{
			pclose(fp);
		}
		else
		{
			fclose(fp);
		}
	}

	chmod(filename, SAVE_MODE);
	return 0;
}

/**
 * Remove and free all objects in the given map.
 * @param m The map. */
static void free_all_objects(mapstruct *m)
{
	int i, j;
	object *op;

	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			object *previous_obj = NULL;

			while ((op = GET_MAP_OB(m, i, j)) != NULL)
			{
				if (op == previous_obj)
				{
					LOG(llevDebug, "free_all_objects: Link error, bailing out.\n");
					break;
				}

				previous_obj = op;

				if (op->head)
				{
					op = op->head;
				}

				remove_ob(op);
			}
		}
	}
}

/**
 * Frees everything allocated by the given map structure.
 *
 * Don't free tmpname - our caller is left to do that.
 * @param m Map to free.
 * @param flag If set, free all objects on the map. */
void free_map(mapstruct *m, int flag)
{
	int i;

	if (!m->in_memory)
	{
		LOG(llevBug, "BUG: Trying to free freed map.\n");
		return;
	}

	remove_light_source_list(m);

	if (m->buttons)
	{
		free_objectlinkpt(m->buttons);
	}

	if (flag && m->spaces)
	{
		free_all_objects(m);
	}

	FREE_AND_NULL_PTR(m->name);
	FREE_AND_NULL_PTR(m->bg_music);
	FREE_AND_NULL_PTR(m->spaces);
	FREE_AND_NULL_PTR(m->msg);
	m->buttons = NULL;
	m->first_light = NULL;

	for (i = 0; i < TILED_MAPS; i++)
	{
		/* Delete the backlinks in other tiled maps to our map */
		if (m->tile_map[i])
		{
			if (m->tile_map[i]->tile_map[map_tiled_reverse[i]] && m->tile_map[i]->tile_map[map_tiled_reverse[i]] != m)
			{
				LOG(llevBug, "BUG: Freeing map %s linked to %s which links back to another map.\n", STRING_SAFE(m->path), STRING_SAFE(m->tile_map[i]->path));
			}

			m->tile_map[i]->tile_map[map_tiled_reverse[i]] = NULL;
			m->tile_map[i] = NULL;
		}

		FREE_AND_CLEAR_HASH(m->tile_path[i]);
	}

	if (m->events)
	{
		map_event *tmp, *next;

		for (tmp = m->events; tmp; tmp = next)
		{
			next = tmp->next;
			map_event_free(tmp);
		}

		m->events = NULL;
	}

	FREE_AND_NULL_PTR(m->bitmap);
	m->in_memory = MAP_SWAPPED;
}

/**
 * Deletes all the data on the map (freeing pointers) and then removes
 * this map from the global linked list of maps.
 * @param m The map to delete. */
void delete_map(mapstruct *m)
{
	if (!m)
	{
		return;
	}

	if (m->in_memory == MAP_IN_MEMORY)
	{
		/* Change to MAP_SAVING, even though we are not,
		 * so that remove_ob doesn't do as much work. */
		m->in_memory = MAP_SAVING;
		free_map(m, 1);
	}
	else
	{
		remove_light_source_list(m);
	}

	/* Remove m from the global map list */
	if (m->next)
	{
		m->next->previous = m->previous;
	}

	if (m->previous)
	{
		m->previous->next = m->next;
	}
	/* If there is no previous, we are first map */
	else
	{
		first_map = m->next;
	}

	/* tmpname can still be needed if the map is swapped out, so we don't
	 * do it in free_map(). */
	FREE_AND_NULL_PTR(m->tmpname);
	FREE_AND_CLEAR_HASH(m->path);
	free(m);
}

/**
 * Makes sure the given map is loaded and swapped in.
 * @param name Path name of the map.
 * @param flags Possible flags:
 * - @ref MAP_FLUSH: Flush the map - always load from the map directory,
 *   and don't do unique items or the like.
 *  - @ref MAP_PLAYER_UNIQUE: This is an unique map for each player.
 *    Don't do any more name translation on it.
 * @return Pointer to the given map. */
mapstruct *ready_map_name(const char *name, int flags)
{
	mapstruct *m;
	shstr *name_sh;

	if (!name)
	{
		return NULL;
	}

	/* Have we been at this level before? */
	if (flags & MAP_NAME_SHARED)
	{
		m = has_been_loaded_sh(name);
	}
	else
	{
		/* Create a temporary shared string for the name if not explicitly given */
		name_sh = add_string(name);
		m = has_been_loaded_sh(name_sh);
		free_string_shared(name_sh);
	}

	/* Map is good to go, so just return it */
	if (m && (m->in_memory == MAP_LOADING || m->in_memory == MAP_IN_MEMORY))
	{
		return m;
	}

	/* Unique maps always get loaded from their original location, and never
	 * a temp location.  Likewise, if map_flush is set, or we have never loaded
	 * this map, load it now.  I removed the reset checking from here -
	 * it seems the probability of a player trying to enter a map that should
	 * reset but hasn't yet is quite low, and removing that makes this function
	 * a bit cleaner (and players probably shouldn't rely on exact timing for
	 * resets in any case - if they really care, they should use the 'maps command. */
	if (!m || (flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE)))
	{
		/* First visit or time to reset */
		if (m)
		{
			/* Doesn't make much difference */
			clean_tmp_map(m);
			delete_map(m);
		}

		/* Create and load a map */
		if (!(m = load_original_map(name, (flags & MAP_PLAYER_UNIQUE))))
		{
			return NULL;
		}

		/* If a player unique map, no extra unique object file to load.
		 * if from the editor, likewise. */
		if (!(flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE)))
		{
			load_unique_objects(m);
		}
	}
	else
	{
		/* If in this loop, we found a temporary map, so load it up. */
		m = load_temporary_map(m);

		if (m == NULL)
		{
			return NULL;
		}

		LOG(llevDebug, "RMN: unique. ");
		load_unique_objects(m);

		LOG(llevDebug, "clean. ");
		clean_tmp_map(m);
		m->in_memory = MAP_IN_MEMORY;
	}

	/* Below here is stuff common to both first time loaded maps and
	 * temp maps. */

	/* In case other objects press some buttons down.
	 * We handle here all kind of "triggers" which are triggered
	 * permanent by objects like buttons or inventory checkers.
	 * We don't check here instant stuff like sacrificing altars.
	 * Because this should be handled on map making side. */
	LOG(llevDebug, "buttons. ");
	update_buttons(m);
	LOG(llevDebug, "end ready_map_name(%s)\n", m->path ? m->path : "<nopath>");

	return m;
}

/**
 * Remove the temporary file used by the map.
 * @param m Map. */
void clean_tmp_map(mapstruct *m)
{
	if (m->tmpname == NULL)
	{
		return;
	}

	unlink(m->tmpname);
}

/**
 * Free all allocated maps. */
void free_all_maps()
{
	int real_maps = 0;

	while (first_map)
	{
		/* I think some of the callers above before it gets here set this to be
		 * saving, but we still want to free this data */
		if (first_map->in_memory == MAP_SAVING)
		{
			first_map->in_memory = MAP_IN_MEMORY;
		}

		delete_map(first_map);
		real_maps++;
	}

	LOG(llevDebug, "free_all_maps: Freed %d maps\n", real_maps);
}

/**
 * This function updates various attributes about a specific space on the
 * map (what it looks like, whether it blocks magic, has a living
 * creatures, prevents people from passing through, etc).
 * @param m Map to update.
 * @param x X position on the given map.
 * @param y Y position on the given map. */
void update_position(mapstruct *m, int x, int y)
{
	object *tmp;
	int flags, move_flags, light;

#ifdef DEBUG_OLDFLAGS
	int oldflags;

	if (!((oldflags = GET_MAP_FLAGS(m, x, y)) & (P_NEED_UPDATE | P_FLAGS_UPDATE)))
	{
		LOG(llevDebug, "DBUG: update_position called with P_NEED_UPDATE|P_FLAGS_UPDATE not set: %s (%d, %d)\n", m->path, x, y);
	}
#endif

	/* save our update flag */
	flags = oldflags & P_NEED_UPDATE;

	/* update our flags */
	if (oldflags & P_FLAGS_UPDATE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UP - FLAGS: %d,%d\n", x, y);
#endif
		light = move_flags = 0;

		/* This is a key function and highly often called - every saved tick is good. */
		for (tmp = get_map_ob (m, x, y); tmp; tmp = tmp->above)
		{
			if (QUERY_FLAG(tmp, FLAG_PLAYER_ONLY))
			{
				flags |= P_PLAYER_ONLY;
			}

			if (tmp->type == CHECK_INV)
			{
				flags |= P_CHECK_INV;
			}

			if (tmp->type == MAGIC_EAR)
			{
				flags |= P_MAGIC_EAR;
			}

			if (QUERY_FLAG(tmp, FLAG_IS_PLAYER))
			{
				flags |= P_IS_PLAYER;
			}

			if (QUERY_FLAG(tmp, FLAG_DOOR_CLOSED))
			{
				flags |= P_DOOR_CLOSED;
			}

			if (QUERY_FLAG(tmp, FLAG_ALIVE))
			{
				flags |= P_IS_ALIVE;
			}

			if (QUERY_FLAG(tmp, FLAG_NO_MAGIC))
			{
				flags |= P_NO_MAGIC;
			}

			if (QUERY_FLAG(tmp, FLAG_NO_CLERIC))
			{
				flags |= P_NO_CLERIC;
			}

			if (QUERY_FLAG(tmp, FLAG_BLOCKSVIEW))
			{
				flags |= P_BLOCKSVIEW;
			}

			if (QUERY_FLAG(tmp, FLAG_WALK_ON))
			{
				flags |= P_WALK_ON;
			}

			if (QUERY_FLAG(tmp, FLAG_WALK_OFF))
			{
				flags |= P_WALK_OFF;
			}

			if (QUERY_FLAG(tmp, FLAG_FLY_ON))
			{
				flags |= P_FLY_ON;
			}

			if (QUERY_FLAG(tmp, FLAG_FLY_OFF))
			{
				flags |= P_FLY_OFF;
			}

			if (QUERY_FLAG(tmp, FLAG_NO_PASS))
			{
				if (flags & P_NO_PASS)
				{
					if (!QUERY_FLAG(tmp, FLAG_PASS_THRU))
					{
						flags &= ~P_PASS_THRU;
					}
				}
				else
				{
					flags |= P_NO_PASS;

					if (QUERY_FLAG(tmp, FLAG_PASS_THRU))
					{
						flags |= P_PASS_THRU;
					}
				}
			}

			if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
			{
				move_flags |= tmp->terrain_type;
			}

			if (QUERY_FLAG(tmp, FLAG_NO_PVP))
			{
				flags |= P_NO_PVP;
			}

			if (tmp->type == MAGIC_MIRROR)
			{
				flags |= P_MAGIC_MIRROR;
			}
		}

#ifdef DEBUG_OLDFLAGS
		/* We don't want to rely on this function to have accurate flags, but
		 * since we're already doing the work, we calculate them here.
		 * if they don't match, logic is broken someplace. */
		if (((oldflags & ~(P_FLAGS_UPDATE | P_FLAGS_ONLY | P_NO_ERROR)) != flags) && (!(oldflags & P_NO_ERROR)))
		{
			LOG(llevDebug,"DBUG: update_position: updated flags do not match old flags: %s (%d,%d) old:%x != %x\n", m->path, x, y, (oldflags & ~P_NEED_UPDATE), flags);
		}
#endif

		SET_MAP_FLAGS(m, x, y, flags);
		SET_MAP_MOVE_FLAGS(m, x, y, move_flags);
	}

	/* Check if we must rebuild the map layers for client view */
	if ((oldflags & P_FLAGS_ONLY) || !(oldflags & P_NEED_UPDATE))
	{
		return;
	}

	/* Clear out need update flag */
	SET_MAP_FLAGS(m, x, y, GET_MAP_FLAGS(m, x, y) & ~P_NEED_UPDATE);
}

/**
 * Updates the map's timeout.
 * @param map Map to update. */
void set_map_reset_time(mapstruct *map)
{
	uint32 timeout = MAP_RESET_TIMEOUT(map);

	if (timeout == 0)
	{
		timeout = MAP_DEFAULTRESET;
	}

	if (timeout >= MAP_MAXRESET)
	{
		timeout = MAP_MAXRESET;
	}

	MAP_WHEN_RESET(map) = seconds() + timeout;
}

/**
 * Get real coordinates from map.
 *
 * Return NULL if no map is valid (coordinates out of bounds and no tiled
 * map), otherwise it returns the map the coordinates are really on, and
 * updates x and y to be the localized coordinates.
 * @param m Map to consider.
 * @param[out] x Will contain the real X position that was checked.
 * @param[out] y Will contain the real Y position that was checked.
 * @return Map that is at specified location. Will be NULL if not on any
 * map. */
mapstruct *get_map_from_coord(mapstruct *m, int *x, int *y)
{
	/* m should never be null, but if a tiled map fails to load below, it
	 * could happen. */
	if (!m)
	{
		return NULL;
	}

	/* Simple case - coordinates are within this local map. */
	if (*x >= 0 && *x < MAP_WIDTH(m) && *y >= 0 && *y < MAP_HEIGHT(m))
	{
		return m;
	}

	/* West, Northwest or Southwest (3, 7 or 6) */
	if (*x < 0)
	{
		/* Northwest */
		if (*y < 0)
		{
			if (!m->tile_path[7])
			{
				return NULL;
			}

			if (!m->tile_map[7] || m->tile_map[7]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(m, 7))
				{
					return NULL;
				}
			}

			*y += MAP_HEIGHT(m->tile_map[7]);
			*x += MAP_WIDTH(m->tile_map[7]);
			return get_map_from_coord(m->tile_map[7], x, y);
		}

		/* Southwest */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[6])
			{
				return NULL;
			}

			if (!m->tile_map[6] || m->tile_map[6]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(m, 6))
				{
					return NULL;
				}
			}

			*y -= MAP_HEIGHT(m);
			*x += MAP_WIDTH(m->tile_map[6]);
			return get_map_from_coord(m->tile_map[6], x, y);
		}

		/* West */
		if (!m->tile_path[3])
		{
			return NULL;
		}

		if (!m->tile_map[3] || m->tile_map[3]->in_memory != MAP_IN_MEMORY)
		{
			if (!load_and_link_tiled_map(m, 3))
			{
				return NULL;
			}
		}

		*x += MAP_WIDTH(m->tile_map[3]);
		return get_map_from_coord(m->tile_map[3], x, y);
	}

	/* East, Northeast or Southeast (1, 4 or 5) */
	if (*x >= MAP_WIDTH(m))
	{
		/* Northeast */
		if (*y < 0)
		{
			if (!m->tile_path[4])
			{
				return NULL;
			}

			if (!m->tile_map[4] || m->tile_map[4]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(m, 4))
				{
					return NULL;
				}
			}

			*y += MAP_HEIGHT(m->tile_map[4]);
			*x -= MAP_WIDTH(m);
			return get_map_from_coord(m->tile_map[4], x, y);
		}

		/* Southeast */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[5])
			{
				return NULL;
			}

			if (!m->tile_map[5] || m->tile_map[5]->in_memory != MAP_IN_MEMORY)
			{
				if (!load_and_link_tiled_map(m, 5))
				{
					return NULL;
				}
			}

			*y -= MAP_HEIGHT(m);
			*x -= MAP_WIDTH(m);
			return get_map_from_coord(m->tile_map[5], x, y);
		}

		/* East */
		if (!m->tile_path[1])
		{
			return NULL;
		}

		if (!m->tile_map[1] || m->tile_map[1]->in_memory != MAP_IN_MEMORY)
		{
			if (!load_and_link_tiled_map(m, 1))
			{
				return NULL;
			}
		}

		*x -= MAP_WIDTH(m);
		return get_map_from_coord(m->tile_map[1], x, y);
	}

	/* Because we have tested x above, we don't need to check for
	 * Northwest, Southwest, Northeast and Northwest here again. */
	if (*y < 0)
	{
		if (!m->tile_path[0])
		{
			return NULL;
		}

		if (!m->tile_map[0] || m->tile_map[0]->in_memory != MAP_IN_MEMORY)
		{
			if (!load_and_link_tiled_map(m, 0))
			{
				return NULL;
			}
		}

		*y += MAP_HEIGHT(m->tile_map[0]);
		return get_map_from_coord(m->tile_map[0], x, y);
	}

	if (*y >= MAP_HEIGHT(m))
	{
		if (!m->tile_path[2])
		{
			return NULL;
		}

		if (!m->tile_map[2] || m->tile_map[2]->in_memory != MAP_IN_MEMORY)
		{
			if (!load_and_link_tiled_map(m, 2))
			{
				return NULL;
			}
		}

		*y -= MAP_HEIGHT(m);
		return get_map_from_coord(m->tile_map[2], x, y);
	}

	return NULL;
}

/**
 * Same as get_map_from_coord(), but this version doesn't load tiled maps
 * into memory, if they are not already.
 * @param m Map to consider.
 * @param[out] x Will contain the real X position that was checked. If
 * coordinates are not in a map this is set to 0, or to -1 if there is a
 * tiled map but it's not loaded.
 * @param[out] y Will contain the real Y position that was checked.
 * @return Map that is at specified location. Will be NULL if not on any
 * map. */
mapstruct *get_map_from_coord2(mapstruct *m, int *x, int *y)
{
	if (!m)
	{
		*x = 0;
		return NULL;
	}

	/* Simple case - coordinates are within this local map. */
	if (*x >= 0 && *x < MAP_WIDTH(m) && *y >= 0 && *y < MAP_HEIGHT(m))
	{
		return m;
	}

	/* West, Northwest or Southwest (3, 7 or 6) */
	if (*x < 0)
	{
		/* Northwest */
		if (*y < 0)
		{
			if (!m->tile_path[7])
			{
				*x = 0;
				return NULL;
			}

			if (!m->tile_map[7] || m->tile_map[7]->in_memory != MAP_IN_MEMORY)
			{
				*x = -1;
				return NULL;
			}

			*y += MAP_HEIGHT(m->tile_map[7]);
			*x += MAP_WIDTH(m->tile_map[7]);

			return get_map_from_coord2(m->tile_map[7], x, y);
		}

		/* Southwest */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[6])
			{
				*x = 0;
				return NULL;
			}

			if (!m->tile_map[6] || m->tile_map[6]->in_memory != MAP_IN_MEMORY)
			{
				*x = -1;
				return NULL;
			}

			*y -= MAP_HEIGHT(m);
			*x += MAP_WIDTH(m->tile_map[6]);

			return get_map_from_coord2(m->tile_map[6], x, y);
		}

		/* West */
		if (!m->tile_path[3])
		{
			*x = 0;
			return NULL;
		}

		if (!m->tile_map[3] || m->tile_map[3]->in_memory != MAP_IN_MEMORY)
		{
			*x = -1;
			return NULL;
		}

		*x += MAP_WIDTH(m->tile_map[3]);
		return get_map_from_coord2(m->tile_map[3], x, y);
	}

	/* East, Northeast or Southeast (1, 4 or 5) */
	if (*x >= MAP_WIDTH(m))
	{
		/* Northeast */
		if (*y < 0)
		{
			if (!m->tile_path[4])
			{
				*x = 0;
				return NULL;
			}

			if (!m->tile_map[4] || m->tile_map[4]->in_memory != MAP_IN_MEMORY)
			{
				*x = -1;
				return NULL;
			}

			*y += MAP_HEIGHT(m->tile_map[4]);
			*x -= MAP_WIDTH(m);

			return get_map_from_coord2(m->tile_map[4], x, y);
		}

		/* Southeast */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[5])
			{
				*x = 0;
				return NULL;
			}

			if (!m->tile_map[5] || m->tile_map[5]->in_memory != MAP_IN_MEMORY)
			{
				*x = -1;
				return NULL;
			}

			*y -= MAP_HEIGHT(m);
			*x -= MAP_WIDTH(m);

			return get_map_from_coord2(m->tile_map[5], x, y);
		}

		/* East */
		if (!m->tile_path[1])
		{
			*x = 0;
			return NULL;
		}

		if (!m->tile_map[1] || m->tile_map[1]->in_memory != MAP_IN_MEMORY)
		{
			*x = -1;
			return NULL;
		}

		*x -= MAP_WIDTH(m);
		return get_map_from_coord2(m->tile_map[1], x, y);
	}

	/* Because we have tested x above, we don't need to check for
	 * Northwest, Southwest, Northeast and Northwest here again. */
	if (*y < 0)
	{
		if (!m->tile_path[0])
		{
			*x = 0;
			return NULL;
		}

		if (!m->tile_map[0] || m->tile_map[0]->in_memory != MAP_IN_MEMORY)
		{
			*x = -1;
			return NULL;
		}

		*y += MAP_HEIGHT(m->tile_map[0]);

		return get_map_from_coord2(m->tile_map[0], x, y);
	}

	if (*y >= MAP_HEIGHT(m))
	{
		if (!m->tile_path[2])
		{
			*x = 0;
			return NULL;
		}

		if (!m->tile_map[2] || m->tile_map[2]->in_memory != MAP_IN_MEMORY)
		{
			*x = -1;
			return NULL;
		}

		*y -= MAP_HEIGHT(m);
		return get_map_from_coord2(m->tile_map[2], x, y);
	}

	*x = 0;
	return NULL;
}

/**
 * This is used by get_player to determine where the other
 * creature is.  get_rangevector takes into account map tiling,
 * so you just can not look the the map coordinates and get the
 * right value.  distance_x/y are distance away, which
 * can be negativbe.  direction is the crossfire direction scheme
 * that the creature should head.  part is the part of the
 * monster that is closest.
 *
 * get_rangevector looks at op1 and op2, and fills in the
 * structure for op1 to get to op2.
 * We already trust that the caller has verified that the
 * two objects are at least on adjacent maps.  If not,
 * results are not likely to be what is desired.
 * if the objects are not on maps, results are also likely to
 * be unexpected
 *
 * Flags: 0x1 is don't translate for closest body part.
 *        0x2 is do recursive search on adjacent tiles.
 * + any flags accepted by get_rangevector_from_mapcoords() below.
 *
 * Returns TRUE if successful, or FALSE otherwise.
 * @todo Document. */
int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags)
{
	if (!get_rangevector_from_mapcoords(op1->map, op1->x, op1->y, op2->map, op2->x, op2->y, retval, flags | RV_NO_DISTANCE))
	{
		return 0;
	}

	retval->part = op1;

	/* If this is multipart, find the closest part now */
	if (!(flags & RV_IGNORE_MULTIPART) && op1->more)
	{
		object *tmp, *best = NULL;
		int best_distance = retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y, tmpi;

		/* we just tkae the offset of the piece to head to figure
		 * distance instead of doing all that work above again
		 * since the distance fields we set above are positive in the
		 * same axis as is used for multipart objects, the simply arithemetic
		 * below works. */
		for (tmp = op1->more; tmp; tmp = tmp->more)
		{
			tmpi = (retval->distance_x - tmp->arch->clone.x) * (retval->distance_x - tmp->arch->clone.x) + (retval->distance_y - tmp->arch->clone.y) * (retval->distance_y - tmp->arch->clone.y);

			if (tmpi < best_distance)
			{
				best_distance = tmpi;
				best = tmp;
			}
		}

		if (best)
		{
			retval->distance_x -= best->arch->clone.x;
			retval->distance_y -= best->arch->clone.y;
			retval->part = best;
		}
	}

	retval->distance = isqrt(retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y);
	retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);

	return 1;
}

/**
 * This is the base for get_rangevector above, but can more generally compute the
 * rangvector between any two points on any maps.
 *
 * The part field of the rangevector is always set to NULL by this function.
 * (Since we don't actually know about any objects)
 *
 * If the function fails (because of the maps being separate), it will return FALSE and
 * the vector is not otherwise touched. Otherwise it will return TRUE.
 *
 * Calculates manhattan distance (dx+dy) per default. (fast)
 * - Flags:
 *   0x4 - calculate euclidian (straight line) distance (slow)
 *   0x8 - calculate diagonal  (max(dx + dy)) distance   (fast)
 *   0x8|0x04 - don't calculate distance (or direction) (fastest)
 * @todo Document. */
int get_rangevector_from_mapcoords(mapstruct *map1, int x1, int y1, mapstruct *map2, int x2, int y2, rv_vector *retval, int flags)
{
	retval->part = NULL;

	if (map1 == map2)
	{
		retval->distance_x = x2 - x1;
		retval->distance_y = y2 - y1;
	}
	else if (map1->tile_map[0] == map2)
	{
		retval->distance_x = x2 - x1;
		retval->distance_y = -(y1 + (MAP_HEIGHT(map2) - y2));
	}
	else if (map1->tile_map[1] == map2)
	{
		retval->distance_y = y2 - y1;
		retval->distance_x = (MAP_WIDTH(map1) - x1) + x2;
	}
	else if (map1->tile_map[2] == map2)
	{
		retval->distance_x = x2 - x1;
		retval->distance_y = (MAP_HEIGHT(map1) - y1) + y2;
	}
	else if (map1->tile_map[3] == map2)
	{
		retval->distance_y = y2 - y1;
		retval->distance_x = -(x1 + (MAP_WIDTH(map2) - x2));
	}
	else if (map1->tile_map[4] == map2)
	{
		retval->distance_y = -(y1 + (MAP_HEIGHT(map2)- y2));
		retval->distance_x = (MAP_WIDTH(map1) - x1) + x2;
	}
	else if (map1->tile_map[5] == map2)
	{
		retval->distance_x = (MAP_WIDTH(map1) - x1) + x2;
		retval->distance_y = (MAP_HEIGHT(map1) - y1) + y2;
	}
	else if (map1->tile_map[6] == map2)
	{
		retval->distance_y = (MAP_HEIGHT(map1) - y1) + y2;
		retval->distance_x = -(x1 + (MAP_WIDTH(map2) - x2));
	}
	else if (map1->tile_map[7] == map2)
	{
		retval->distance_x = -(x1 + (MAP_WIDTH(map2) - x2));
		retval->distance_y = -(y1 + (MAP_HEIGHT(map2) - y2));
	}
	else if (flags & RV_RECURSIVE_SEARCH)
	{
		retval->distance_x = x2;
		retval->distance_y = y2;

		if (!relative_tile_position(map1, map2, &retval->distance_x, &retval->distance_y))
		{
			return 0;
		}

		retval->distance_x -= x1;
		retval->distance_y -= y1;
	}
	else
	{
		return 0;
	}

	switch (flags & (0x04 | 0x08))
	{
		case RV_MANHATTAN_DISTANCE:
			retval->distance =  abs(retval->distance_x) + abs(retval->distance_y);
			break;

		case RV_EUCLIDIAN_DISTANCE:
			retval->distance = isqrt(retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y);
			break;

		case RV_DIAGONAL_DISTANCE:
			retval->distance = MAX(abs(retval->distance_x), abs(retval->distance_y));
			break;

		/* No distance calc */
		case RV_NO_DISTANCE:
			return 1;
	}

	retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);
	return 1;
}

/**
 * Checks whether two objects are on the same map, taking map tiling
 * into account.
 * @param op1 First object to check for.
 * @param op2 Second object to check against the first.
 * @return 1 if both objects are on same map, 0 otherwise. */
int on_same_map(object *op1, object *op2)
{
	if (!op1->map || !op2->map)
	{
		return 0;
	}

	if (op1->map == op2->map || op1->map->tile_map[0] == op2->map || op1->map->tile_map[1] == op2->map || op1->map->tile_map[2] == op2->map || op1->map->tile_map[3] == op2->map || op1->map->tile_map[4] == op2->map || op1->map->tile_map[5] == op2->map || op1->map->tile_map[6] == op2->map || op1->map->tile_map[7] == op2->map)
	{
		return 1;
	}

	return 0;
}

/**
 * Count the players on a map, using the local map player list.
 * @param m The map.
 * @return Number of players on this map. */
int players_on_map(mapstruct *m)
{
	object *tmp;
	int count;

	for (count = 0, tmp = m->player_first; tmp; tmp = CONTR(tmp)->map_above)
	{
		count++;
	}

	return count;
}

/**
 * Returns true if square x, y has P_NO_PASS set, which is true for walls
 * and doors but not monsters.
 * @param m Map to check for
 * @param x X coordinate to check for
 * @param y Y coordinate to check for
 * @return Non zero if blocked, 0 otherwise. */
int wall_blocked(mapstruct *m, int x, int y)
{
	int r;

	if (!(m = get_map_from_coord(m, &x, &y)))
	{
		return 1;
	}

	r = GET_MAP_FLAGS(m, x, y) & (P_NO_PASS | P_PASS_THRU);

	return r;
}

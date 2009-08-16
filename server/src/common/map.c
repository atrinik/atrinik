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

#include <global.h>
#include <funcpoint.h>

#include <loader.h>
/* ---win32 exclude header */
#ifndef WIN32
#include <unistd.h>
#endif

int	global_darkness_table[MAX_DARKNESS + 1] =
{
	0, 20, 40, 80, 160, 320, 640, 1280
};

/* our global map_tag value for the server */
uint32 global_map_tag;

/* to get the reverse direction for all 8 tiled map index */
int map_tiled_reverse[TILED_MAPS] =
{
	2, 3, 0, 1, 6, 7, 4, 5
};

extern int nrofallocobjects,nroffreeobjects;

#define DEBUG_OLDFLAGS 1

#if 0
/* If 0 this block because I don't know if it is still needed.
 * if it is, it really should be done via autoconf now days
 * and not by specific machine checks.
 */

#if defined(sgi)
/* popen_local is defined in porting.c */
#define popen popen_local
#endif

#if defined (MACH) || defined (NeXT) || defined (__MACH__)
#ifndef S_ISGID
#define S_ISGID 0002000
#endif
#ifndef S_IWOTH
#define S_IWOTH 0000200
#endif
#ifndef S_IWGRP
#define S_IWGRP 0000020
#endif
#ifndef S_IWUSR
#define S_IWUSR 0000002
#endif
#ifndef S_IROTH
#define S_IROTH 0000400
#endif
#ifndef S_IRGRP
#define S_IRGRP 0000040
#endif
#ifndef S_IRUSR
#define S_IRUSR 0000004
#endif
#endif
#if defined(MACH) || defined(vax) || defined(ibm032) || defined(NeXT) || defined(__MACH__)
#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif
#endif
#endif


/* this updates the orig_map->tile_map[tile_num] value after loading
 * the map.  It also takes care of linking back the freshly loaded
 * maps tile_map values if it tiles back to this one.  It returns
 * the value of orig_map->tile_map[tile_num].  It really only does this
 * so that it is easier for calling functions to verify success. */
static mapstruct *load_and_link_tiled_map(mapstruct *orig_map, int tile_num)
{
	int dest_tile = map_tiled_reverse[tile_num];

	orig_map->tile_map[tile_num] = ready_map_name(orig_map->tile_path[tile_num], MAP_NAME_SHARED | (MAP_UNIQUE(orig_map) ? 1 : 0));

	if (orig_map->tile_map[tile_num]->tile_path[dest_tile])
	{
		/* no need for strcmp as we now use shared strings */
		if (orig_map->tile_map[tile_num]->tile_path[dest_tile] == orig_map->path)
			orig_map->tile_map[tile_num]->tile_map[dest_tile] = orig_map;
	}
	else
		LOG(llevBug, "BUG: load_and_link_tiled_map(): map %s (%d) points to map %s but has no relink\n", orig_map->path, tile_num, orig_map->tile_map[tile_num]->path);

	return orig_map->tile_map[tile_num];
}

/* The recursive part of the function below. */
static int relative_tile_position_rec(mapstruct *map1, mapstruct *map2, int *x, int *y, uint32 id)
{
	int i;

	if (map1 == map2)
		return TRUE;

	map1->traversed = id;

	/* TODO: A bidirectional breadth-first search would be more efficient */
	/* Depth-first search for the destination map */
	for (i = 0; i < TILED_MAPS; i++)
	{
		if (map1->tile_path[i])
		{
			if (!map1->tile_map[i] || map1->tile_map[i]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(map1, i);

			if (map1->tile_map[i]->traversed != id && ((map1->tile_map[i] == map2) || relative_tile_position_rec(map1->tile_map[i], map2, x, y, id)))
			{
				switch (i)
				{
						/* North */
					case 0:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						return TRUE;

						/* East */
					case 1:
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* South */
					case 2:
						*y += MAP_HEIGHT(map1);
						return TRUE;

						/* West */
					case 3:
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;

						/* Northest */
					case 4:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* Southest */
					case 5:
						*y += MAP_HEIGHT(map1);
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* Southwest */
					case 6:
						*y += MAP_HEIGHT(map1);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;

						/* Northwest */
					case 7:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;
				}
			}
		}
	}

	return FALSE;
}

/* Find the distance between two map tiles on a tiled map.
 * Returns true if the two tiles are part of the same map.
 * the distance from the topleft (0,0) corner of map1 to the topleft corner of map2
 * will be added to x and y.
 *
 * This function does not work well with assymetrically tiled maps.
 * It will also (naturally) perform bad on very large tilesets such as the world map
 * as it may need to load all tiles into memory before finding a path between two tiles.
 * We probably want to handle the world map as a special case, considering that
 * all tiles are of equal size, and that we might be able to parse their coordinates from
 * their names... */
static int relative_tile_position(mapstruct *map1, mapstruct *map2, int *x, int *y)
{
	int i;
	static uint32 traversal_id = 0;

	/* Save some time in the simplest cases ( very similar to on_same_map() )*/
	if (map1 == NULL || map2 == NULL)
		return FALSE;

	if (map1 == map2)
		return TRUE;

	for (i = 0; i < TILED_MAPS; i++)
	{
		if (map1->tile_path[i])
		{
			if (!map1->tile_map[i] || map1->tile_map[i]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(map1, i);

			if (map1->tile_map[i] == map2)
			{
				switch (i)
				{
						/* North */
					case 0:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						return TRUE;

						/* East */
					case 1:
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* South */
					case 2:
						*y += MAP_HEIGHT(map1);
						return TRUE;

						/* West */
					case 3:
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;

						/* Northest */
					case 4:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* Southest */
					case 5:
						*y += MAP_HEIGHT(map1);
						*x += MAP_WIDTH(map1);
						return TRUE;

						/* Southwest */
					case 6:
						*y += MAP_HEIGHT(map1);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;

						/* Northwest */
					case 7:
						*y -= MAP_HEIGHT(map1->tile_map[i]);
						*x -= MAP_WIDTH(map1->tile_map[i]);
						return TRUE;
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
			m->traversed = 0;

		traversal_id = 0;
	}

	/* recursive search */
	return relative_tile_position_rec(map1, map2, x, y, ++traversal_id);
}

/* Returns the mapstruct which has a name matching the given argument.
 * return NULL if no match is found. This version _requires_ a shared string as input. */
mapstruct *has_been_loaded_sh(const char *name)
{
	mapstruct *map;
	int namebug = 0;

	if (!name || !*name)
		return 0;

	/* this IS a bug starting without '/' - this can lead in double loaded maps! */
	if (*name != '/' && *name != '.')
	{
		/* Can't handle offset using shared strings... */
		char namebuf[HUGE_BUF];

		namebug = 1;

		namebuf[0] = '/';
		namebuf[1] = '\0';
		strcat(namebuf, name);
		name = add_string(namebuf);

		LOG(llevDebug, "DEBUG: has_been_loaded_sh: found map name without starting '/': fixed! %s\n", STRING_SAFE(name));
	}

	for (map = first_map; map; map = map->next)
	{
		/*LOG(-1, "check map: >%s< find: >%s<\n", name, map->path);*/
		if (name == map->path)
			break;
	}

	if (namebug)
		free_string_shared(name);

	return map;
}

/* This makes a path absolute outside the world of Crossfire.
 * In other words, it prepends LIBDIR/MAPDIR/ to the given path
 * and returns the pointer to a static array containing the result.
 * it really should be called create_mapname */
char *create_pathname(const char *name)
{
	static char buf[MAX_BUF];

	/* Why?  having extra / doesn't confuse unix anyplace?  Dependancies
	 * someplace else in the code? msw 2-17-97 */
	if (*name == '/')
		sprintf (buf, "%s%s", settings.mapdir, name);
	else
		sprintf (buf, "%s/%s", settings.mapdir, name);

	return (buf);
}

/* This makes absolute path to the itemfile where unique objects
 * will be saved. Converts '/' to '@'. I think it's essier maintain
 * files than full directory structure, but if this is problem it can
 * be changed. */
static char *create_items_path (const char *s)
{
	static char buf[MAX_BUF];
	char *t;

	if (*s == '/')
		s++;

	sprintf(buf, "%s/%s/", settings.localdir, settings.uniquedir);

	for (t = buf+strlen(buf); *s; s++, t++)
	{
		if (*s == '/')
			*t = '@';
		else
			*t = *s;
	}

	*t = 0;
	return buf;
}


/* This function checks if a file with the given path exists.
 * -1 is returned if it fails, otherwise the mode of the file
 * is returned.
 * It tries out all the compression suffixes listed in the uncomp[] array.
 *
 * If prepend_dir is set, then we call create_pathname (which prepends
 * libdir & mapdir).  Otherwise, we assume the name given is fully
 * complete.
 * Only the editor actually cares about the writablity of this -
 * the rest of the code only cares that the file is readable.
 * when the editor goes away, the call to stat should probably be
 * replaced by an access instead (similar to the windows one, but
 * that seems to be missing the prepend_dir processing */
int check_path(const char *name, int prepend_dir)
{
#ifdef WIN32
	/* ***win32: check this sucker in windows style. */
	char buf[MAX_BUF];

	if (prepend_dir)
		strcpy(buf, create_pathname(name));
	else
		strcpy(buf, name);

	return _access(buf, 0);
#else
	char buf[MAX_BUF], *endbuf;
	struct stat statbuf;
	int mode = 0, i;

	if (prepend_dir)
		strcpy(buf, create_pathname(name));
	else
		strcpy(buf, name);

	/* old method (strchr(buf, '\0')) seemd very odd to me -
	 * this method should be equivalant and is clearer.
	 * Can not use strcat because we need to cycle through
	 * all the names. */
	endbuf = buf + strlen(buf);

	for (i = 0; i < NROF_COMPRESS_METHODS; i++)
	{
		if (uncomp[i][0])
			strcpy(endbuf, uncomp[i][0]);
		else
			*endbuf = '\0';

		if (!stat(buf, &statbuf))
			break;
	}

	if (i == NROF_COMPRESS_METHODS)
		return -1;

	if (!S_ISREG(statbuf.st_mode))
		return -1;

	if (((statbuf.st_mode & S_IRGRP) && getegid() == statbuf.st_gid) || ((statbuf.st_mode & S_IRUSR) && geteuid() == statbuf.st_uid) || (statbuf.st_mode & S_IROTH))
		mode |= 4;

	if ((statbuf.st_mode & S_IWGRP && getegid() == statbuf.st_gid) || (statbuf.st_mode & S_IWUSR && geteuid() == statbuf.st_uid) || (statbuf.st_mode & S_IWOTH))
		mode |= 2;

	return mode;
#endif
}

/* Moved from main.c */
char *normalize_path(const char *src, const char *dst, char *path)
{
	char *p, *q;
	char buf[HUGE_BUF];
	/*    static char path[HUGE_BUF]; */

	/*LOG(llevDebug,"path before normalization >%s<>%s<\n", src, dst);*/

	if (*dst == '/')
	{
		strcpy(buf, dst);
	}
	else
	{
		strcpy (buf, src);

		if ((p = strrchr(buf, '/')))
			p[1] = '\0';
		else
			strcpy(buf, "/");

		strcat(buf, dst);
	}

	q = p = buf;
	while ((q = strstr(q, "//")))
		p = ++q;

	*path = '\0';
	q = path;
	p = strtok(p, "/");
	while (p)
	{
		if (!strcmp(p, ".."))
		{
			q = strrchr(path, '/');
			if (q)
				*q = '\0';
			else
			{
				*path = '\0';
				LOG(llevBug, "BUG: Illegal path.\n");
			}
		}
		else
		{
			strcat(path, "/");
			strcat(path, p);
		}

		p = strtok(NULL, "/");
	}

	/*LOG(llevDebug,"path after normalization >%s<\n", path);*/

	return path;
}

/* Prints out debug-information about a map.
 * Dumping these at llevError doesn't seem right, but is
 * necessary to make sure the information is in fact logged. */
void dump_map(mapstruct *m)
{
	LOG(llevSystem, "Map %s status: %d.\n", m->path, m->in_memory);
	LOG(llevSystem, "Size: %dx%d Start: %d,%d\n", MAP_WIDTH(m), MAP_HEIGHT(m), MAP_ENTER_X(m), MAP_ENTER_Y(m));

	if (m->msg != NULL)
		LOG(llevSystem, "Message:\n%s", m->msg);

	if (m->tmpname != NULL)
		LOG(llevSystem, "Tmpname: %s\n", m->tmpname);

	LOG(llevSystem, "Difficulty: %d\n", m->difficulty);
	LOG(llevSystem, "Darkness: %d\n", m->darkness);
	LOG(llevSystem, "Light: %d\n", m->light_value);
	LOG(llevSystem, "Outdoor: %d\n", MAP_OUTDOORS(m));
}

/* Prints out debug-information about all maps.
 * This basically just goes through all the maps and calls
 * dump_map on each one. */
void dump_all_maps()
{
	mapstruct *m;

	for (m = first_map; m != NULL; m = m->next)
	{
		dump_map(m);
	}
}

/* Returns true if a wall is present in a given location.
 * Calling object should do a <return>&P_PASS_THRU if it
 * has CAN_PASS_THRU to see it can cross here.
 * The PLAYER_ONLY flag here is analyzed without checking the
 * caller type. Thats possible because player movement releated
 * functions should always use blocked(). */
int wall(mapstruct *m, int x, int y)
{
	if (!(m = out_of_map(m, &x, &y)))
		return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);

	return (GET_MAP_FLAGS(m, x, y) & (P_DOOR_CLOSED | P_PLAYER_ONLY | P_NO_PASS | P_PASS_THRU));
}

/* Returns true if it's impossible to see through the given coordinate
 * in the given map. */
int blocks_view(mapstruct *m, int x, int y)
{
	mapstruct *nm;

	if (!(nm = out_of_map(m, &x, &y)))
		return (P_BLOCKSVIEW | P_NO_PASS | P_OUT_OF_MAP);

	return (GET_MAP_FLAGS(nm, x, y) & P_BLOCKSVIEW);
}

/* Returns true if the given coordinate in the given map blocks magic. */
int blocks_magic(mapstruct *m, int x, int y)
{
	if (!(m = out_of_map(m, &x, &y)))
		return (P_BLOCKSVIEW | P_NO_PASS | P_NO_MAGIC | P_OUT_OF_MAP);

	return (GET_MAP_FLAGS(m, x, y) & P_NO_MAGIC);
}

/* Returns true if clerical spells cannot work here */
int blocks_cleric(mapstruct *m, int x, int y)
{
	if (!(m = out_of_map(m, &x, &y)))
		return (P_BLOCKSVIEW | P_NO_PASS | P_NO_CLERIC | P_OUT_OF_MAP);

	return (GET_MAP_FLAGS(m, x, y) & P_NO_CLERIC);
}

/* I total reworked the blocked functions. There was several bugs, glitches
 * and loops in. The loops really scaled with bigger load very badly, slowing
 * this part down for heavy traffic.
 * Changes: check ALL P_xxx flags (and really all) of a tile node here. If its impossible
 * to enter the tile - blocked() will tell it us.
 * This included to capsule and integrate blocked_tile() in blocked().
 * blocked_tile() is the function where the single objects of a node gets
 * tested - for example for CHECK_INV. But i added a P_CHECK_INV flag - so its
 * now only called when really needed - before it was called for EVERY moving
 * object for every successful step.
 * PASS_THRU check is moved in blocked() too.. This should generate for example for
 * pathfinding better results. Note, that PASS_THRU only has a meaning when NO_PASS
 * is set. If a object has both flags, NO_PASS can be passed when object has
 * CAN_PASS_THRU. If object has PASS_THRU without NO_PASS, PASS_THRU is ignored.
 * blocked() checks player vs player stuff too. No block in non pvp areas.
 * Note, that blocked() is only on the first glance bigger as before - i moved stuff
 * in which was in blocked_tile() or handled from calling functions around the call -
 * so its less or same code but moved in blocked().
 *
 * Return: 0 = can be passed , elsewhere it gives one or more flags which invoke
 * the block AND/OR which was not tested. (for outside check).
 * MT-2003 */

/* i added the door flag now. The trick is, that we want mark the door as possible
 * to open here and sometimes not. If the object spot is in forbidden terrain, we
 * don't want its possible to open it, even we stand near to it. But for example if
 * it blocked by alive object, we want open it. If the spot marked as pass_thru and
 * we can pass_thru, then we want skip the door (means not open it).
 * MT-29.01.2004 */
int blocked(object *op, mapstruct *m, int x, int y, int terrain)
{
	int flags;
	MapSpace *msp;

	flags = (msp = GET_MAP_SPACE_PTR(m, x, y))->flags;

	/* lets start... first, look at the terrain. If we don't have
	 * a valid terrain flag, this is forbidden to enter. */
	if (msp->move_flags & ~terrain)
		return ((flags & (P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU)) | P_NO_TERRAIN);

	/* the terrain is ok... whats first?
	 * A.) P_IS_ALIVE - we leave without question
	 * (NOTE: player objects has NO is_alive set!)
	 * B.) P_NO_PASS - if set we leave here when no PASS_THRU is set
	 * and/or the passer has no CAN_PASS_THRU. */
	if (flags & P_IS_ALIVE)
		return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_ALIVE | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));

	/* still one flag to check: perhaps P_PASS_THRU overrules NO_PASS? */
	/* i seperated it from below - perhaps we add here more tests */
	if (flags & P_NO_PASS)
	{
		/* logic is: no_pass when..
		 * - no PASS_THRU... or
		 * - PASS_THRU set but op==NULL (no PASS_THRU check possible)
		 * - PASS_THRU set and object has no CAN_PASS_THRU */
		if (!(flags & P_PASS_THRU) || !op || !QUERY_FLAG(op, FLAG_CAN_PASS_THRU))
			return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_IS_PLAYER | P_CHECK_INV | P_PASS_THRU));

		/* ok, NO_PASS is overruled... we go on... */
	}

	/* now.... whats left? No explicit flag can forbid us to enter anymore  - except:
	 * a.) perhaps is a player in and we are a monster or the player is in a pvp area.
	 * b.) we need to check_inv - which can kick us out too (checker power) */
	if (flags & P_IS_PLAYER)
	{
		/* ok... we leave here when
		 * a.) op == NULL (because we can't check for op==PLAYER then)
		 * b.) P_IS_PVP or MAP_FLAG_PVP */
		if (!op || flags & P_IS_PVP || m->map_flags & MAP_FLAG_PVP)
			return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));

		/* when we are here: no player pvp stuff was triggered. But:
		 * a.) the tile IS blocked by a player (we still in IS_PLAYER area)
		 * b.) we are not in any pvp area
		 * c.) we have a op pointer to check.
		 *
		 * we can handle here more exclusive stuff now... Like we can pass spells
		 * through player by checking owner or something... Just insert it here. */

		/* for now, the easiest way - if op is no player (it is a monster or somewhat
		 * else "solid" object) - then no pass */
		if (op->type != PLAYER)
			return (flags & (P_DOOR_CLOSED | P_IS_PLAYER | P_CHECK_INV));
	}

	/* we have a object ptr - do some last checks */
	if (op)
	{
		/* player only space and not a player tell them: no pass and possible checker here */
		if ((flags & P_PLAYER_ONLY) && op->type != PLAYER)
			return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV));

		/* and here is our CHECK_INV ...
		 * blocked_tile() is now only and exclusive called from here.
		 * lets skip it, when op is NULL - so we can turn the check from outside
		 * on/off (for example if we only want test size stuff) */
		if (flags & P_CHECK_INV)
		{
			/* we fake a NO_PASS when the checker kick us out - in fact thats
			 * how it should be. tell them: no pass and checker here */
			if (blocked_tile(op, m, x, y))
				return (flags & (P_DOOR_CLOSED | P_NO_PASS | P_CHECK_INV));
		}
	}

	/* ah... 0 is what we want.... 0 == we can pass */
	return (flags & (P_DOOR_CLOSED));
}


/* Returns true if the given coordinate is blocked by the
 * object passed is not blocking.  This is used with
 * multipart monsters - if we want to see if a 2x2 monster
 * can move 1 space to the left, we don't want its own area
 * to block it from moving there.
 * Returns TRUE if the space is blocked by something other than the
 * monster. */
/* why is controlling the own arch clone offsets with the new
 * freearr_[] offset a good thing?
 * a.) we don't must check any flags for tiles where we was before
 * b.) we don't block in moving when we got teleported in a no_pass somewhere
 * c.) no call to out_of_map() needed for all parts
 * d.) no checks of objects in every tile node of the multi arch
 * e.) no recursive call needed anymore
 * f.) the multi arch are handled in maps like the single arch
 * g.) no scaling by heavy map action when we move (more objects
 *     on the map don't interest us anymore here) */
int blocked_link(object *op, int xoff, int yoff)
{
	object *tmp, *tmp2;
	mapstruct *m;
	int xtemp, ytemp;

	for (tmp = op; tmp; tmp = tmp->more)
	{
		/* we search for this new position */
		xtemp = tmp->arch->clone.x + xoff;
		ytemp = tmp->arch->clone.y + yoff;

		/* lets check it match a different part of us */
		for (tmp2 = op; tmp2; tmp2 = tmp2->more)
		{
			/* if this is true, we can be sure this position is valid */
			if (xtemp == tmp2->arch->clone.x && ytemp == tmp2->arch->clone.y)
				break;
		}

		/* if this is NULL, tmp will move in a new node */
		if (!tmp2)
		{
			xtemp = tmp->x + xoff;
			ytemp = tmp->y + yoff;

			/* if this new node is illegal - we can skip all */
			if (!(m = out_of_map(tmp->map, &xtemp, &ytemp)))
				return -1;

			/* tricky: we use always head for tests - no need to copy any flags to the tail */
			/* we should kick in here the door test - but we need to diff we are
			 * just testing here or we doing a real step! */
			if ((xtemp = blocked(op, m, xtemp, ytemp, op->terrain_flag)))
				return xtemp;
		}
	}

	/* when we are here - then we can move */
	return 0;
}

/* As above, but using an absolute coordinate (map, x, y) - triplet
 * TODO: this function should really be combined with the above
 * to reduce code duplication... */
int blocked_link_2(object *op, mapstruct *map, int x, int y)
{
	object *tmp, *tmp2;
	int xtemp, ytemp;
	mapstruct *m;

	for (tmp = op; tmp; tmp = tmp->more)
	{
		/* we search for this new position */
		xtemp = x + tmp->arch->clone.x;
		ytemp = y + tmp->arch->clone.y;
		/* lets check it match a different part of us */
		for (tmp2 = op; tmp2; tmp2 = tmp2->more)
		{
			/* if this is true, we can be sure this position is valid */
			if (xtemp == tmp2->x && ytemp == tmp2->y)
				break;
		}

		/* if this is NULL, tmp will move in a new node */
		if (!tmp2)
		{
			/* if this new node is illegal - we can skip all */
			if (!(m = out_of_map(map, &xtemp, &ytemp)))
				return -1;

			/* tricky: we use always head for tests - no need to copy any flags to the tail */
			if ((xtemp = blocked(op, m, xtemp, ytemp, op->terrain_flag)))
				return xtemp;
		}
	}

	/* when we are here - then we can move */
	return 0;
}


/* blocked_tile()
 * return: 0= not blocked 1: blocked
 * This is used for any action which needs to browse
 * through the objects of the tile node - for special objects
 * like inventory checkers - or general for all what can't
 * be easy handled by map flags in blocked(). */
int blocked_tile(object *op, mapstruct *m, int x, int y)
{
	object *tmp;

	for (tmp = GET_MAP_OB(m, x, y); tmp != NULL; tmp = tmp->above)
	{
		/* This must be before the checks below.  Code for inventory checkers. */
		/* Note: we only do this check here because the last_grace cause the
		 * CHECK_INV to block the space. The check_inv is called again in
		 * move_apply() - there it will do the trigger and so on. This here is only
		 * for testing the tile - not for invoking the check_inv power! */
		if (tmp->type == CHECK_INV && tmp->last_grace)
		{
			/* If last_sp is set, the player/monster needs an object,
			 * so we check for it.  If they don't have it, they can't
			 * pass through this space. */
			if (tmp->last_sp)
			{
				if (check_inv_recursive(op, tmp) == NULL)
					return 1;

				continue;
			}
			else
			{
				/* In this case, the player must not have the object -
				 * if they do, they can't pass through. */
				if (check_inv_recursive(op, tmp) != NULL)
					return 1;

				continue;
			}
		}
	}

	return 0;
}

/* Testing a arch to fit in a position.
 * Return: 0 == no block.-1 == out of map, else the blocking flags from blocked() */

/* Advanced arch_blocked() function. We CAN give a object ptr too know. If we do,
 * we can test the right terrain flags AND all specials from blocked(). This is
 * extremly useful for pathfinding. */
int arch_blocked(archetype *at, object *op, mapstruct *m, int x, int y)
{
	archetype *tmp;
	mapstruct *mt;
	int xt, yt, t;

	if (op)
		t = op->terrain_flag;
	else
		t = TERRAIN_ALL;

	if (at == NULL)
	{
		if (!(m = out_of_map(m, &x, &y)))
			return -1;

		return blocked(op, m, x, y, t);
	}

	for (tmp = at; tmp; tmp = tmp->more)
	{
		xt = x + tmp->clone.x;
		yt = y + tmp->clone.y;

		if (!(mt = out_of_map(m, &xt, &yt)))
			return -1;

		/* double used xt... small hack */
		if ((xt = blocked(op, mt, xt, yt, t)))
			return xt;
	}

	return 0;
}

/* Returns true if the given archetype can't fit into the map at the
 * given spot (some part of it is outside the map-boundaries). */
int arch_out_of_map(archetype *at, mapstruct *m, int x, int y)
{
	archetype *tmp;
	int xt, yt;

	if (at == NULL)
		return out_of_map(m, &x, &y) == NULL ? 1 : 0;

	for (tmp = at; tmp != NULL; tmp = tmp->more)
	{
		xt = x + tmp->clone.x;
		yt = y + tmp->clone.y;

		if (!out_of_map(m, &xt, &yt))
			return 1;
	}

	return 0;
}

/* Loads (ands parses) the objects into a given map from the specified
 * file pointer.
 * mapflags is the same as we get with load_original_map */

/* i optimized this function now - i remove ALOT senseless stuff,
 * processing the load & expanding of objects here in one loop.
 * MT - 05.02.2004 */

/* now, this function is the very deep core of the whole server map &
 * object handling. To understand the tiled map handling, you have to
 * understand the flow of this function. It can now called recursive
 * and it will call themself recursive if the insert_object_in_map()
 * function below has found a multi arch reaching in a different map.
 * Then the out_of_map() call in insert_object_in_map() will trigger a
 * new map load and a recursive call of this function. This will work
 * without any problems. Note the restore_light_source_list() call at
 * the end of the list. Adding overlapping light sources for tiled map
 * in this recursive structure was the hard part but it will work now
 * without problems. MT-25.02.2004 */
void load_objects(mapstruct *m, FILE *fp, int mapflags)
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
		/* atm, we don't need and handle multi arches saved with tails! */
		if (i == LL_MORE)
		{
			LOG(llevDebug, "BUG: load_objects(%s): object %s - its a tail!\n", m->path ? m->path : ">no map<", query_short_name(op, NULL));
			continue;
		}

		/* if the archetype for the object is null, means that we
		 * got an invalid object.  Don't do anythign with it - the game
		 * or editor will not be able to do anything with it either. */
		if (op->arch == NULL)
		{
			LOG(llevDebug, "BUG:load_objects(%s): object %s (%d)- invalid archetype. (pos:%d,%d)\n", m->path ? m->path : ">no map<", query_short_name(op, NULL), op->type, op->x, op->y);
			continue;
		}

		/* do some safety for containers */
		if (op->type == CONTAINER)
		{
			/* used for containers as link to players viewing it */
			op->attacked_by = NULL;
			op->attacked_by_count = 0;
			sum_weight(op);
		}

		if (op->type == MONSTER)
			fix_monster(op);

		/* important pre set for the animation/face of a object */
		if (QUERY_FLAG(op, FLAG_IS_TURNABLE) || QUERY_FLAG(op, FLAG_ANIMATE))
			SET_ANIMATION(op, (NUM_ANIMATIONS(op) / NUM_FACINGS(op)) * op->direction + op->state);

		/* expand a multi arch - we have only the head saved in a map!
		 * the *real* fancy point is, that our head/tail don't must fit
		 * in this map! insert_ob will take about it and load the needed
		 * map - then this function and the map loader is called recursive! */

		/* we have a multi arch head? */
		if (op->arch->more)
		{
			/* a important note: we have sometimes the head of a multi arch
			 * object in the inventory of objects - for example mobs
			 * which changed type in spawn points and in the mob itself
			 * as TYPE_BASE_INFO. As long as this arches are not on the map,
			 * we will not come in trouble here because load_object() will them
			 * load on the fly. That means too, that multi arches in inventories
			 * are always NOT expanded - means no tail. For inventory objects,
			 * we never hit this breakpoint here. */
			tail = op->arch->more;
			prev = op,last_more = op;

			/* then clone the tail using the default arch */
			do
			{
				tmp = get_object();
				copy_object(&tail->clone, tmp);

				tmp->x += op->x;
				tmp->y += op->y;
				tmp->map = op->map;

				/* adjust the single object specific data except flags. */
				tmp->type = op->type;
				tmp->layer = op->layer;

				/* link the tail object... */
				tmp->head = prev, last_more->more = tmp, last_more = tmp;

			}
			while ((tail = tail->more));

			/* now some tricky stuff again:
			 * to speed up some core functions like moving or remove_ob()/insert_ob
			 * and because there are some "arch depending and not object depending"
			 * flags, we init the tails with some of the head settings. */

			if (QUERY_FLAG(op, FLAG_SYS_OBJECT))
				SET_MULTI_FLAG(op->more, FLAG_SYS_OBJECT)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_SYS_OBJECT);

			if (QUERY_FLAG(op, FLAG_NO_APPLY))
				SET_MULTI_FLAG(op->more, FLAG_NO_APPLY)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_NO_APPLY);

			if (QUERY_FLAG(op, FLAG_IS_INVISIBLE))
				SET_MULTI_FLAG(op->more, FLAG_IS_INVISIBLE)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_IS_INVISIBLE);

			if (QUERY_FLAG(op, FLAG_IS_ETHEREAL))
				SET_MULTI_FLAG(op->more, FLAG_IS_ETHEREAL)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_IS_ETHEREAL);

			if (QUERY_FLAG(op, FLAG_CAN_PASS_THRU))
				SET_MULTI_FLAG(op->more, FLAG_CAN_PASS_THRU)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_CAN_PASS_THRU);

			if (QUERY_FLAG(op, FLAG_FLYING))
				SET_MULTI_FLAG(op->more, FLAG_FLYING)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_FLYING);

			if (QUERY_FLAG(op, FLAG_BLOCKSVIEW))
				SET_MULTI_FLAG(op->more, FLAG_BLOCKSVIEW)
				else
					CLEAR_MULTI_FLAG(tmp->more, FLAG_BLOCKSVIEW);
		}

		insert_ob_in_map(op, m, op, INS_NO_MERGE | INS_NO_WALK_ON);

		/* this is from fix_auto_apply() which is removed now */

		/* auto_apply() will remove the flag_auto_apply after first use */
		if (QUERY_FLAG(op, FLAG_AUTO_APPLY))
			auto_apply(op);
		/* for fresh maps, create treasures */
		else if ((mapflags & MAP_ORIGINAL) && op->randomitems)
			create_treasure(op->randomitems, op, op->type != TREASURE ? GT_APPLY : 0, op->level ? op->level : m->difficulty, T_STYLE_UNSET, ART_CHANCE_UNSET, 0, NULL);

#if 0
		/* iam not sure this is senseful.
		 * it was part of fix_auto_apply() but it should
		 * redundant */
		else if (op->type == TIMED_GATE)
		{
			op->speed = 0;
			update_ob_speed(op);
		}
#endif

		op = get_object();
		op->map = m;
	}

	delete_loader_buffer(mybuffer);

	/* this MUST be set here or check_light_source_list()
	 * will fail. If we set this too early, the recursive called map
	 * will add a light source to the caller and then the caller itself
	 * here again... */
	m->in_memory = MAP_IN_MEMORY;

	/* this is the only place we can insert this because the
	 * recursive nature of load_objects(). */
	check_light_source_list(m);
}

/* This saves all the objects on the map in a (most times) non destructive fashion.
 * Except spawn point/mobs and multi arches - see below.
 * Modified by MSW 2001-07-01 to do in a single pass - reduces code,
 * and we only save the head of multi part objects - this is needed
 * in order to do map tiling properly.
 * The function/engine is now multi arch/tiled map save - put on the
 * map what you like. MT-07.02.04 */
void save_objects(mapstruct *m, FILE *fp, FILE *fp2)
{
	int i, j = 0;
	object *head, *op, *otmp, *tmp, *last_valid;

	/* first, we have to remove all dynamic objects from this map.
	 * from spell effects with owners (because the owner can't
	 * be restored after a save) or from spawn points generated mobs.
	 * We need to remove them in a right way - perhaps our spawn mob
	 * was sitting on a button and we need to save then the unpressed
	 * button - when not removed right our map get messed up when reloaded.
	 *
	 * We need to be a bit careful here.
	 *
	 * We will give the move_apply() code (which handles object changes when
	 * something is removed) the MOVE_APPLY_VANISHED flag - we MUST
	 * take care in all called function about it.
	 *
	 * a example: A button which is pressed will call a spawn point "remove object x"
	 * and unpressed "spawn onject x".
	 * This is ok in the way, the button only set a flag/value in the spawn point
	 * so in the next game tick the spawn point can do action. Because we will save
	 * now, that action will be called when the map is reloaded. All ok.
	 * NOT ok is, that the button then (or any other from move apply called object)
	 * does a action IMMIDIALTY except it is a static effect (like we put a wall
	 * in somewhere).
	 * Absolut forbidden are dynamic effect like instant spawns of mobs on other maps
	 * point - then our map will get messed up again. Teleporters are a bit critical here
	 * and i fear the code and callings in move_apply() will need some more carefully
	 * examination. */
	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			for (op = get_map_ob (m, i, j); op; op = otmp)
			{
				otmp = op->above;
				/* thats NULL OR a valid ptr - it CAN'T be a non valid
				 * or we had remove it before AND reseted the ptr then right. */
				last_valid = op->below;

				/* ok, we will *never* save maps with player on */
				if (op->type == PLAYER)
				{
					LOG(llevDebug, "SemiBUG: Tried to save map with player on!(%s (%s))\n", query_name(op, NULL), m->path);
					continue;
				}

				head = op->head ? op->head : op;

				/* here we remove all "dynamic" content which are around on the map.
				 * ATM i remove some spell effects with it.
				 * For things like permanent counterspell walls or something we should
				 * use and create special objects. */
				if (QUERY_FLAG(head, FLAG_NO_SAVE))
				{
					remove_ob(head);
					check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

					/* invalid next ptr! */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
							otmp = op->above;
						else if (last_valid)
							otmp = last_valid->above;
						/* should be really rare */
						else
							otmp = get_map_ob(m, i, j);
					}

					continue;
				}

				/* here we handle the mobs of a spawn point - called spawn mobs.
				 * We *never* save spawn mobs - not even if they are on the same map.
				 * We remove them and tell the spawn point to generate them new in the next tick.
				 * (In case of the saved map it is the reloading).
				 * If reloaded, the spawn point will restore a new mob of same kind on
				 * the default position. */
				else if (QUERY_FLAG(op, FLAG_SPAWN_MOB))
				{
					/* browse the inv for the map spawn info */
					for (tmp = head->inv; tmp; tmp = tmp->below)
					{
						if (tmp->type == SPAWN_POINT_INFO)
						{
							/* thats the spawn point... */
							if (tmp->owner && tmp->owner->type == SPAWN_POINT)
							{
								/* tell the source spawn point to respawn this deleted object.
								 * It can be here OR on a different map. */
								/* force a pre spawn setting */
								tmp->owner->stats.sp = tmp->owner->last_sp;
								/* we force active spawn point */
								tmp->owner->speed_left += 1.0f;
								tmp->owner->enemy = NULL;
							}
							else
								LOG(llevBug, "BUG: Spawn mob (%s (%s)) has SPAWN INFO without or illegal owner set (%s)!\n", op->arch->name, query_name(head, NULL), query_name(tmp->owner, NULL));

							remove_ob(head);
							check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

							/* sometimes goto's are VERY useful */
							goto save_objects_jump1;
						}
					}

					LOG(llevBug, "BUG: Spawn mob (%s %s) without SPAWN INFO.\n", head->arch->name, query_name(head, NULL));
					remove_ob(head);
					check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

					if (!OBJECT_FREE(tmp) && tmp->owner && tmp->owner->type == SPAWN_POINT)
						tmp->owner->enemy = NULL;

save_objects_jump1:

					/* invalid next ptr! */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
							otmp = op->above;
						else if (last_valid)
							otmp = last_valid->above;
						/* should be really rare */
						else
							otmp = get_map_ob(m, i, j);
					}

					continue;
				}
				else if (op->type == SPAWN_POINT)
				{
					/* Handling of the spawn points is much easier than handling the mob.
					 * if the spawn point still controls some mob, we delete the mob  - where ever
					 * it is. Also, set pre spawn value to last mob - so we restore our creature
					 * when we reload this map. */
					if (op->enemy)
					{
						/* we have a legal spawn? */
						if (op->enemy_count == op->enemy->count && !QUERY_FLAG(op->enemy, FLAG_REMOVED) && !OBJECT_FREE(op->enemy))
						{
							/* force a pre spawn setting */
							op->stats.sp = op->last_sp;
							op->speed_left += 1.0f;
							/* and delete the spawn */
							remove_ob(op->enemy);
							check_walk_off (op->enemy, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);
							op->enemy = NULL;

							/* invalid next ptr! */
							if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
							{
								if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
									otmp = op->above;
								else if (last_valid)
									otmp = last_valid->above;
								/* should be really rare */
								else
									otmp = get_map_ob(m, i, j);
							}
						}
					}
				}

				/* we will delete here all temporary owner objects.
				 * We talk here about spell effects, pets, golems and
				 * other "dynamic" objects.
				 * What NOT should be deleted are throw objects and other
				 * permanent items which has a owner setting! (if they have) */
				if (head->owner)
				{
					/* perhaps we should add here a flag for pets...
					 * But the pet code needs a rework so or so.
					 * ATM we simply delete GOLEMS and clearing
					 * from all other spells/stuff the owner tags.
					 * SPAWN MOBS are not here so we only speak about
					 * spell effects
					 * we *can* delete them here too - but then i would
					 * prefer a no_save flag. Only reason to save them is
					 * to reset for example buttons or avoiding side effects
					 * like a fireball saved with neutral owner which does then
					 * something evil - but that CAN always catched in the code
					 * and scripts so lets go the easy way here - as less we
					 * manipulate the map here as more secure we are! */

					/* a golem needs a valid release from the player... */
					if (head->type == GOLEM)
					{
						(*send_golem_control_func)(head, GOLEM_CTR_RELEASE);
						remove_friendly_object(head);
						remove_ob(head);
						check_walk_off(head, NULL, MOVE_APPLY_VANISHED | MOVE_APPLY_SAVING);

						/* invalid next ptr! */
						if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
						{
							if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
								otmp = op->above;
							else if (last_valid)
								otmp = last_valid->above;
							/* should be really rare */
							else
								otmp = get_map_ob(m, i, j);
						}
						continue;
					}

					LOG(llevDebug, "WARNING (only debug): save_obj(): obj w. owner. map:%s obj:%s (%s) (%d,%d)\n", m->path, query_name(op, NULL), op->arch->name ? op->arch->name : "<no arch name>", op->x, op->y);
					head->owner = NULL;
					continue;
				}
			}
		}
	}

	/* The map is now cleared from non static objects on this or other maps
	 * (when the source was from this map). Now all can be saved as a legal
	 * snapshot of the map state mashine.
	 * That means all button/mashine relations are correct. */
	for (i = 0; i < MAP_WIDTH(m); i++)
	{
		for (j = 0; j < MAP_HEIGHT(m); j++)
		{
			for (op = get_map_ob (m, i, j); op; op = otmp)
			{
				otmp = op->above;
				/* thats NULL OR a valid ptr - it CAN'T be a non valid
				 * or we had remove it before AND reseted the ptr then right. */
				last_valid = op->below;

				/* do some testing... */
				/* ok, we will *never* save maps with player on */
				if (op->type == PLAYER)
					continue;

				/* here we do the magic! */

				/* its a tail... */
				if (op->head)
				{
					int xt, yt;

					/* the magic is, that we have a tail here, but we
					 * save the head now and give it the x/y
					 * position basing on this tail position and its
					 * x/y clone arch default multi tile offsets!
					 * With this trick, we even can generate negative
					 * map positions - and thats exactly what we want
					 * when our head is on a different map as this tail!
					 * insert_ob() and the map loader will readjust map and
					 * positions and load the other map when needed!
					 * we must save x/y or remove_ob() will fail. */
					tmp = op->head;
					xt = tmp->x;
					yt = tmp->y;
					tmp->x = op->x - op->arch->clone.x;
					tmp->y = op->y - op->arch->clone.y;

					if (QUERY_FLAG(tmp, FLAG_UNIQUE))
						save_map_object(fp2 , tmp, 3);
					else
						save_map_object(fp, tmp, 3);

					tmp->x = xt;
					tmp->y = yt;
					/* this is only a "trick" remove - no walk off check */
					remove_ob(tmp);

					/* invalid next ptr! */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						/* remember: if we have remove for example 2 or more objects above, the
						 * op->above WILL be still valid - remove_ob() will handle it right.
						 * IF we get here a valid ptr, ->above WILL be valid too. Always. */
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
							otmp = op->above;
						else if (last_valid)
							otmp = last_valid->above;
						/* should be really rare */
						else
							otmp = get_map_ob(m, i, j);
					}

					continue;
				}

				if (QUERY_FLAG(op, FLAG_UNIQUE))
					save_map_object(fp2, op, 3);
				else
					save_map_object(fp, op, 3);

				/* its a head (because we had tails tested before) */
				if (op->more)
				{
					/* only a "trick" remove - no move_apply() changes or something */
					remove_ob(op);

					/* invalid next ptr! */
					if (otmp && (QUERY_FLAG(otmp, FLAG_REMOVED) || OBJECT_FREE(otmp)))
					{
						if (!QUERY_FLAG(op, FLAG_REMOVED) && !OBJECT_FREE(op))
							otmp = op->above;
						else if (last_valid)
							otmp = last_valid->above;
						/* should be really rare */
						else
							otmp = get_map_ob(m, i, j);
					}
				}
			}
		}
	}
}

/* Allocates, initialises, and returns a pointer to a mapstruct.
 * Modified to no longer take a path option which was not being
 * used anyways.  MSW 2001-07-01 */
mapstruct *get_linked_map()
{
	mapstruct *map = (mapstruct *) calloc(1, sizeof(mapstruct));
	mapstruct *mp;

	if (map == NULL)
		LOG(llevError, "ERROR: get_linked_map(): OOM.\n");

	for (mp = first_map; mp != NULL && mp->next != NULL; mp = mp->next);

	if (mp == NULL)
		first_map = map;
	else
		mp->next = map;

	map->buttons = NULL;
	map->first_light = NULL;
	map->bitmap = NULL;
	map->in_memory = MAP_SWAPPED;

	/* The maps used to pick up default x and y values from the
	 * map archetype.  Mimic that behaviour. */
	MAP_WIDTH(map) = 16;
	MAP_HEIGHT(map) = 16;
	MAP_RESET_TIMEOUT(map) = 7200;
	MAP_TIMEOUT(map) = 300;
	/* default light of a map is full daylight */
	MAP_DARKNESS(map) = -1;
	map->light_value = global_darkness_table[MAX_DARKNESS];

	MAP_ENTER_X(map) = 0;
	MAP_ENTER_Y(map) = 0;

	return map;
}

/* Allocates the arrays contained in a mapstruct.
 * This basically allocates the dynamic array of spaces for the
 * map. */
void allocate_map(mapstruct *m)
{
#if 0
	/* These are obnoxious - presumably the caller of this function knows what it is
	 * doing.  Instead of checking for load status, lets check instead to see
	 * if the data has already been allocated. */
	if (m->in_memory != MAP_SWAPPED)
		return;
#endif

	m->in_memory = MAP_LOADING;
	/* Log this condition and free the storage.  We could I suppose
	 * realloc, but if the caller is presuming the data will be intact,
	 * that is their poor assumption. */
	if (m->spaces || m->bitmap)
	{
		LOG(llevError, "ERROR: allocate_map callled with already allocated map (%s)\n", m->path);

		if (m->spaces)
			free(m->spaces);

		if (m->bitmap)
			free(m->bitmap);
	}

	if (m->buttons)
		LOG(llevBug, "Bug: allocate_map callled with allready set buttons (%s)\n", m->path);

	m->spaces = calloc(1, MAP_WIDTH(m) * MAP_HEIGHT(m) * sizeof(MapSpace));

	m->bitmap = malloc(((MAP_WIDTH(m) + 31) / 32) * MAP_HEIGHT(m) * sizeof(uint32));

	if (m->spaces == NULL || m->bitmap == NULL)
		LOG(llevError, "ERROR: allocate_map(): OOM.\n");
}

/* Creatures and returns a map of the specific size.  Used
 * in random map code and the editor. */
mapstruct *get_empty_map(int sizex, int sizey)
{
	mapstruct *m = get_linked_map();
	m->width = sizex;
	m->height = sizey;
	m->in_memory = MAP_SWAPPED;
	allocate_map(m);
	return m;
}

/* This loads the header information of the map.  The header
 * contains things like difficulty, size, timeout, etc.
 * this used to be stored in the map object, but with the
 * addition of tiling, fields beyond that easily named in an
 * object structure were needed, so it just made sense to
 * put all the stuff in the map object so that names actually make
 * sense.
 * This could be done in lex (like the object loader), but I think
 * currently, there are few enough fields this is not a big deal.
 * MSW 2001-07-01
 * return 0 on success, 1 on failure. */
static int load_map_header(FILE *fp, mapstruct *m)
{
	char buf[HUGE_BUF], msgbuf[HUGE_BUF], *key = buf, *value, *end;
	int msgpos = 0;

	while (fgets(buf, HUGE_BUF - 1, fp) != NULL)
	{
		buf[HUGE_BUF - 1] = 0;
		key = buf;

		while (isspace(*key))
			key++;

		/* empty line */
		if (*key == 0)
			continue;

		value = strchr(key, ' ');
		if (!value)
		{
			end = strchr(key, '\n');
			*end = 0;
		}
		else
		{
			*value = 0;
			value++;
			while (isspace(*value))
				value++;

			end = strchr(value, '\n');
		}

		/* key is the field name, value is what it should be set
		 * to.  We've already done the work to null terminate key,
		 * and strip off any leading spaces for both of these.
		 * We have not touched the newline at the end of the line -
		 * these are needed for some values.  the end pointer
		 * points to the first of the newlines.
		 * value could be NULL!  It would be easy enough to just point
		 * this to "" to prevent cores, but that would let more errors slide
		 * through. */

		if (!strcmp(key, "arch"))
		{
			/* This is an oddity, but not something we care about much. */
			if (strcmp(value, "map\n"))
				LOG(llevError, "ERROR: loading map and got a non 'arch map' line(%s %s)?\n", key, value);
		}
		else if (!strcmp(key, "name"))
		{
			*end = 0;
			m->name = strdup_local(value);
		}
		else if (!strcmp(key, "msg"))
		{
			while (fgets(buf, HUGE_BUF - 1, fp) != NULL)
			{
				if (!strcmp(buf, "endmsg\n"))
					break;
				else
				{
					/* slightly more efficient than strcat */
					strcpy(msgbuf + msgpos, buf);
					msgpos += strlen(buf);
				}
			}

			/* There are lots of maps that have empty messages (eg, msg/endmsg
			 * with nothing between).  There is no reason in those cases to
			 * keep the empty message.  Also, msgbuf contains garbage data
			 * when msgpos is zero, so copying it results in crashes */
			if (msgpos != 0)
				m->msg = strdup_local(msgbuf);
		}
		else if (!strcmp(key, "bg_music"))
		{
			*end = 0;
			m->bg_music = strdup_local(value);
		}
		else if (!strcmp(key, "enter_x"))
		{
			m->enter_x = atoi(value);
		}
		else if (!strcmp(key, "enter_y"))
		{
			m->enter_y = atoi(value);
		}
		else if (!strcmp(key, "width"))
		{
			m->width = atoi(value);
		}
		else if (!strcmp(key, "height"))
		{
			m->height = atoi(value);
		}
		else if (!strcmp(key, "reset_timeout"))
		{
			m->reset_timeout = atoi(value);
		}
		else if (!strcmp(key, "swap_time"))
		{
			m->timeout = atoi(value);
		}
		else if (!strcmp(key, "difficulty"))
		{
			m->difficulty = atoi(value);
		}
		else if (!strcmp(key, "darkness"))
		{
			MAP_DARKNESS(m) = atoi(value);
			if (MAP_DARKNESS(m) == -1)
				m->light_value = global_darkness_table[MAX_DARKNESS];
			else
			{
				if (MAP_DARKNESS(m) < 0 || MAP_DARKNESS(m) > MAX_DARKNESS)
				{
					LOG(llevBug, "\nBug: Illegal map darkness %d, setting to %d\n", MAP_DARKNESS(m), MAX_DARKNESS);
					MAP_DARKNESS(m) = MAX_DARKNESS;
				}

				m->light_value = global_darkness_table[MAP_DARKNESS(m)];
			}
		}
		else if (!strcmp(key, "light"))
		{
			m->light_value = atoi(value);
		}
		/* i assume that the default map_flags settings is 0 - so we don't handle <flagset> 0 */
		else if (!strcmp(key, "no_save"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_NO_SAVE;
		}
		else if (!strcmp(key, "no_magic"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_NOMAGIC;
		}
		else if (!strcmp(key, "no_priest"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_NOPRIEST;
		}
		else if (!strcmp(key, "no_harm"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_NOHARM;
		}
		else if (!strcmp(key, "no_summon"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_NOSUMMON;
		}
		else if (!strcmp(key, "fixed_login"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_FIXED_LOGIN;
		}
		else if (!strcmp(key, "perm_death"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_PERMDEATH;
		}
		else if (!strcmp(key, "ultra_death"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_ULTRADEATH;
		}
		else if (!strcmp(key, "ultimate_death"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_ULTIMATEDEATH;
		}
		else if (!strcmp(key, "pvp"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_PVP;
		}
		else if (!strcmp(key, "outdoor"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_OUTDOOR;
		}
		else if (!strcmp(key, "map_tag"))
		{
			m->map_tag =  (uint32) atoi(value);
		}
		else if (!strcmp(key, "unique"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_UNIQUE;
		}
		else if (!strcmp(key, "fixed_resettime"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_FIXED_RTIME;
		}
		else if (!strcmp(key, "plugins"))
		{
			if (atoi(value))
				m->map_flags |= MAP_FLAG_PLUGINS;
		}
		else if (!strncmp(key, "tile_path_", 10))
		{
			int tile = atoi(key + 10);

			if (tile < 1 || tile > TILED_MAPS)
			{
				LOG(llevError,"ERROR: load_map_header: tile location %d out of bounds (%s)\n", tile, m->path);
			}
			else
			{
				*end = 0;
				if (m->tile_path[tile - 1])
				{
					LOG(llevError, "ERROR: load_map_header: tile location %d duplicated (%s <-> %s)\n", tile, m->path, m->tile_path[tile - 1]);
					FREE_AND_CLEAR_HASH(m->tile_path[tile - 1]);
				}

				/* If path not absoulute, try to normalize it */
				if (check_path(value, 1) == -1)
				{
					normalize_path(m->path, value, msgbuf);
					if (check_path(msgbuf, 1) == -1)
					{
						LOG(llevBug,"BUG: get_map_header: Can not normalize tile path %s %s %s\n", m->path, value, msgbuf);
						value = NULL;
					}
					else
						value = msgbuf;
				}

				/* We have a correct path to a neighbour tile */
				if (value)
				{
					mapstruct *neighbour;
					int dest_tile = map_tiled_reverse[tile - 1];
					const char *path_sh = add_string(value);

					m->tile_path[tile - 1] = path_sh;

					/* If the neighbouring map tile has been loaded, set up the map pointers */
					if ((neighbour = has_been_loaded_sh(path_sh)))
					{
#if 0
						LOG(llevDebug, "add t_map %s (%d). ", value, tile - 1);
#endif
						m->tile_map[tile - 1] = neighbour;
						/* Replaced strcmp with ptr check since its a shared string now */
						if (neighbour->tile_path[dest_tile] == NULL || neighbour->tile_path[dest_tile] == m->path)
							neighbour->tile_map[dest_tile] = m;
#if 0
						else
							LOG(llevDebug, "NO? t_map %s (%d). ", value, tile - 1);
#endif
					}
#if 0
					else
					{
						LOG(llevDebug, "skip t_map %s (%d). ", value, tile - 1);
					}
#endif
				}
			}
		}
		else if (!strcmp(key, "end"))
			break;
		else
		{
			LOG(llevBug, "BUG: Got unknown value in map header: %s %s\n", key, value);
		}
	}

	if (strcmp(key, "end"))
	{
		LOG(llevBug, "BUG: Got premature eof on map header!\n");
		return 1;
	}

	return 0;
}

/* Opens the file "filename" and reads information about the map
 * from the given file, and stores it in a newly allocated
 * mapstruct.  A pointer to this structure is returned, or NULL on failure.
 * flags correspond to those in map.h.  Main ones used are
 * MAP_PLAYER_UNIQUE, in which case we don't do any name changes, and
 * MAP_BLOCK, in which case we block on this load.  This happens in all
 *   cases, no matter if this flag is set or not.
 * MAP_STYLE: style map - don't add active objects, don't add to server
 *		managed map list. */
mapstruct *load_original_map(const char *filename, int flags)
{
	FILE *fp;
	mapstruct *m;
	int comp;
	char pathname[MAX_BUF];
	char tmp_fname[MAX_BUF];

	/* this IS a bug - because the missing '/' strcpy will fail when it
	 * search the loaded maps - this can lead in a double load and break
	 * the server!
	 * '.' sign unique maps in fixed directories. */
	if (*filename != '/' &&  *filename != '.')
	{
		LOG(llevDebug, "DEBUG: load_original_map: filename without start '/' - overruled. %s\n", filename);
		tmp_fname[0] = '/';
		strcpy(tmp_fname + 1, filename);
		filename = tmp_fname;
	}

	/* be sure we have always a unique map_tag */
	global_map_tag++;
	if (flags & MAP_PLAYER_UNIQUE)
	{
		LOG(llevDebug, "load_original_map unique: %s (%x)\n", filename,flags);
		strcpy(pathname, filename);
	}
	else
	{
		LOG(llevDebug, "load_original_map: %s (%x) ", filename,flags);
		strcpy(pathname, create_pathname(filename));
	}

	if ((fp = open_and_uncompress(pathname, 0, &comp)) == NULL)
	{
		if (!(flags & MAP_PLAYER_UNIQUE))
			LOG(llevBug, "BUG: Can't open map file %s\n", pathname);

		return NULL;
	}

	LOG(llevDebug, "link map. ");
	m = get_linked_map();

	LOG(llevDebug, "header: ");
	FREE_AND_COPY_HASH(m->path, filename);
	/* pre init the map tag */
	m->map_tag = global_map_tag;

	if (load_map_header(fp, m))
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
		LOG(llevBug, "\nBUG: Map %s has difficulty 0. Changing to 1 (non special item area).\n", filename);
		MAP_DIFFICULTY(m) = 1;
	}

#if 0
	MAP_DIFFICULTY(m) = calculate_difficulty(m);
#endif
	set_map_reset_time(m);
	LOG(llevDebug, "done!\n");
	return m;
}

/* Loads a map, which has been loaded earlier, from file.
 * Return the map object we load into (this can change from the passed
 * option if we can't find the original map) */
static mapstruct *load_temporary_map(mapstruct *m)
{
	FILE *fp;
	int comp;
	char buf[MAX_BUF];

	if (!m->tmpname)
	{
		LOG(llevBug, "BUG: No temporary filename for map %s! fallback to original!\n", m->path);
		strcpy(buf, m->path);
		delete_map(m);
		m = load_original_map(buf, 0);

		if (m == NULL)
			return NULL;

		return m;
	}

	LOG(llevDebug, "load_temporary_map: %s (%s) ", m->tmpname, m->path);

	if ((fp = open_and_uncompress(m->tmpname, 0, &comp)) == NULL)
	{
		LOG(llevBug, "BUG: Can't open temporary map %s! fallback to original!\n", m->tmpname);
		strcpy(buf, m->path);
		delete_map(m);
		m = load_original_map(buf, 0);

		if (m == NULL)
			return NULL;

		return m;
	}

	LOG(llevDebug, "header: ");

	if (load_map_header(fp, m))
	{
		LOG(llevBug, "BUG: Error loading map header for %s (%s)! fallback to original!\n", m->path, m->tmpname);
		delete_map(m);
		m = load_original_map(m->path, 0);

		if (m == NULL)
			return NULL;

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

/******************************************************************************
 * This is the start of unique map handling code
 *****************************************************************************/

/* This goes through map 'm' and removed any unique items on the map. */
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
					unique = 1;

				if (op->head == NULL && (QUERY_FLAG(op, FLAG_UNIQUE) || unique))
				{
					if (QUERY_FLAG(op, FLAG_IS_LINKED))
						remove_button_link(op);

					remove_ob(op);
					/* check off should be right here ... */
					check_walk_off(op, NULL, MOVE_APPLY_VANISHED);
				}
			}
		}
	}
}

/* Loads unique objects from file(s) into the map which is in memory
 * m is the map to load unique items into. */
static void load_unique_objects(mapstruct *m)
{
	FILE *fp;
	int comp,count;
	char firstname[MAX_BUF];

	for (count = 0; count < 10; count++)
	{
		sprintf(firstname, "%s.v%02d", create_items_path(m->path), count);
		if (!access(firstname, R_OK))
			break;
	}

	/* If we get here, we did not find any map */
	if (count == 10)
		return;

	LOG(llevDebug, "open unique items file for %s\n", create_items_path(m->path));
	if ((fp = open_and_uncompress(firstname, 0, &comp)) == NULL)
	{
		/* There is no expectation that every map will have unique items, but this
		 * is debug output, so leave it in. */
		LOG(llevDebug, "Can't open unique items file for %s\n", create_items_path(m->path));
		return;
	}

	m->in_memory = MAP_LOADING;

	/* if we have loaded unique items from */
	if (m->tmpname == NULL)
		delete_unique_items(m);

	load_objects(m, fp, 0);
	close_and_delete(fp, comp);
}


/* Saves a map to file.  If flag is set, it is saved into the same
 * file it was (originally) loaded from.  Otherwise a temporary
 * filename will be genarated, and the file will be stored there.
 * The temporary filename will be stored in the mapstructure.
 * If the map is unique, we also save to the filename in the map
 * (this should have been updated when first loaded) */
int new_save_map(mapstruct *m, int flag)
{
	FILE *fp, *fp2;
	char filename[MAX_BUF], buf[MAX_BUF];
	int i;

	if (flag && !*m->path)
	{
		LOG(llevBug, "BUG: Tried to save map without path.\n");
		return -1;
	}

	if (flag || MAP_UNIQUE(m))
	{
		if (!MAP_UNIQUE(m))
			strcpy(filename, create_pathname(m->path));
		else
		{
			/* that ensures we always reload from original maps */
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
			strcat(filename, uncomp[m->compressed][0]);

		make_path_to_file(filename);
	}
	else
	{
		if (m->tmpname == NULL)
			m->tmpname = tempnam_local(settings.tmpdir, NULL);

		strcpy(filename, m->tmpname);
	}

	LOG(llevDebug, "Saving map %s to %s\n", m->path, filename);

	m->in_memory = MAP_SAVING;

	/* Compress if it isn't a temporary save.  Do compress if unique */
	if (m->compressed && (MAP_UNIQUE(m) || flag))
	{
		char buf[MAX_BUF];
		strcpy(buf, uncomp[m->compressed][2]);
		strcat(buf, " > ");
		strcat(buf, filename);
		fp = popen(buf, "w");
	}
	else
		fp = fopen(filename, "w");

	if (fp == NULL)
	{
		LOG(llevError, "ERROR: Can't open file %s for saving.\n", filename);
		return -1;
	}

	/* legacy */
	fprintf(fp, "arch map\n");

	if (m->name)
		fprintf(fp, "name %s\n", m->name);

	if (!flag)
		fprintf(fp, "swap_time %d\n", m->swap_time);

	if (m->reset_timeout)
		fprintf(fp, "reset_timeout %d\n", m->reset_timeout);

	if (MAP_FIXED_RESETTIME(m))
		fprintf(fp, "fixed_resettime %d\n", MAP_FIXED_RESETTIME(m) ? 1 : 0);

	/* we unfortunately have no idea if this is a value the creator set
	 * or a difficulty value we generated when the map was first loaded */
	if (m->difficulty)
		fprintf(fp, "difficulty %d\n", m->difficulty);

	fprintf(fp, "darkness %d\n", m->darkness);
	fprintf(fp, "light %d\n", m->light_value);
	fprintf(fp, "map_tag %d\n", m->map_tag);

	if (m->width)
		fprintf(fp, "width %d\n", m->width);

	if (m->height)
		fprintf(fp, "height %d\n", m->height);

	if (m->enter_x)
		fprintf(fp, "enter_x %d\n", m->enter_x);

	if (m->enter_y)
		fprintf(fp, "enter_y %d\n", m->enter_y);

	if (m->msg)
		fprintf(fp, "msg\n%sendmsg\n", m->msg);

	if (MAP_UNIQUE(m))
		fprintf(fp, "unique %d\n", MAP_UNIQUE(m) ? 1 : 0);

	if (MAP_OUTDOORS(m))
		fprintf(fp, "outdoor %d\n", MAP_OUTDOORS(m) ? 1 : 0);

	if (MAP_NOSAVE(m))
		fprintf(fp, "no_save %d\n", MAP_NOSAVE(m) ? 1 : 0);

	if (MAP_NOMAGIC(m))
		fprintf(fp, "no_magic %d\n", MAP_NOMAGIC(m) ? 1 : 0);

	if (MAP_NOPRIEST(m))
		fprintf(fp, "no_priest %d\n", MAP_NOPRIEST(m) ? 1 : 0);

	if (MAP_NOHARM(m))
		fprintf(fp, "no_harm %d\n", MAP_NOHARM(m) ? 1 : 0);

	if (MAP_NOSUMMON(m))
		fprintf(fp, "no_summon %d\n", MAP_NOSUMMON(m) ? 1 : 0);

	if (MAP_FIXEDLOGIN(m))
		fprintf(fp, "fixed_login %d\n", MAP_FIXEDLOGIN(m) ? 1 : 0);

	if (MAP_PERMDEATH(m))
		fprintf(fp, "perm_death %d\n", MAP_PERMDEATH(m) ? 1 : 0);

	if (MAP_ULTRADEATH(m))
		fprintf(fp, "ultra_death %d\n", MAP_ULTRADEATH(m) ? 1 : 0);

	if (MAP_ULTIMATEDEATH(m))
		fprintf(fp, "ultimate_death %d\n", MAP_ULTIMATEDEATH(m) ? 1 : 0);

	if (MAP_PVP(m))
		fprintf(fp, "pvp %d\n", MAP_PVP(m) ? 1 : 0);

	/* Save any tiling information */
	for (i = 0; i < TILED_MAPS; i++)
	{
		if (m->tile_path[i])
			fprintf(fp, "tile_path_%d %s\n", i + 1, m->tile_path[i]);
	}

	fprintf(fp, "end\n");

	/* In the game save unique items in the different file, but
	 * in the editor save them to the normal map file.
	 * If unique map, save files in the proper destination (set by
	 * player) */

	/* save unique items into fp2 */
	fp2 = fp;
	if ((flag == 0 || flag == 2) && !MAP_UNIQUE(m))
	{
		sprintf(buf, "%s.v00", create_items_path(m->path));
		if ((fp2 = fopen(buf, "w")) == NULL)
			LOG(llevBug, "BUG: Can't open unique items file %s\n", buf);

		save_objects(m, fp, fp2);
		if (fp2 != NULL)
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
	/* save same file when not playing, like in editor */
	else
		save_objects(m, fp, fp);

	if (fp)
	{
		if (m->compressed && !flag)
			pclose(fp);
		else
			fclose(fp);
	}

	/* We save unique maps in the database. */
	if (MAP_UNIQUE(m))
	{
		sqlite3 *db;
		sqlite3_stmt *statement;
		char *sqlbuf, linebuf[MAX_BUF] = "", mapPath[MAX_BUF], *p = NULL, path[MAX_BUF];
		int update = 0, sqlresult, size = HUGE_BUF;

		/* Open the filename where we temporarily saved this map and grab all the lines. */
		fp = fopen(filename, "r");

		sqlbuf = (char *)malloc(size);

		sqlbuf[0] = '\0';

		/* Go through the lines */
		while (fgets(linebuf, MAX_BUF, fp))
		{
			/* If this would overflow, reallocate the buffer with more bytes */
			if (strlen(linebuf) + strlen(sqlbuf) > (unsigned int) size)
			{
				size += strlen(linebuf) + strlen(sqlbuf) + 1;

				if ((p = (char *)realloc(sqlbuf, size)) == NULL)
				{
					fclose(fp);
					unlink(filename);
					LOG(llevError, "ERROR: Out of memory.\n");
					return 0;
				}
				else
					sqlbuf = p;
			}

			strcat(sqlbuf, linebuf);
		}

		fclose(fp);

		sprintf(mapPath, "%s", m->path);

		/* Only want the filename. */
		p = strtok(mapPath, "/");
		while (p != NULL)
		{
			sprintf(path, "%s", p);
			p = strtok(NULL, "/");
		}

		/* Open the database */
		db_open(DB_DEFAULT, &db);

		if (!db_prepare_format(db, &statement, "SELECT mapPath FROM unique_maps WHERE mapPath = '%s';", path))
		{
			LOG(llevBug, "BUG: new_save_map(): SQL failed to prepare for selecting unique map! (%s)\n", db_errmsg(db));
			db_close(db);
			unlink(filename);
			return 0;
		}

		if (db_step(statement) == SQLITE_ROW)
			update = 1;

		db_finalize(statement);

		/* SQL to update the unique map. */
		if (update)
			sqlresult = db_prepare_format(db, &statement, "UPDATE unique_maps SET data = '%s' WHERE mapPath = '%s';", db_sanitize_input(sqlbuf), path);
		else
			sqlresult = db_prepare_format(db, &statement, "INSERT INTO unique_maps (mapPath, data) VALUES('%s', '%s');", path, db_sanitize_input(sqlbuf));

		/* Prepare the SQL */
		if (!sqlresult)
		{
			LOG(llevBug, "BUG: new_save_map(): SQL failed to prepare for updating/inserting unique map! (%s)\n", db_errmsg(db));
			db_close(db);
			unlink(filename);
			return 0;
		}

		/* Run the SQL */
		db_step(statement);

		/* Finalize it */
		db_finalize(statement);

		/* Close database */
		db_close(db);

		/* Remove the temporary file. */
		unlink(filename);

		/* Free the buf */
		if (sqlbuf)
			free(sqlbuf);

		return 0;
	}

	chmod(filename, SAVE_MODE);
	return 0;
}

/* Remove and free all objects in the given map. */
void free_all_objects(mapstruct *m)
{
	int i, j;
	object *op;

	/*LOG(llevDebug, "FAO-start: map:%s ->%d\n", m->name ? m->name : (m->tmpname ? m->tmpname : ""), m->in_memory);*/
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
				if (op->head != NULL)
					op = op->head;

				/* technical remove - no check off */
				remove_ob(op);
			}
		}
	}
	/*LOG(llevDebug, "FAO-end: map:%s ->%d\n", m->name ? m->name : (m->tmpname ? m->tmpname : ""), m->in_memory);*/
}

/* Frees everything allocated by the given mapstructure.
 * don't free tmpname - our caller is left to do that */
void free_map(mapstruct *m, int flag)
{
	int i;

	if (!m->in_memory)
	{
		LOG(llevBug, "BUG: Trying to free freed map.\n");
		return;
	}

	/* if we don't do this, we leave the light mask part
	 * on a possible tiled map and when we reload, the area
	 * will be set with wrong light values. */
	remove_light_source_list(m);

	/* I put this before free_all_objects() -
	 * because the link flag is now tested in destroy_object()
	 * to have a clean handling of temporary/dynamic buttons on
	 * a map. Because we delete now the FLAG_LINKED from the object
	 * in free_objectlinkpt() we don't trigger it inside destroy_object() */
	if (m->buttons)
		free_objectlinkpt(m->buttons);

	if (flag && m->spaces)
		free_all_objects(m);

	FREE_AND_NULL_PTR(m->name);
	FREE_AND_NULL_PTR(m->spaces);
	FREE_AND_NULL_PTR(m->msg);
	m->buttons = NULL;
	m->first_light = NULL;

	for (i = 0; i < TILED_MAPS; i++)
		FREE_AND_CLEAR_HASH(m->tile_path[i]);

	if (m->bitmap)
	{
		free(m->bitmap);
		m->bitmap = NULL;
	}

	m->in_memory = MAP_SWAPPED;
}

/* function: vanish mapstruct
 * m       : pointer to mapstruct, if NULL no action
 * this deletes all the data on the map (freeing pointers)
 * and then removes this map from the global linked list of maps. */
void delete_map(mapstruct *m)
{
	mapstruct *tmp, *last;
	int i;

	if (!m)
		return;

	if (m->in_memory == MAP_IN_MEMORY)
	{
		/* change to MAP_SAVING, even though we are not,
		  * so that remove_ob doesn't do as much work. */
		m->in_memory = MAP_SAVING;
		free_map(m, 1);
	}
	else
		remove_light_source_list(m);

	/* move this out of free_map, since tmpname can still be needed if
	 * the map is swapped out. */
	FREE_AND_NULL_PTR(m->tmpname);
	last = NULL;

	/* We need to look through all the maps and see if any maps
	 * are pointing at this one for tiling information.  Since
	 * tiling can be assymetric, we just can not look to see which
	 * maps this map tiles with and clears those. */
	/* iam somewhat sure asymetric maps will NOT work with the
	 * recursive map loading + map light source system/reloading.
	 * asymetric map gives the power to "stack" maps but we really
	 * need it? MT-2004 think about don't allow asymetric maps */
	for (tmp = first_map; tmp != NULL; tmp = tmp->next)
	{
		if (tmp->next == m)
			last = tmp;

		/* This should hopefully get unrolled on a decent compiler */
		for (i = 0; i < TILED_MAPS; i++)
			if (tmp->tile_map[i] == m)
				tmp->tile_map[i] = NULL;
	}

	/* If last is null, then this should be the first map in the list */
	if (!last)
	{
		if (m == first_map)
			first_map = m->next;
		else
			/* m->path is freed below, so should hopefully still have
			 * some useful data in it. */
			LOG(llevBug, "BUG: delete_map: Unable to find map %s in list\n", m->path);
	}
	else
		last->next = m->next;

	/* Free our pathname (we'd like to use it above)*/
	if (m->path)
	{
		FREE_AND_CLEAR_HASH(m->path);
	}

	free(m);
}

/**
 * Check the map owner. Called from functions like pickup,
 * drop, etc.
 * @param map The map to check
 * @param op Player object to check
 * @return 1 if we the player is the map owner, 0 otherwise */
int check_map_owner(mapstruct *map, object *op)
{
	/* Sanity checks */
	if (map == NULL || map->path == NULL)
	{
		LOG(llevBug, "BUG: check_map_owner(): Map provided is either null or has no valid path!\n");
		return 0;
	}

	/* Non unique maps or unique maps with no_save 1 are ok for everyone. */
	if (!map->owner || !MAP_UNIQUE(map) || MAP_NOSAVE(map))
	{
		return 1;
	}

	/* Same for DMs. */
	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		return 1;
	}

	/* Check the map owner */
	if (strcmp(MAP_OWNER(map), op->name) == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Create map owner for unique maps with not no_save 1. */
/**
 * Create map owner for unique maps
 * @param map
 * @return
 */
char *create_map_owner(mapstruct *map)
{
	char buf[MAX_BUF], name[MAX_BUF], *p;

	snprintf(buf, sizeof(buf), "%s", map->path);

	/* Sanity checks */
	if (map == NULL || map->path == NULL)
	{
		LOG(llevBug, "BUG: check_map_owner(): Map provided is either null or has no valid path!\n");
		return 0;
	}

	p = strtok(buf, "/");

	while (p)
	{
		snprintf(name, sizeof(name), "%s", p);
		p = strtok(NULL, "/");
	}

	p = strtok(name, "$");

	return p;
}

/* Makes sure the given map is loaded and swapped in.
 * name is path name of the map.
 * flags meaning:
 * 0x1 (MAP_FLUSH): flush the map - always load from the map directory,
 *   and don't do unique items or the like.
 * 0x2 (MAP_PLAYER_UNIQUE) - this is a unique map for each player.
 *   dont do any more name translation on it.
 *
 * Returns a pointer to the given map. */
mapstruct *ready_map_name(const char *name, int flags)
{
	mapstruct *m;
	const char *name_sh;

	if (!name)
		return NULL;

	/* Have we been at this level before? */
	if (flags & MAP_NAME_SHARED)
		m = has_been_loaded_sh(name);
	else
	{
		/* Create a temporary shared string for the name if not explicitly given */
		name_sh = add_string(name);
		m = has_been_loaded_sh(name_sh);
		free_string_shared(name_sh);
	}

	/* Map is good to go, so just return it */
	if (m && (m->in_memory == MAP_LOADING || m->in_memory == MAP_IN_MEMORY))
		return m;

	/* unique maps always get loaded from their original location, and never
	 * a temp location.  Likewise, if map_flush is set, or we have never loaded
	 * this map, load it now.  I removed the reset checking from here -
	 * it seems the probability of a player trying to enter a map that should
	 * reset but hasn't yet is quite low, and removing that makes this function
	 * a bit cleaner (and players probably shouldn't rely on exact timing for
	 * resets in any case - if they really care, they should use the 'maps command. */
	if ((flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE)) || !m)
	{
		/* first visit or time to reset */
		if (m)
		{
			/* Doesn't make much difference */
			clean_tmp_map(m);
			delete_map(m);
		}

		/* Unique maps we handle a bit differently. */
		if (flags & MAP_PLAYER_UNIQUE)
		{
			sqlite3 *db;
			sqlite3_stmt *statement;
			char mapPath[MAX_BUF], path[MAX_BUF], *p;
			FILE *fp;

			/* Open the database */
			db_open(DB_DEFAULT, &db);

			sprintf(mapPath, "%s", name);

			/* We only need the map name. */
			p = strtok(mapPath, "/");
			while (p != NULL)
			{
				sprintf(path, "%s", p);
				p = strtok(NULL, "/");
			}

			/* Prepare the query */
			if (!db_prepare_format(db, &statement, "SELECT data FROM unique_maps WHERE mapPath = '%s';", path))
			{
				LOG(llevBug, "BUG: ready_map_name(): Failed to prepare SQL for unique map! (%s)\n", db_errmsg(db));
				db_close(db);
				return NULL;
			}

			/* Run the query */
			if (db_step(statement) == SQLITE_ROW)
			{
				fp = fopen(name, "w");
				fputs((char *)db_column_text(statement, 0), fp);
				fclose(fp);
			}

			/* Finalize it */
			db_finalize(statement);

			/* Close database */
			db_close(db);
		}

		/* create and load a map */
		if (!(m = load_original_map(name, (flags & MAP_PLAYER_UNIQUE))))
			return NULL;

		/* If a player unique map, no extra unique object file to load.
		 * if from the editor, likewise. */
		if (!(flags & (MAP_FLUSH | MAP_PLAYER_UNIQUE)))
			load_unique_objects(m);

		if (flags & MAP_PLAYER_UNIQUE)
			unlink(name);
	}
	else
	{
		/* If in this loop, we found a temporary map, so load it up. */
		m = load_temporary_map(m);

		if (m == NULL)
			return NULL;

		LOG(llevDebug, "RMN: unique. ");
		load_unique_objects(m);

		LOG(llevDebug, "clean. ");
		clean_tmp_map(m);
		m->in_memory = MAP_IN_MEMORY;

		/* tempnam() on sun systems (probably others) uses malloc
		 * to allocated space for the string.  Free it here.
		 * In some cases, load_temporary_map above won't find the
		 * temporary map, and so has reloaded a new map.  If that
		 * is the case, tmpname is now null */
		FREE_AND_NULL_PTR(m->tmpname);
		/* It's going to be saved anew anyway */
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

void clean_tmp_map(mapstruct *m)
{
	if (m->tmpname == NULL)
		return;

	(void) unlink(m->tmpname);
}

void free_all_maps()
{
	int real_maps = 0;

	while (first_map)
	{
		/* I think some of the callers above before it gets here set this to be
		 * saving, but we still want to free this data */
		if (first_map->in_memory == MAP_SAVING)
			first_map->in_memory = MAP_IN_MEMORY;

		delete_map(first_map);
		real_maps++;
	}

	LOG(llevDebug, "free_all_maps: Freed %d maps\n", real_maps);
}

/* This function updates various attributes about a specific space
 * on the map (what it looks like, whether it blocks magic,
 * has a living creatures, prevents people from passing
 * through, etc) */
void update_position(mapstruct *m, int x, int y)
{
	object *tmp;
	MapSpace *mp;
	int i, ii, flags, move_flags, light;

#ifdef DEBUG_OLDFLAGS
	int oldflags;
	if (!((oldflags = GET_MAP_FLAGS(m, x, y)) & (P_NEED_UPDATE | P_FLAGS_UPDATE)))
		LOG(llevDebug, "DBUG: update_position called with P_NEED_UPDATE|P_FLAGS_UPDATE not set: %s (%d, %d)\n", m->path, x, y);
#endif

	/* save our update flag */
	flags = oldflags & P_NEED_UPDATE;

	/* update our flags */
	if (oldflags & P_FLAGS_UPDATE)
	{
#ifdef DEBUG_CORE
		LOG(llevDebug, "UP - FLAGS: %d,%d\n", x, y);
#endif
		/*LOG(llevDebug, "flags:: %x (%d, %d) %x (NU:%x NE:%x)\n", oldflags, x, y, P_NEED_UPDATE | P_NO_ERROR, P_NEED_UPDATE, P_NO_ERROR);*/
		light = move_flags = 0;

		/* This is a key function and highly often called - every saved tick is good. */
		for (tmp = get_map_ob (m, x, y); tmp; tmp = tmp->above)
		{
			if (QUERY_FLAG(tmp, FLAG_PLAYER_ONLY))
				flags |= P_PLAYER_ONLY;

			if (tmp->type == CHECK_INV)
				flags |= P_CHECK_INV;

			if (tmp->type == MAGIC_EAR)
				flags |= P_MAGIC_EAR;

			if (QUERY_FLAG(tmp, FLAG_IS_PLAYER))
				flags |= P_IS_PLAYER;

			if (QUERY_FLAG(tmp, FLAG_DOOR_CLOSED))
				flags |= P_DOOR_CLOSED;

			if (QUERY_FLAG(tmp, FLAG_ALIVE))
				flags |= P_IS_ALIVE;

			if (QUERY_FLAG(tmp, FLAG_NO_MAGIC))
				flags |= P_NO_MAGIC;

			if (QUERY_FLAG(tmp, FLAG_NO_CLERIC))
				flags |= P_NO_CLERIC;

			if (QUERY_FLAG(tmp, FLAG_BLOCKSVIEW))
				flags |= P_BLOCKSVIEW;

			if (QUERY_FLAG(tmp, FLAG_CAN_REFL_SPELL))
				flags |= P_REFL_SPELLS;

			if (QUERY_FLAG(tmp, FLAG_CAN_REFL_MISSILE))
				flags |= P_REFL_MISSILE;

			if (QUERY_FLAG(tmp, FLAG_WALK_ON))
				flags |= P_WALK_ON;

			if (QUERY_FLAG(tmp, FLAG_WALK_OFF))
				flags |= P_WALK_OFF;

			if (QUERY_FLAG(tmp, FLAG_FLY_ON))
				flags |= P_FLY_ON;

			if (QUERY_FLAG(tmp, FLAG_FLY_OFF))
				flags |= P_FLY_OFF;

			if (QUERY_FLAG(tmp, FLAG_NO_PASS))
			{
				/* we also handle PASS_THRU here...
				 * a.) if NO_PASS is set before, we test for PASS_THRU
				 * - if we have no FLAG_PASS_THRU, we delete PASS_THRU
				 * - if we have FLAG_PASS_THRU, we do nothing - other object blocks always
				 * b.) if no NO_PASS is set, we set it AND set PASS_THRU if needed */
				if (flags & P_NO_PASS)
				{
					/* just fire it... always true */
					if (!QUERY_FLAG(tmp, FLAG_PASS_THRU))
						flags &= ~P_PASS_THRU;
				}
				else
				{
					flags |= P_NO_PASS;
					if (QUERY_FLAG(tmp, FLAG_PASS_THRU))
						flags |= P_PASS_THRU;
				}
			}

			if (QUERY_FLAG(tmp, FLAG_IS_FLOOR))
				move_flags |= tmp->terrain_type;
		}

#ifdef DEBUG_OLDFLAGS
		/* we don't want to rely on this function to have accurate flags, but
		 * since we're already doing the work, we calculate them here.
		 * if they don't match, logic is broken someplace. */
		if (((oldflags & ~(P_FLAGS_UPDATE | P_FLAGS_ONLY | P_NO_ERROR)) != flags) && (!(oldflags & P_NO_ERROR)))
			LOG(llevDebug,"DBUG: update_position: updated flags do not match old flags: %s (%d,%d) old:%x != %x\n", m->path, x, y, (oldflags & ~P_NEED_UPDATE), flags);
#endif

		SET_MAP_FLAGS(m, x, y, flags);
		SET_MAP_MOVE_FLAGS(m, x, y, move_flags);
#if 0
		SET_MAP_LIGHT(m, x, y, light);
#endif
	}

	/* check we must rebuild the map layers for client view */
	if ((oldflags & P_FLAGS_ONLY) || !(oldflags & P_NEED_UPDATE))
		return;

#ifdef DEBUG_CORE
	LOG(llevDebug, "UP - LAYER: %d,%d\n", x, y);
#endif

	mp = &m->spaces[x + m->width * y];
	/* ALWAYS is client layer 0 (cl0) a floor. force it */
	mp->client_mlayer[0] = 0;
	mp->client_mlayer_inv[0] = 0;

	if (mp->layer[1])
	{
		mp->client_mlayer[1] = 1;
		mp->client_mlayer_inv[1] = 1;
	}
	else
		mp->client_mlayer_inv[1] = mp->client_mlayer[1] = -1;

	/* and 2 layers for moving stuff */
	mp->client_mlayer[2] = mp->client_mlayer[3] = -1;
	mp->client_mlayer_inv[2] = mp->client_mlayer_inv[3] = -1;

	/* THE INV FLAG CHECK IS FIRST IMPLEMENTATION AND REALLY NOT THE FASTEST WAY -
	 * WE CAN AVOID IT COMPLETE BY USING A 2nd INV QUEUE */

	/* now we first look for a object for cl3 */
	for (i = 6; i > 1; i--)
	{
		if (mp->layer[i])
		{
			/* the last */
			mp->client_mlayer_inv[3] = mp->client_mlayer[3] = i;
			i--;
			break;
		}
	}

	/* inv LAYER: perhaps we have something invisible before it*/

	/* we skip layer 7 - no invisible stuff on layer 7 */
	for (ii = 6 + 7; ii > i + 6; ii--)
	{
		if (mp->layer[ii])
		{
			mp->client_mlayer_inv[2] = mp->client_mlayer_inv[3];
			/* the last */
			mp->client_mlayer_inv[3] = ii;
			break;
		}
	}

	/* and a last one for cl2 */
	for (; i > 1; i--)
	{
		if (mp->layer[i])
		{
			/* the last */
			mp->client_mlayer[2] = mp->client_mlayer_inv[2] = i;
			break;
		}
	}

	/* in layer[2] we have now normal layer 3 or normal layer 2
	 * now seek a possible inv. object to substitute normal */
	for (ii--; ii > 8; ii--)
	{
		if (mp->layer[ii])
		{
			mp->client_mlayer_inv[2] = ii;
			break;
		}
	}

	/* clear out need update flag */
	SET_MAP_FLAGS(m, x, y, GET_MAP_FLAGS(m, x, y) & ~P_NEED_UPDATE);
}

void set_map_reset_time(mapstruct *map)
{
#ifdef MAP_RESET
#ifdef MAP_MAXRESET
	if (MAP_RESET_TIMEOUT(map) > MAP_MAXRESET)
		MAP_WHEN_RESET(map) = seconds() + MAP_MAXRESET;
	else
#endif
		MAP_WHEN_RESET(map) = seconds() + MAP_RESET_TIMEOUT (map);
#else
	/* Will never be reset */
	MAP_WHEN_RESET(map) = -1;
#endif
}

/* out of map now checks all 8 possible neighbours of
 * a tiled map and loads them in when needed. */
mapstruct *out_of_map(mapstruct *m, int *x, int *y)
{
	/* Simple case - coordinates are within this local map.*/
	if (!m)
		return NULL;

	if (((*x) >= 0) && ((*x) < MAP_WIDTH(m)) && ((*y) >= 0) && ((*y) < MAP_HEIGHT(m)))
		return m;

	/* thats w, nw or sw (3,7 or 6) */
	if (*x < 0)
	{
		/*  nw.. */
		if (*y < 0)
		{
			if (!m->tile_path[7])
				return NULL;

			if (!m->tile_map[7] || m->tile_map[7]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(m, 7);

			*y += MAP_HEIGHT(m->tile_map[7]);
			*x += MAP_WIDTH(m->tile_map[7]);
			return out_of_map(m->tile_map[7], x, y);
		}

		/* sw */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[6])
				return NULL;

			if (!m->tile_map[6] || m->tile_map[6]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(m, 6);

			*y -= MAP_HEIGHT(m);
			*x += MAP_WIDTH(m->tile_map[6]);
			return out_of_map(m->tile_map[6], x, y);
		}

		/* it MUST be west */
		if (!m->tile_path[3])
			return NULL;

		if (!m->tile_map[3] || m->tile_map[3]->in_memory != MAP_IN_MEMORY)
			load_and_link_tiled_map(m, 3);

		*x += MAP_WIDTH(m->tile_map[3]);
		return out_of_map(m->tile_map[3], x, y);
	}

	/* that's e, ne or se (1 ,4 or 5) */
	if (*x >= MAP_WIDTH(m))
	{
		/*  ne.. */
		if (*y < 0)
		{
			if (!m->tile_path[4])
				return NULL;

			if (!m->tile_map[4] || m->tile_map[4]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(m, 4);

			*y += MAP_HEIGHT(m->tile_map[4]);
			*x -= MAP_WIDTH(m);
			return out_of_map(m->tile_map[4], x, y);
		}

		/* se */
		if (*y >= MAP_HEIGHT(m))
		{
			if (!m->tile_path[5])
				return NULL;

			if (!m->tile_map[5] || m->tile_map[5]->in_memory != MAP_IN_MEMORY)
				load_and_link_tiled_map(m, 5);

			*y -= MAP_HEIGHT(m);
			*x -= MAP_WIDTH(m);
			return out_of_map(m->tile_map[5], x, y);
		}

		if (!m->tile_path[1])
			return NULL;

		if (!m->tile_map[1] || m->tile_map[1]->in_memory != MAP_IN_MEMORY)
			load_and_link_tiled_map(m, 1);

		*x -= MAP_WIDTH(m);
		return out_of_map(m->tile_map[1], x, y);
	}

	/* because we have tested x above, we don't need to check
	 * for nw,sw,ne and nw here again. */
	if (*y < 0)
	{
		if (!m->tile_path[0])
			return NULL;

		if (!m->tile_map[0] || m->tile_map[0]->in_memory != MAP_IN_MEMORY)
			load_and_link_tiled_map(m, 0);

		*y += MAP_HEIGHT(m->tile_map[0]);
		return out_of_map(m->tile_map[0], x, y);
	}

	if (*y >= MAP_HEIGHT(m))
	{
		if (!m->tile_path[2])
			return NULL;

		if (!m->tile_map[2] || m->tile_map[2]->in_memory != MAP_IN_MEMORY)
			load_and_link_tiled_map(m, 2);

		*y -= MAP_HEIGHT(m);
		return out_of_map(m->tile_map[2], x, y);
	}

	return NULL;
}

/* this is a special version of out_of_map() - this version ONLY
 * adjust to loaded maps - it will not trigger a re/newload of a
 * tiled map not in memory. If out_of_map() fails to adjust the
 * map positions, it will return NULL when the there is no tiled
 * map and NULL when the map is not loaded.
 * As special marker, x is set 0 when the coordinates are not
 * in a map (outside also possible tiled maps) and to -1 when
 * there is a tiled map but its not loaded. */
mapstruct *out_of_map2(mapstruct *m, int *x, int *y)
{
	/* Simple case - coordinates are within this local map.*/
	if (!m)
	{
		*x = 0;
		return NULL;
	}

	if (((*x) >= 0) && ((*x) < MAP_WIDTH(m)) && ((*y) >= 0) && ((*y) < MAP_HEIGHT(m)))
		return m;

	/* thats w, nw or sw (3,7 or 6) */
	if (*x < 0)
	{
		/*  nw.. */
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

			return out_of_map2(m->tile_map[7], x, y);
		}

		/* sw */
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

			return out_of_map2(m->tile_map[6], x, y);
		}

		/* it MUST be west */
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
		return out_of_map2(m->tile_map[3], x, y);
	}

	/* that's e, ne or se (1 ,4 or 5) */
	if (*x >= MAP_WIDTH(m))
	{
		/*  ne.. */
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

			return out_of_map2(m->tile_map[4], x, y);
		}

		/* se */
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

			return out_of_map2(m->tile_map[5], x, y);
		}

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
		return out_of_map2(m->tile_map[1], x, y);
	}

	/* because we have tested x above, we don't need to check
	 * for nw,sw,ne and nw here again. */
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

		return out_of_map2(m->tile_map[0], x, y);
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
		return out_of_map2(m->tile_map[2], x, y);
	}

	*x = 0;
	return NULL;
}

/* From map.c
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
 * Returns TRUE if successful, or FALSE otherwise. */
int get_rangevector(object *op1, object *op2, rv_vector *retval, int flags)
{
	object	*best;

	if (!get_rangevector_from_mapcoords(op1->map, op1->x, op1->y, op2->map, op2->x, op2->y, retval, flags | RV_NO_DISTANCE))
		return FALSE;

	best = op1;
	/* If this is multipart, find the closest part now */
	if (!(flags & RV_IGNORE_MULTIPART) && op1->more)
	{
		object *tmp;
		int best_distance = retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y, tmpi;

		/* we just tkae the offset of the piece to head to figure
		 * distance instead of doing all that work above again
		 * since the distance fields we set above are positive in the
		 * same axis as is used for multipart objects, the simply arithemetic
		 * below works. */
		for (tmp = op1->more; tmp; tmp = tmp->more)
		{
			tmpi = (op1->x - tmp->x + retval->distance_x) * (op1->x - tmp->x + retval->distance_x) + (op1->y - tmp->y + retval->distance_y) * (op1->y - tmp->y + retval->distance_y);
			if (tmpi < best_distance)
			{
				best_distance = tmpi;
				best = tmp;
			}
		}

		if (best != op1)
		{
			retval->distance_x += op1->x - best->x;
			retval->distance_y += op1->y - best->y;
		}
	}

	retval->part = best;
	retval->distance = isqrt(retval->distance_x * retval->distance_x + retval->distance_y * retval->distance_y);
	retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);

	return TRUE;
}

/* this is the base for get_rangevector above, but can more generally compute the
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
 *   0x8|0x04 - don't calculate distance (or direction) (fastest) */
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
			/*LOG(llevDebug, "DBUG: get_rangevector_from_mapcoords: No tileset path between maps '%s' and '%s'\n", map1->path, map2->path);*/
			return FALSE;
		}

		retval->distance_x -= x1;
		retval->distance_y -= y1;
	}
	else
	{
		/*LOG(llevDebug, "DBUG: get_rangevector_from_mapcoords: objects not on adjacent maps\n");*/
		return FALSE;
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
			return TRUE;
	}

	retval->direction = find_dir_2(-retval->distance_x, -retval->distance_y);
	return TRUE;
}

/* Returns true of op1 and op2 are effectively on the same map
 * (as related to map tiling).
 * this will ONLY work if op1 and op2 are on a DIRECT connected
 * tiled map. Any recursive idea here will kill in a big tiled
 * world map the server. */
int on_same_map(object *op1, object *op2)
{
	if (!op1->map || !op2->map)
		return FALSE;

	if (op1->map == op2->map || op1->map->tile_map[0] == op2->map || op1->map->tile_map[1] == op2->map || op1->map->tile_map[2] == op2->map || op1->map->tile_map[3] == op2->map || op1->map->tile_map[4] == op2->map || op1->map->tile_map[5] == op2->map || op1->map->tile_map[6] == op2->map ||op1->map->tile_map[7] == op2->map)
		return TRUE;

	return FALSE;
}

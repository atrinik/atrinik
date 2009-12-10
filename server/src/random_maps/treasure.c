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
 * This deals with inserting treasures in random maps. */

#include <global.h>
#include <random_map.h>
#include <rproto.h>

/**
 * @defgroup TREASURE_OPTIONS Treasure options
 *
 * Some defines for various options which can be set for random map
 * treasures.
 *@{*/

/** All the treasure is at the C's for onions. */
#define CONCENTRATED  1
/** doors to treasure are hidden. */
#define HIDDEN        2
/** Chest has a key, which is placed randomly in the map. */
#define KEYREQUIRED   4
/** Treasure has doors around it. */
#define DOORED        8
/** Trap dropped in same location as chest. */
#define TRAPPED       16
/** 1/2 as much treasure as default */
#define SPARSE        32
/** 2x as much treasure as default */
#define RICH          64
/** Fill/tile the entire map with treasure */
#define FILLED        128
/** Set this to the last real option, for random */
#define LAST_OPTION   64
/*@}*/

#define NO_PASS_DOORS 0
#define PASS_DOORS 1

/** Defines name of the chest archetype to use for treasures. */
#define CHEST_ARCHETYPE "chest"

/** Defines which treasure list to use for chests in random maps. */
#define CHEST_TREASURELIST "chest"

/** Key arch to use for placing keys. */
#define TREASURE_KEY "key2"

/** Door to use for surround_by_doors() function. */
#define SURROUND_DOOR "door1_locked"

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

	if (!(m = out_of_map(m, &x, &y)))
	{
		return 1;
	}

	r = GET_MAP_FLAGS(m, x, y) & (P_NO_PASS | P_PASS_THRU);

	return r;
}

/**
 * Place treasures in the map.
 * @param map Where to insert to.
 * @param layout Layout the map was generated from.
 * @param treasure_style Treasure style. May be empty or NULL for random
 * style, or "none" for no treasures.
 * @param treasureoptions Treasure options. 0 for random choices or
 * positive
 * @param RP Random map parameters. */
void place_treasure(mapstruct *map, char **layout, char *treasure_style, int treasureoptions, RMParms *RP)
{
	char styledirname[256], stylefilepath[256];
	mapstruct *style_map = 0;
	int num_treasures;

	/* bail out if treasure isn't wanted. */
	if (treasure_style)
	{
		if (!strcmp(treasure_style, "none"))
		{
			return;
		}
	}

	if (treasureoptions <= 0)
	{
		treasureoptions = RANDOM() % (2 * LAST_OPTION);
	}

	/* Filter out the mutually exclusive options */
	if ((treasureoptions & RICH) && (treasureoptions & SPARSE))
	{
		if (RANDOM() % 2)
		{
			treasureoptions -= 1;
		}
		else
		{
			treasureoptions -= 2;
		}
	}

	/* Pick the number of treasures */
	if (treasureoptions & SPARSE)
	{
		num_treasures = BC_RANDOM(RP->total_map_hp / 600 + RP->difficulty / 2 + 1);
	}
	else if (treasureoptions & RICH)
	{
		num_treasures = BC_RANDOM(RP->total_map_hp / 150 + 2 * RP->difficulty + 1);
	}
	else
	{
		num_treasures = BC_RANDOM(RP->total_map_hp / 300 + RP->difficulty + 1);
	}

	if (num_treasures <= 0 )
	{
		return;
	}

	/* Get the style map */
	snprintf(styledirname, sizeof(styledirname), "%s", "/styles/treasurestyles");
	snprintf(stylefilepath, sizeof(stylefilepath), "%s/%s", styledirname, treasure_style);

	style_map = find_style(styledirname, treasure_style, -1);

	/* All the treasure at one spot in the map. */
	if (treasureoptions & CONCENTRATED)
	{
		/* map_layout_style global, and is previously set */
		switch (RP->map_layout_style)
		{
			case ONION_LAYOUT:
			case SPIRAL_LAYOUT:
			case SQUARE_SPIRAL_LAYOUT:
			{
				int i, j;

				/* search the onion for C's or '>', and put treasure there. */
				for (i = 0; i < RP->Xsize; i++)
				{
					for (j = 0; j < RP->Ysize; j++)
					{
						if (layout[i][j] == 'C' || layout[i][j] == '>')
						{
							int tdiv = RP->symmetry_used;
							object *chest;

							/* this symmetry uses a divisor of 2*/
							if (tdiv == 3)
							{
								tdiv = 2;
							}

							/* Don't put a chest on an exit. */
							chest = place_chest(treasureoptions, i, j, map, style_map, num_treasures / tdiv, RP);

							/* if no chest was placed NEXT */
							if (!chest)
							{
								continue;
							}

							if (treasureoptions & (DOORED | HIDDEN))
							{
								object **doorlist = find_doors_in_room(map, i, j, RP);

								lock_and_hide_doors(doorlist, map, treasureoptions, RP);
								free(doorlist);
							}
						}
					}
				}

				break;
			}

			default:
			{
				int i = -1, j = -1, tries = 0;
				object *chest;

				while (i == -1 && tries < 100)
				{
					i = RANDOM() % (RP->Xsize - 2) + 1;
					j = RANDOM() % (RP->Ysize - 2) + 1;

					find_enclosed_spot(map, &i, &j, RP);

					if (wall_blocked(map, i, j))
					{
						i = -1;
					}

					tries++;
				}

				chest = place_chest(treasureoptions, i, j, map, style_map, num_treasures, RP);

				if (!chest)
				{
					return;
				}

				i = chest->x;
				j = chest->y;

				if (treasureoptions & (DOORED | HIDDEN))
				{
					object **doorlist = surround_by_doors(map, layout, i, j, treasureoptions);

					lock_and_hide_doors(doorlist, map, treasureoptions, RP);
					free(doorlist);
				}
			}
		}
	}
	/* DIFFUSE treasure layout */
	else
	{
		int ti, i, j;

		for (ti = 0; ti < num_treasures; ti++)
		{
			i = RANDOM() % (RP->Xsize - 2) + 1;
			j = RANDOM() % (RP->Ysize - 2) + 1;

			place_chest(treasureoptions, i, j, map, style_map, 1, RP);
		}
	}
}

/**
 * Put a chest into the map.
 * near x and y, with the treasure style
 * determined (may be NULL, or may be a treasure list from lib/treasures,
 * if the global variable "treasurestyle" is set to that treasure list's
 * name).
 * @param treasureoptions Options.
 * @param x X position around which to put treasure.
 * @param y Y position around which to put treasure.
 * @param map Map to put on.
 * @param RP Random map parameters.
 * @return Inserted chest, NULL on failure. */
object *place_chest(int treasureoptions, int x, int y, mapstruct *map, mapstruct *style_map, int n_treasures, RMParms *RP)
{
	object *the_chest;
	int i, xl, yl;

	(void) style_map;

	/* first, find a place to put the chest. */
	i = find_first_free_spot(find_archetype(CHEST_ARCHETYPE), map, x, y);
	xl = x + freearr_x[i];
	yl = y +  freearr_y[i];

	/* if the placement is blocked, return a fail. */
	if (wall_blocked(map,xl,yl))
	{
		return NULL;
	}

	the_chest = get_archetype(CHEST_ARCHETYPE);

	treasurelist *tlist = find_treasurelist(CHEST_TREASURELIST);
	the_chest->randomitems = tlist;
	the_chest->stats.hp = n_treasures;

	/* Stick a trap in the chest if required  */
	if (treasureoptions & TRAPPED)
	{
		mapstruct *trap_map = find_style("/styles/trapstyles", "traps", -1);
		object *the_trap;

		if (trap_map)
		{
			the_trap = pick_random_object(trap_map);
			the_trap->stats.Cha = 10 + RP->difficulty;
			the_trap->level = BC_RANDOM((3 * RP->difficulty) / 2);

			if (the_trap)
			{
				object *new_trap = arch_to_object(the_trap->arch);

				copy_object(new_trap, the_trap);
				new_trap->x = x;
				new_trap->y = y;

				insert_ob_in_ob(new_trap, the_chest);
			}
		}
	}

	/* Set the chest lock code, and call the keyplacer routine with
	 * the lockcode.  It's not worth bothering to lock the chest if
	 * there's only 1 treasure....*/
	if ((treasureoptions & KEYREQUIRED) && n_treasures > 1)
	{
		char keybuf[256];

		snprintf(keybuf, sizeof(keybuf), "%d", (int) RANDOM());
		FREE_AND_COPY_HASH(the_chest->slaying, keybuf);

		keyplace(map, x, y, keybuf, PASS_DOORS, 1, RP);
	}

	/* Actually place the chest. */
	the_chest->x = xl;
	the_chest->y = yl;

	insert_ob_in_map(the_chest, map, NULL, 0);

	return the_chest;
}

/**
 * Finds the closest monster and returns him, regardless of doors
 * or walls.
 * @param map Map where to look for.
 * @param x X position where to look from.
 * @param y Y position where to look from.
 * @param RP Random map parameters.
 * @return The monster, or NULL if not found. */
object *find_closest_monster(mapstruct *map, int x, int y)
{
	int i, lx, ly;
	mapstruct *mt;

	for (i = 0; i < SIZEOFFREE; i++)
	{
		lx = x + freearr_x[i];
		ly = y + freearr_y[i];

		if (!(mt = out_of_map(map, &lx, &ly)))
		{
			continue;
		}

		/* Don't bother searching this square unless the map says life exists. */
		if (GET_MAP_FLAGS(mt, lx, ly) & P_IS_ALIVE)
		{
			object *the_monster = get_map_ob(mt, lx, ly);

			for ( ; the_monster != NULL && (!QUERY_FLAG(the_monster, FLAG_MONSTER)); the_monster = the_monster->above)
			{
			}

			if (the_monster && QUERY_FLAG(the_monster, FLAG_MONSTER))
			{
				return the_monster;
			}
		}
	}

	return NULL;
}

/**
 * Places keys in the map, preferably in something alive.
 *
 * The idea is that you call keyplace on x, y where a door is, and it'll
 * make sure a key is placed on both sides of the door.
 * @param map Map where to put the key.
 * @param x X position where to put the key.
 * @param y Y position where to put the key.
 * @param keycode The key's code.
 * @param door_flag If NO_PASS_DOORS won't cross doors or walls to
 * keyplace, PASS_DOORS will. If PASS_DOORS is set, the x & y values that
 * are passed in are basically meaningless - IMO, it is a bit of
 * misnomer, as when it is set, it just randomly chooses spaces on the
 * map, ideally finding a close monster, to put the key in. \n
 * In fact, if PASS_DOORS is set, there is no guarantee that the keys
 * will be on both sides of the door - it may happen by randomness, but
 * the code doesn't work to make sure it happens.
 * @param n_keys Number of keys to place. If 1, it will place 1 key.
 * Otherwise, it will place 2-4 keys.
 * @param RP Random map parameters.
 * @return 1 if key was successfully placed, 0 otherwise. */
int keyplace(mapstruct *map, int x, int y, char *keycode, int door_flag, int n_keys, RMParms *RP)
{
	int i = 0, j = 0, kx, ky;
	object *the_keymaster, *the_key;
	char keybuf[MAX_BUF];

	/* Get a key and set its keycode */
	the_key = get_archetype(TREASURE_KEY);
	FREE_AND_COPY_HASH(the_key->slaying, keycode);

	snprintf(keybuf, sizeof(keybuf), "key from level %d of a random map", RP->dungeon_level);
	FREE_AND_COPY_HASH(the_key->name, keybuf);

	if (door_flag == PASS_DOORS)
	{
		int tries = 0;

		the_keymaster = NULL;

		while (tries < 5 && the_keymaster == NULL)
		{
			i = (RANDOM() % (RP->Xsize - 2)) + 1;
			j = (RANDOM() % (RP->Ysize - 2)) + 1;

			tries++;

			the_keymaster = find_closest_monster(map, i, j);
		}

		/* If we don't find a good keymaster, drop the key on the ground. */
		if (the_keymaster == NULL)
		{
			int freeindex = find_first_free_spot(the_key->arch, map, i, j);

			kx = i + freearr_x[freeindex];
			ky = j + freearr_y[freeindex];
		}
	}
	/* NO_PASS_DOORS --we have to work harder. */
	else
	{
		/* don't try to keyplace if we're sitting on a blocked square and
		 * NO_PASS_DOORS is set. */
		if (n_keys == 1)
		{
			if (wall_blocked(map, x, y))
			{
				return 0;
			}

			the_keymaster = find_monster_in_room(map, x, y, RP);

			/* if fail, find a spot to drop the key. */
			if (the_keymaster == NULL)
			{
				find_spot_in_room(map, x, y, &kx, &ky, RP);
			}
		}
		else
		{
			/* count how many keys we actually place */
			int sum = 0;

			/* I'm lazy, so just try to place in all 4 directions. */
			sum += keyplace(map, x + 1, y, keycode, NO_PASS_DOORS, 1, RP);
			sum += keyplace(map, x, y + 1, keycode, NO_PASS_DOORS, 1, RP);
			sum += keyplace(map, x - 1, y, keycode, NO_PASS_DOORS, 1, RP);
			sum += keyplace(map, x, y - 1, keycode, NO_PASS_DOORS, 1, RP);

			/* We might have made a disconnected map - place more keys,
			 * diagonally this time. */
			if (sum < 2)
			{
				keyplace(map, x + 1, y + 1, keycode, NO_PASS_DOORS, 1, RP);
				keyplace(map, x + 1, y - 1, keycode, NO_PASS_DOORS, 1, RP);
				keyplace(map, x - 1, y + 1, keycode, NO_PASS_DOORS, 1, RP);
				keyplace(map, x - 1, y - 1, keycode, NO_PASS_DOORS, 1, RP);
			}

			return 1;
		}
	}

	if (the_keymaster == NULL)
	{
		the_key->x = kx;
		the_key->y = ky;

		insert_ob_in_map(the_key, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

		return 1;
	}

	insert_ob_in_ob(the_key, the_keymaster);

	return 1;
}

/**
 * A recursive routine which will return a monster, eventually, if there is one.
 *
 * One should really call find_monster_in_room().
 * @param layout Map layout.
 * @param map Generated map.
 * @param x X position where to look from.
 * @param y Y position where to look from.
 * @param RP Random map parameters.
 * @return Monster, or NULL if none found. */
object *find_monster_in_room_recursive(char **layout, mapstruct *map, int x, int y, RMParms *RP)
{
	int i, j;
	object *monster;

	/* bounds check x and y */
	if (!(map = out_of_map(map, &x, &y)))
	{
		return NULL;
	}

	/* if the square is blocked or searched already, leave */
	if (layout[x][y] != '\0')
	{
		return NULL;
	}

	/* check the current square for a monster.  If there is one,
	 * set theMonsterToFind and return it. */
	layout[x][y] = 1;

	if (GET_MAP_FLAGS(map, x, y) & P_IS_ALIVE)
	{
		monster = get_map_ob(map, x, y);

		/* Check off this point */
		for ( ; monster != NULL && (!QUERY_FLAG(monster, FLAG_MONSTER)); monster = monster->above)
		{
		}

		if (monster && QUERY_FLAG(monster, FLAG_MONSTER))
		{
			return monster;
		}
	}

	/* now search all the 8 squares around recursively for a monster, in random order */
	for (i = RANDOM() % 8, j = 0; j < 8; i++, j++)
	{
		monster = find_monster_in_room_recursive(layout, map, x + freearr_x[i % 8 + 1], y + freearr_y[i % 8 + 1], RP);

		if (monster != NULL)
		{
			return monster;
		}
	}

	return NULL;
}

/**
 * Find a monster in a room. Real work is done by
 * find_monster_in_room_recursive().
 * @param map Generated map.
 * @param x X position where to look from.
 * @param y Y position where to look from.
 * @param RP Random map parameters.
 * @return Monster, or NULL if none found.
 * @todo Couldn't the layout be given instead of being calculated? */
object *find_monster_in_room(mapstruct *map, int x, int y, RMParms *RP)
{
	char **layout2 = (char **) calloc(sizeof(char *), RP->Xsize);
	int i, j;
	object *monster;

	/* Allocate and copy the layout, converting C to 0. */
	for (i = 0; i < RP->Xsize; i++)
	{
		layout2[i] = (char *) calloc(sizeof(char), RP->Ysize);

		for (j = 0; j < RP->Ysize; j++)
		{
			if (wall_blocked(map, i, j))
			{
				layout2[i][j] = '#';
			}
		}
	}

	monster = find_monster_in_room_recursive(layout2, map, x, y, RP);

	/* Deallocate the temporary layout */
	for (i = 0; i < RP->Xsize; i++)
	{
		free(layout2[i]);
	}

	free(layout2);

	return monster;
}


/**
 * Datastructure needed by find_spot_in_room() and
 * find_spot_in_room_recursive() */
typedef struct free_spots_struct {
	/** Positions. */
    int *room_free_spots_x;

	/** Positions. */
    int *room_free_spots_y;

	/** Number of positions. */
    int number_of_free_spots_in_room;
} free_spots_struct;

/**
 * The workhorse routine, which finds the free spots in a room:
 *
 * a datastructure of free points is set up, and a position chosen from
 * that datastructure.
 * @param layout Map layout.
 * @param x X position where to look from.
 * @param y Y position where to look from.
 * @param RP Random map parameters.
 * @param spots Currently found free spots. */
static void find_spot_in_room_recursive(char **layout, int x, int y, RMParms *RP, free_spots_struct *spots)
{
	int i, j;

	/* bounds check x and y */
	if (!(x >= 0 && y >= 0 && x < RP->Xsize && y < RP->Ysize))
	{
		return;
	}

	/* if the square is blocked or searched already, leave */
	if (layout[x][y] != '\0')
	{
		return;
	}

	/* Set the current square as checked, and add it to the list.
	 * Check off this point */
	layout[x][y] = 1;

	spots->room_free_spots_x[spots->number_of_free_spots_in_room] = x;
	spots->room_free_spots_y[spots->number_of_free_spots_in_room] = y;
	spots->number_of_free_spots_in_room++;

	/* Now search all the 8 squares around recursively for free spots,
	 * in random order */
	for (i = RANDOM() % 8, j = 0; j < 8; i++, j++)
	{
		find_spot_in_room_recursive(layout, x + freearr_x[i % 8 + 1], y + freearr_y[i % 8 + 1], RP, spots);
	}
}

/**
 * Find a random non-blocked spot in this room to drop a key.
 * @param map Nap to look into.
 * @param x X position where to look from.
 * @param y Y position where to look from.
 * @param RP Random map parameters.
 * @return 1 if spot found, 0 otherwise.
 * @todo Couldn't layout be given instead of being computed? */
void find_spot_in_room(mapstruct *map, int x, int y, int *kx, int *ky, RMParms *RP)
{
	char **layout2 = (char **) calloc(sizeof(char *),RP->Xsize);
	int i, j;
	free_spots_struct spots;

	spots.number_of_free_spots_in_room = 0;
	spots.room_free_spots_x = (int *) calloc(sizeof(int), RP->Xsize * RP->Ysize);
	spots.room_free_spots_y = (int *) calloc(sizeof(int), RP->Xsize * RP->Ysize);

	/* Allocate and copy the layout, converting C to 0. */
	for (i = 0; i < RP->Xsize; i++)
	{
		layout2[i] = (char *) calloc(sizeof(char), RP->Ysize);

		for (j = 0; j < RP->Ysize; j++)
		{
			if (wall_blocked(map, i, j))
			{
				layout2[i][j] = '#';
			}
		}
	}

	/* Setup num_free_spots and room_free_spots */
	find_spot_in_room_recursive(layout2, x, y, RP, &spots);

	if (spots.number_of_free_spots_in_room > 0)
	{
		i = RANDOM() % spots.number_of_free_spots_in_room;

		*kx = spots.room_free_spots_x[i];
		*ky = spots.room_free_spots_y[i];
	}

	/* Deallocate the temporary layout */
	for (i = 0; i < RP->Xsize; i++)
	{
		free(layout2[i]);
	}

	free(layout2);
	free(spots.room_free_spots_x);
	free(spots.room_free_spots_y);
}

/**
 * Searches the map for a spot with walls around it.
 *
 * The more walls the better, but it'll settle for 1 wall, or even 0, but
 * it'll return 0 if no FREE spots are found.
 * @param map Map where to look.
 * @param RP Random map parameters. */
void find_enclosed_spot(mapstruct *map, int *cx, int *cy,RMParms *RP)
{
	int x = *cx, y = *cy, i;

	for (i = 0; i < SIZEOFFREE; i++)
	{
		int lx, ly, sindex;

		lx = x + freearr_x[i];
		ly = y + freearr_y[i];

		sindex = surround_flag3(map, lx, ly, RP);

		/* If it's blocked on 3 sides, it's enclosed */
		if (sindex == 7 || sindex == 11 || sindex == 13 || sindex == 14)
		{
			*cx = lx;
			*cy = ly;

			return;
		}
	}

	/* OK, if we got here, we're obviously someplace where there's no enclosed
	 * spots--try to find someplace which is 2x enclosed.  */
	for (i = 0; i < SIZEOFFREE; i++)
	{
		int lx, ly, sindex;

		lx = x + freearr_x[i];
		ly = y + freearr_y[i];

		sindex = surround_flag3(map, lx, ly, RP);

		/* If it's blocked on 3 sides, it's enclosed */
		if (sindex == 3 || sindex == 5 || sindex == 9 || sindex == 6 || sindex == 10 || sindex == 12)
		{
			*cx= lx;
			*cy= ly;

			return;
		}
	}

	/* Settle for one surround point */
	for (i = 0; i < SIZEOFFREE; i++)
	{
		int lx, ly, sindex;

		lx = x + freearr_x[i];
		ly = y + freearr_y[i];

		sindex = surround_flag3(map, lx, ly, RP);

		/* If it's blocked on 3 sides, it's enclosed */
		if (sindex)
		{
			*cx = lx;
			*cy = ly;

			return;
		}
	}

	/* Give up and return the closest free spot. */
	i = find_first_free_spot(find_archetype(CHEST_ARCHETYPE), map, x, y);

	if (i != -1 && i < SIZEOFFREE)
	{
		*cx = x + freearr_x[i];
		*cy = y + freearr_y[i];
	}

	/* Indicate failure */
	*cx = *cy = -1;
}

/**
 * Remove living things on specified spot. */
void remove_monsters(int x, int y, mapstruct *map)
{
	object *tmp;

	for (tmp = get_map_ob(map, x, y); tmp != NULL; tmp = tmp->above)
	{
		if (QUERY_FLAG(tmp, FLAG_MONSTER))
		{
			if (tmp->head)
			{
				tmp = tmp->head;
			}

			remove_ob(tmp);
			tmp = get_map_ob(map, x, y);

			if (tmp == NULL)
			{
				break;
			}
		}
	}
}

/**
 * Surrounds the point x, y by doors, so as to enclose something, like
 * a chest.
 *
 * It only goes as far as the 8 squares surrounding, and it'll remove
 * any monsters it finds.
 * @param map Map to work on.
 * @param layout Map's layout.
 * @param x X position to surround.
 * @param y Y position to surround.
 * @param opts Option flags. Currently unused.
 * @return Array of generated doors, NULL-terminated. Should be freed by
 * caller. */
object **surround_by_doors(mapstruct *map, char **layout, int x, int y, int opts)
{
	int i, ndoors_made = 0;
	object **doorlist = (object **) calloc(9, sizeof(object *));

	(void) opts;

	/* Place doors in all the 8 adjacent unblocked squares. */
	for (i = 1; i < 9; i++)
	{
		int x1 = x + freearr_x[i], y1 = y + freearr_y[i];

		/* Place a door */
		if (!wall_blocked(map, x1, y1) || layout[x1][y1] == '>')
		{
			object *new_door = get_archetype(SURROUND_DOOR);

			if (freearr_x[i] != 0 || freearr_y[i] == 0)
			{
				new_door->direction = 3;

				if (QUERY_FLAG(new_door, FLAG_IS_TURNABLE) || QUERY_FLAG(new_door, FLAG_ANIMATE))
				{
					SET_ANIMATION(new_door, (NUM_ANIMATIONS(new_door) / NUM_FACINGS(new_door)) * new_door->direction + new_door->state);
				}
			}

			new_door->x = x + freearr_x[i];
			new_door->y = y + freearr_y[i];
			remove_monsters(new_door->x, new_door->y, map);
			insert_ob_in_map(new_door, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

			doorlist[ndoors_made] = new_door;
			ndoors_made++;
		}
	}

	return doorlist;
}

/**
 * Returns the first door in this square, or NULL if there isn't a door.
 * @param map Map where to look.
 * @param x X position where to look.
 * @param y Y position where to look.
 * @return The door, or NULL if none found. */
object *door_in_square(mapstruct *map, int x, int y)
{
	object *tmp;

	for (tmp = get_map_ob(map, x, y); tmp != NULL; tmp = tmp->above)
	{
		if (tmp->type == DOOR || tmp->type == LOCKED_DOOR)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * The workhorse routine, which finds the doors in a room.
 * @param doorlist List of doors.
 * @param ndoors Number of found doors.
 * @param RP Random map parameters. */
void find_doors_in_room_recursive(char **layout, mapstruct *map, int x, int y, object **doorlist, int *ndoors, RMParms *RP)
{
	int i, j;
	object *door;

	/* bounds check x and y */
	if (!(x >= 0 && y >= 0 && x < RP->Xsize && y < RP->Ysize))
	{
		return;
	}

	/* if the square is blocked or searched already, leave */
	if (layout[x][y] == 1)
	{
		return;
	}

	/* there could be a door here */
	if (layout[x][y] == '#')
	{
		/* check off this point */
		layout[x][y] = 1;

		door = door_in_square(map, x, y);

		if (door != NULL)
		{
			doorlist[*ndoors] = door;

			/* eek!  out of memory */
			if (*ndoors > 254)
			{
				printf("find_doors_in_room_recursive:Too many doors for memory allocated!\n");
				return;
			}

			*ndoors = *ndoors + 1;
		}
	}
	else
	{
		layout[x][y] = 1;

		/* Now search all the 8 squares around recursively for free spots,in random order */
		for (i = RANDOM() % 8, j = 0; j < 8; i++, j++)
		{
			find_doors_in_room_recursive(layout, map, x + freearr_x[i % 8 + 1], y +freearr_y[i % 8 + 1], doorlist, ndoors, RP);
		}
	}
}


/**
 * Gets all doors in a room.
 * @param map Map to look into.
 * @param x X position in a room to find door for.
 * @param y Y position in a room to find door for.
 * @param RP Random map parameters.
 * @return Door list. Should be freed by caller. NULL-terminated.
 * @todo Couldn't layout be given instead of being computed? */
object** find_doors_in_room(mapstruct *map, int x, int y, RMParms *RP)
{
	char **layout2 = (char **) calloc(sizeof(char *), RP->Xsize);
	object **doorlist = (object **) calloc(sizeof(int), 256);
	int i, j, ndoors = 0;

	/* Allocate and copy the layout, converting C to 0. */
	for (i = 0; i < RP->Xsize; i++)
	{
		layout2[i] = (char *) calloc(sizeof(char), RP->Ysize);

		for (j = 0; j < RP->Ysize; j++)
		{
			if (wall_blocked(map, i, j))
			{
				layout2[i][j] = '#';
			}
		}
	}

	/* Setup num_free_spots and room_free_spots */
	find_doors_in_room_recursive(layout2, map, x, y, doorlist, &ndoors, RP);

	/* Deallocate the temporary layout */
	for (i = 0; i < RP->Xsize; i++)
	{
		free(layout2[i]);
	}

	free(layout2);

	return doorlist;
}

/**
 * Locks and/or hides all the doors in doorlist, or does nothing if
 * opts doesn't say to lock/hide doors.
 *
 * Note that some doors can be not locked if no good spot to put a key was found.
 * @param doorlist Doors to list. NULL-terminated.
 * @param map Map we're working on.
 * @param opts Options.
 * @param RP Random map parameters. */
void lock_and_hide_doors(object **doorlist, mapstruct *map, int opts, RMParms *RP)
{
	object *door;
	int i;

	/* lock the doors and hide the keys. */
	if (opts & DOORED)
	{
		for (i = 0, door = doorlist[0]; doorlist[i] != NULL; i++)
		{
			object *new_door = get_archetype(SURROUND_DOOR);
			char keybuf[256];

			door = doorlist[i];

			new_door->face = door->face;
			new_door->x = door->x;
			new_door->y = door->y;
			remove_ob(door);
			doorlist[i] = new_door;

			insert_ob_in_map(new_door, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

			snprintf(keybuf, sizeof(keybuf), "%d", (int) RANDOM());
			FREE_AND_COPY_HASH(new_door->slaying, keybuf);

			keyplace(map, new_door->x, new_door->y, keybuf, NO_PASS_DOORS, 2, RP);
		}
	}

	/* Change the faces of the doors and surrounding walls to hide them. */
	if (opts & HIDDEN)
	{
		for (i = 0, door = doorlist[0]; doorlist[i] != NULL; i++)
		{
			object *wallface;

			door = doorlist[i];

			wallface = retrofit_joined_wall(map, door->x, door->y, 1, RP);

			if (wallface != NULL)
			{
				retrofit_joined_wall(map, door->x - 1, door->y, 0, RP);
				retrofit_joined_wall(map, door->x + 1, door->y, 0, RP);
				retrofit_joined_wall(map, door->x, door->y - 1, 0, RP);
				retrofit_joined_wall(map, door->x, door->y + 1, 0, RP);

				door->face = wallface->face;

				if (!QUERY_FLAG(wallface, FLAG_REMOVED))
				{
					remove_ob(wallface);
				}
			}
		}
	}
}

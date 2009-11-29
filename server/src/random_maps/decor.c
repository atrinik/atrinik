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
 * These functions handle decoration in random maps. */

#include <global.h>
#include <random_map.h>
#include <rproto.h>

/** Number of decor styles that can be chosen if none specified. */
#define NR_DECOR_OPTIONS 1

/**
 * Count number of objects at a spot.
 * @param map Map we want to check
 * @param x X coordinate
 * @param y Y coordinate
 * @return Count of objects in the map at x, y. */
static int obj_count_in_map(mapstruct *map, int x, int y)
{
	int count = 0;
	object *tmp;

	if (!(map = out_of_map(map, &x, &y)))
	{
		return 0;
	}

	for (tmp = get_map_ob(map, x, y); tmp != NULL; tmp = tmp->above)
	{
		count++;
	}

	return count;
}

/**
 * Put the decor into the map.
 *
 * Right now, it's very primitive.
 * @param map Map to add decor to.
 * @param maze Layout of the map, as was generated.
 * @param decorstyle Style to use. Can be NULL, in which case random is
 * selected.
 * @param decor_option How to place the decoration:
 * - <b>0</b>: No decoration.
 * - <b>1</b>: Randomly place decoration.
 * - <b>Anything else</b>: Fill the map with decoration.
 * @param RP Parameters of the random map. */
void put_decor(mapstruct *map, char **maze, char *decorstyle, int decor_option, RMParms *RP)
{
	mapstruct *decor_map;
	char style_name[256];

	snprintf(style_name, sizeof(style_name), "/styles/decorstyles");

	decor_map = find_style(style_name, decorstyle, -1);

	if (decor_map == NULL)
	{
		return;
	}

	/* pick a random option, only 1 option right now. */
	if (decor_option == 0)
	{
		decor_option = RANDOM() % NR_DECOR_OPTIONS + 1;
	}

	switch (decor_option)
	{
		case 0:
			break;

		/* Random placement of decor objects. */
		case 1:
		{
			int number_to_place = RANDOM() % ((RP->Xsize * RP->Ysize) / 5), failures = 0;
			object *new_decor_object;

			while (failures < 100 && number_to_place > 0)
			{
				int x = RANDOM() % (RP->Xsize - 2) + 1, y = RANDOM() % (RP->Ysize - 2) + 1;

				/* Empty */
				if (maze[x][y] == 0 && obj_count_in_map(map, x, y) < 2)
				{
					object *this_object;

					new_decor_object = pick_random_object(decor_map);
					this_object = arch_to_object(new_decor_object->arch);
					copy_object(new_decor_object, this_object);
					this_object->x = x;
					this_object->y = y;

					/* It screws things up if decor can stop people */
					CLEAR_FLAG(this_object, FLAG_NO_PASS);

					insert_ob_in_map(this_object, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);

					number_to_place--;
				}
				else
				{
					failures++;
				}
			}

			break;
		}

		/* Place decor objects everywhere: tile the map. */
		default:
		{
			int x, y;

			for (x = 1; x < RP->Xsize - 1; x++)
			{
				for (y = 1; y < RP->Ysize - 1; y++)
				{
					if (maze[x][y] == 0)
					{
						object *new_decor_object, *this_object;

						new_decor_object = pick_random_object(decor_map);
						this_object = arch_to_object(new_decor_object->arch);
						copy_object(new_decor_object, this_object);
						this_object->x = x;
						this_object->y = y;

						/* It screws things up if decor can stop people */
						CLEAR_FLAG(this_object, FLAG_NO_PASS);

						insert_ob_in_map(this_object, map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
					}
				}
			}

			break;
		}
	}
}

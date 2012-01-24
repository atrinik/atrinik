/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Random map floor handling. */

#include <global.h>

/**
 * Creates the Atrinik map structure from the layout, and adds the floor.
 * @param floorstyle floor style. Can be NULL, in which case a random one
 * is chosen.
 * @param RP Random map parameters.
 * @return Atrinik map structure. */
mapstruct *make_map_floor(char *floorstyle, RMParms *RP)
{
	char styledirname[256], stylefilepath[256];
	mapstruct *style_map = NULL, *newMap = NULL;
	int x, y;

	/* Allocate the map */
	newMap = get_empty_map(RP->Xsize, RP->Ysize);

	/* Get the style map */
	strncpy(styledirname, "/styles/floorstyles", sizeof(styledirname) - 1);
	snprintf(stylefilepath, sizeof(stylefilepath), "%s/%s", styledirname, floorstyle);
	style_map = find_style(styledirname, floorstyle, -1);

	if (style_map == NULL)
	{
		return newMap;
	}

	/* Fill up the map with the given floor style */
	for (x = 0; x < RP->Xsize; x++)
	{
		for (y = 0; y < RP->Ysize; y++)
		{
			object *the_floor = pick_random_object(style_map), *thisfloor = get_object();

			copy_object(the_floor, thisfloor, 0);
			thisfloor->x = x;
			thisfloor->y = y;

			insert_ob_in_map(thisfloor, newMap, thisfloor, INS_NO_MERGE | INS_NO_WALK_ON);
		}
	}

	return newMap;
}

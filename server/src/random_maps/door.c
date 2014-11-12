/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Door related functions. */

#include <global.h>

/**
 * Search for doors or walls around a spot.
 * @param layout Maze.
 * @param x X coordinate to check
 * @param y Y coordinate to check
 * @param Xsize X map size
 * @param Ysize Y map size
 * @return Combination of flags:
 * - <b>1</b>: Door or wall to left.
 * - <b>2</b>: Door or wall to right.
 * - <b>4</b>: Door or wall above.
 * - <b>8</b>: Door or wall below. */
int surround_check2(char **layout, int x, int y, int Xsize, int Ysize)
{
    /* 1 = door or wall to left,
     *     2 = door or wall to right,
     *     4 = door or wall above
     *     8 = door or wall below */
    int surround_index = 0;

    if ((x > 0) && (layout[x - 1][y] == 'D' || layout[x - 1][y] == '#')) {
        surround_index += 1;
    }

    if ((x < Xsize - 1) && (layout[x + 1][y] == 'D' || layout[x + 1][y] == '#')) {
        surround_index += 2;
    }

    if ((y > 0) && (layout[x][y - 1] == 'D' || layout[x][y - 1] == '#')) {
        surround_index += 4;
    }

    if ((y < Ysize - 1) && (layout[x][y + 1] == 'D' || layout[x][y + 1] == '#')) {
        surround_index += 8;
    }

    return surround_index;
}

/**
 * Add doors to a map.
 * @param the_map Map we're adding the doors to.
 * @param maze Maze layout.
 * @param doorstyle Door style to use. If NULL, will choose one randomly.
 * @param RP Random map parameters. */
void put_doors(mapstruct *the_map, char **maze, char *doorstyle, RMParms *RP)
{
    int x, y;
    mapstruct *vdoors, *hdoors;
    char doorpath[128];

    if (!strcmp(doorstyle, "none")) {
        return;
    }

    vdoors = find_style("/styles/doorstyles/vdoors", doorstyle, -1);

    if (!vdoors) {
        return;
    }

    snprintf(doorpath, sizeof(doorpath), "/styles/doorstyles/hdoors%s", strrchr(vdoors->path, '/'));
    hdoors = find_style(doorpath, 0, -1);

    for (x = 0; x < RP->Xsize; x++) {
        for (y = 0; y < RP->Ysize; y++) {
            if (maze[x][y] == 'D') {
                int sindex = surround_check2(maze, x, y, RP->Xsize, RP->Ysize);
                object *this_door, *new_door;

                if (sindex == 3) {
                    this_door = pick_random_object(hdoors);
                }
                else {
                    this_door = pick_random_object(vdoors);
                }

                new_door = arch_to_object(this_door->arch);
                copy_object(this_door, new_door, 0);
                new_door->x = x;
                new_door->y = y;

                insert_ob_in_map(new_door, the_map, NULL, INS_NO_MERGE | INS_NO_WALK_ON);
            }
        }
    }
}

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
 * Rogue/nethack-like layout generation. */

#include <global.h>

typedef struct {
    /* coordinates of room centers */
    int x;
    int y;

    /* sizes */
    int sx;
    int sy;

    /* coordinates of extrema of the rectangle */
    int ax, ay, zx, zy;

    /* Circle or rectangular */
    int rtype;
} Room;

static int roguelike_place_room(Room *Rooms, int xsize, int ysize, int nrooms);
static void roguelike_make_rooms(Room *Rooms, char **maze, int options);
static void roguelike_link_rooms(Room *Rooms, char **maze);

/**
 * Checks free spots around a spot.
 * @param layout Map layout.
 * @param i X coordinate to check.
 * @param j Y coordinate to check.
 * @param Xsize X size of the layout.
 * @param Ysize Y size of the layout.
 * @return Combination of the following values:
 * - <b>1</b>: Free space to left,
 * - <b>2</b>: Free space to right,
 * - <b>4</b>: Free space above
 * - <b>8</b>: Free space below */
int surround_check(char **layout, int i, int j, int Xsize, int Ysize)
{
    int surround_index = 0;

    if ((i > 0) && (layout[i - 1][j] != 0 && layout[i - 1][j] != '.')) {
        surround_index += 1;
    }

    if ((i < Xsize - 1) && (layout[i + 1][j] != 0 && layout[i + 1][j] != '.')) {
        surround_index += 2;
    }

    if ((j > 0) && (layout[i][j - 1] != 0 && layout[i][j - 1] != '.')) {
        surround_index += 4;
    }

    if ((j < Ysize - 1) && (layout[i][j + 1] != 0 && layout[i][j + 1] != '.')) {
        surround_index += 8;
    }

    return surround_index;
}

/**
 * Actually make the rogue layout. We work by a reduction process:
 *
 * first we make everything a wall, then we remove areas to make rooms.
 * @param xsize X size of the wanted layout size.
 * @param ysize Y size of the wanted layout size.
 * @param options 2 to have circular rooms, 1 for rectangular ones,
 * another value for random choice.
 * @return Generated layout. */
char **roguelike_layout_gen(int xsize, int ysize, int options)
{
    int i, j = 0;
    Room *Rooms = NULL, *walk;
    int nrooms = 0;
    int tries = 0;

    /* Allocate that array, write walls everywhere up */
    char **maze = emalloc(sizeof(char *) * xsize);

    for (i = 0; i < xsize; i++) {
        maze[i] = emalloc(sizeof(char) * ysize);

        for (j = 0; j < ysize; j++) {
            maze[i][j] = '#';
        }
    }

    /* minimum room size is basically 5x5:  if xsize/ysize is
     *    less than 3x that then hollow things out, stick in
     *    a stairsup and stairs down, and exit */
    if (xsize < 11 || ysize < 11) {
        for (i = 1; i < xsize - 1; i++) {
            for (j = 1; j < ysize - 1; j++) {
                maze[i][j] = 0;
            }
        }

        maze[i / 2][j / 2] = '>';
        maze[i / 2][j / 2 + 1] = '<';

        return maze;
    }

    /* decide on the number of rooms */
    nrooms = RANDOM() % 10 + 6;
    Rooms = ecalloc(nrooms + 1, sizeof(Room));

    /* Actually place the rooms */
    i = 0;

    while (tries < 450 && i < nrooms) {
        /* Try to place the room */
        if (!roguelike_place_room(Rooms, xsize, ysize, nrooms)) {
            tries++;
        } else {
            i++;
        }
    }

    /* no can do! */
    if (i == 0) {
        for (i = 1; i < xsize - 1; i++) {
            for (j = 1; j < ysize - 1; j++) {
                maze[i][j] = 0;
            }
        }

        maze[i / 2][j / 2] = '>';
        maze[i / 2][j / 2 + 1] = '<';
        efree(Rooms);

        return maze;
    }

    /* Erase the areas occupied by the rooms */
    roguelike_make_rooms(Rooms, maze, options);

    roguelike_link_rooms(Rooms, maze);

    /* Put in the stairs */
    maze[Rooms->x][Rooms->y] = '<';

    /* Get the last one */
    for (walk = Rooms; walk->x != 0; walk++) {
    }

    /* Back up one */
    walk--;

    maze[walk->x][walk->y] = '>';

    /* convert all the '.' to 0, we're through with the '.' */
    for (i = 0; i < xsize; i++) {
        for (j = 0; j < ysize; j++) {
            if (maze[i][j] == '.') {
                maze[i][j] = 0;
            }

            /* Remove bad door. */
            if (maze[i][j] == 'D') {
                int si = surround_check(maze, i, j, xsize, ysize);

                if (si != 3 && si != 12) {
                    maze[i][j] = 0;

                    /* back up and recheck any nearby doors */
                    i = 0;
                    j = 0;
                }
            }
        }
    }

    efree(Rooms);

    return maze;
}

/**
 * Place a room in the layout.
 * @param Rooms List of existing rooms, new room will be added to it.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @param nrooms Wanted number of room, used to determine size.
 * @return 0 if no room could be generated, 1 otherwise. */
static int roguelike_place_room(Room *Rooms, int xsize, int ysize, int nrooms)
{
    /* trial center locations */
    int tx, ty;
    /* trial sizes */
    int sx, sy;
    /* min coords of rect */
    int ax, ay;
    /* max coords of rect */
    int zx, zy;
    int x_basesize, y_basesize;
    Room *walk;

    /* Decide on the base x and y sizes */
    x_basesize = (int) (xsize / sqrt(nrooms));
    y_basesize = (int) (ysize / sqrt(nrooms));

    tx = RANDOM() % xsize;
    ty = RANDOM() % ysize;

    /* Generate a distribution of sizes centered about basesize */
    sx = (RANDOM() % x_basesize) + (RANDOM() % x_basesize)+ (RANDOM() % x_basesize);
    sy = (RANDOM() % y_basesize) + (RANDOM() % y_basesize)+ (RANDOM() % y_basesize);

    /* Renormalize */
    sy = (int) (sy * 0.5);

    /* Find the corners */
    ax = tx - sx / 2;
    zx = tx + sx / 2 + sx % 2;

    ay = ty - sy / 2;
    zy = ty + sy / 2 + sy % 2;

    /* Check to see if it's in the map */
    if (zx > xsize - 1 || ax < 1) {
        return 0;
    }

    if (zy > ysize - 1 || ay < 1) {
        return 0;
    }

    /* No small fish */
    if (sx < 3 || sy < 3) {
        return 0;
    }

    /* Check overlap with existing rooms */
    for (walk = Rooms; walk->x != 0; walk++) {
        int dx = abs(tx - walk->x), dy = abs(ty - walk->y);

        if ((dx < (walk->sx + sx) / 2 + 2) && (dy < (walk->sy + sy) / 2 + 2)) {
            return 0;
        }
    }

    /* If we've got here, presumably the room is OK. */

    /* Get a pointer to the first free room */
    for (walk = Rooms; walk->x != 0; walk++) {
    }

    walk->x = tx;
    walk->y = ty;
    walk->sx = sx;
    walk->sy = sy;
    walk->ax = ax;
    walk->ay = ay;
    walk->zx = zx;
    walk->zy = zy;

    /* success */
    return 1;
}

/**
 * Write all the rooms into the maze.
 * @param Rooms List of rooms to write.
 * @param maze Where to write to.
 * @param options 2 to have circular rooms, 1 for rectangular ones,
 * another value for random choice. */
static void roguelike_make_rooms(Room *Rooms, char **maze, int options)
{
    int making_circle = 0, i, j, R = 0;
    Room *walk;

    for (walk = Rooms; walk->x != 0; walk++) {
        /* First decide what shape to make */
        switch (options) {
        case 1:
            making_circle = 0;
            break;

        case 2:
            making_circle = 1;
            break;

        default:
            making_circle = ((RANDOM() % 3 == 0) ? 1 : 0);

            if (walk->sx < walk->sy) {
                R = walk->sx / 2;
            } else {
                R = walk->sy / 2;
            }
        }

        /* Enscribe a rectangle */
        for (i = walk->ax; i < walk->zx; i++) {
            for (j = walk->ay; j < walk->zy; j++) {
                if (!making_circle || ((int) (0.5 + hypot(walk->x - i, walk->y - j))) <= R) {
                    maze[i][j] = '.';
                }
            }
        }
    }
}

/**
 * Link generated rooms with corridors.
 * @param Rooms Room list.
 * @param maze Maze.
 * @param xsize X size of the maze.
 * @param ysize Y size of the maze. */
static void roguelike_link_rooms(Room *Rooms, char **maze)
{
    Room *walk;
    int i, j;

    /* Link each room to the previous room */
    if (Rooms[1].x == 0) {
        /* only 1 room */
        return;
    }

    for (walk = Rooms + 1; walk->x != 0; walk++) {
        int x = walk->x, y = walk->y, x2 = (walk - 1)->x, y2 = (walk - 1)->y, in_wall = 0;

        /* Connect in x direction first */
        if (RANDOM() % 2) {
            /* horizontal connect */
            /* swap (x1, y1) (x2, y2) if necessary */
            if (x2 < x) {
                int tx = x2, ty = y2;

                x2 = x;
                y2 = y;
                x = tx;
                y = ty;
            }

            j = y;

            for (i = x; i < x2; i++) {
                if (in_wall == 0 && maze[i][j] == '#') {
                    in_wall = 1;
                    maze[i][j] = 'D';
                } else if (in_wall && maze[i][j] == '.') {
                    in_wall = 0;
                    maze[i - 1][j] = 'D';
                } else if (maze[i][j] != 'D' && maze[i][j] != '.') {
                    maze[i][j] = 0;
                }
            }

            j = MIN(y, y2);

            if (maze[i][j] == '.') {
                in_wall = 0;
            }

            if (maze[i][j] == 0 || maze[i][j] == '#') {
                in_wall = 1;
            }

            for (; j < MAX(y, y2); j++) {
                if (in_wall == 0 && maze[i][j] == '#') {
                    in_wall = 1;
                    maze[i][j] = 'D';
                } else if (in_wall && maze[i][j] == '.') {
                    in_wall = 0;
                    maze[i][j - 1] = 'D';
                } else if (maze[i][j] != 'D' && maze[i][j] != '.') {
                    maze[i][j] = 0;
                }
            }
        }
        else {
            /* Connect in y direction first */

            in_wall = 0;

            /* Swap if necessary */
            if (y2 < y) {
                int tx = x2, ty = y2;

                x2 = x;
                y2 = y;
                x = tx;
                y = ty;
            }

            i = x;

            /* vertical connect */
            for (j = y; j < y2; j++) {
                if (in_wall == 0 && maze[i][j] == '#') {
                    in_wall = 1;
                    maze[i][j] = 'D';
                } else if (in_wall && maze[i][j] == '.') {
                    in_wall = 0;
                    maze[i][j - 1] = 'D';
                } else if (maze[i][j] != 'D' && maze[i][j] != '.') {
                    maze[i][j] = 0;
                }
            }

            i = MIN(x, x2);

            if (maze[i][j] == '.') {
                in_wall = 0;
            }

            if (maze[i][j] == 0 || maze[i][j] == '#') {
                in_wall = 1;
            }

            for (; i < MAX(x, x2); i++) {
                if (in_wall == 0 && maze[i][j] == '#') {
                    in_wall = 1;
                    maze[i][j] = 'D';
                } else if (in_wall && maze[i][j] == '.') {
                    in_wall = 0;
                    maze[i - 1][j] = 'D';
                } else if (maze[i][j] != 'D' && maze[i][j] != '.') {
                    maze[i][j] = 0;
                }
            }
        }
    }
}

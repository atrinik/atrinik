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
 * Square-spiral layout generator. */

#include <global.h>

/**
 * This starts from within a centered onion layer (or between two layers),
 * and looks up until it finds a wall, and then looks right until it
 * finds a vertical wall, i.e., the corner.  It sets cx and cy to that.
 * It also starts from cx and cy.
 * @param maze Where to look. */
void find_top_left_corner(char **maze, int *cx, int *cy)
{
    (*cy)--;

    /* Find the top wall. */
    while (maze[*cx][*cy] == '\0') {
        (*cy)--;
    }

    /* Proceed right until a corner is detected */
    while (maze[*cx][*cy + 1] == '\0') {
        (*cx)++;
    }

    /* cx and cy should now be the top-right corner of the onion layer */
}

/**
 * Generates a square-spiral layout.
 * @param xsize X size of the layout.
 * @param ysize Y size of the layout.
 * @return The generated layout. */
char **make_square_spiral_layout(int xsize, int ysize)
{
    int i, j;
    int cx = 0, cy = 0;
    int tx, ty;

    /* Generate and allocate a doorless, centered onion */
    char **maze = map_gen_onion(xsize, ysize, OPT_CENTERED | OPT_NO_DOORS, 0);

    /* Find the layout center.  */
    for (i = 0; i < xsize; i++) {
        for (j = 0; j < ysize; j++) {
            if (maze[i][j] == 'C') {
                cx = i;
                cy = j;
            }
        }
    }

    tx = cx;
    ty = cy;

    while (1) {
        find_top_left_corner(maze, &tx, &ty);

        if (ty < 2 || tx < 2 || tx > xsize - 2 || ty > ysize - 2) {
            break;
        }

        /* make a vertical wall with a door */
        make_wall(maze, tx, ty - 1, 1);

        /* convert the door that make_wall puts here to a wall */
        maze[tx][ty - 1] = '#';

        /* make a doorway out of this layer */
        maze[tx - 1][ty] = 'D';

        /* walk left until we find the top-left corner */
        while (maze[tx - 1][ty]) {
            tx--;
        }

        /* make a horizontal wall with a door */
        make_wall(maze, tx - 1, ty, 0);

        /* walk down until we find the bottom-left corner */
        while (maze[tx][ty + 1]) {
            ty++;
        }

        /* make a vertical wall with a door */
        make_wall(maze, tx, ty + 1, 1);

        /* walk rightuntil we find the bottom-right corner */
        while (maze[tx + 1][ty]) {
            tx++;
        }

        /* make a horizontal wall with a door */
        make_wall(maze, tx + 1, ty, 0);

        /* set up for next layer. */
        tx++;
    }

    /* place the exits.  */
    if (RANDOM() % 2) {
        maze[cx][cy] = '>';
        maze[xsize - 2][1] = '<';
    } else {
        maze[cx][cy] = '<';
        maze[xsize - 2][1] = '>';
    }

    return maze;
}

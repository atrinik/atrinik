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
 * The spiral room generator.
 */

#include <global.h>

/**
 * @defgroup SPIRAL_xxx Random spiral map options
 *@{
 */
/** Pick random options below */
#define RANDOM_OPTIONS  0
/** Regular spiral--distance increases constantly */
#define REGULAR_SPIRAL  1
/** Uses the min. separation:  most coiling */
#define FINE_SPIRAL     2
/** Scale to a rectangular region, not square */
#define FIT_SPIRAL      4
/** This should be 2x the last real option */
#define MAX_SPIRAL_OPT  8
/*@}*/

#define MINDIST 3

#define MAX_FINE .454545

/**
 * Generates a spiral layout.
 * @param xsize
 * X size of the layout.
 * @param ysize
 * Y size of the layout.
 * @param option
 * Combination of @ref SPIRAL_xxx "SPIRAL_xxx" values.
 * @return
 * The generated layout.
 */
char **map_gen_spiral(int xsize, int ysize, int option)
{
    int i, j, ic, jc;
    float parm = 0, x = 0, y = 0, SizeX, SizeY, xscale, yscale;

    /* Allocate that array, set it up */
    char **maze = ecalloc(sizeof(char *), xsize);

    for (i = 0; i < xsize; i++) {
        maze[i] = ecalloc(sizeof(char), ysize);
    }

    /* Slightly easier to fill and then cut */
    for (i = 0; i < xsize; i++) {
        for (j = 0; j < ysize; j++) {
            maze[i][j] = '#';
        }
    }

    ic = xsize / 2;
    jc = ysize / 2;

    SizeX = (float) xsize / 2.0f - 2.0f;
    SizeY = (float) ysize / 2.0f - 2.0f;

    /* Select random options if necessary */
    if (option == 0) {
        option = rndm(0, MAX_SPIRAL_OPT);
    }

    /* the order in which these are evaluated matters */

    /* the following two are mutually exclusive.
     *    pick one if they're both set. */
    if ((option & REGULAR_SPIRAL) && (option & FIT_SPIRAL)) {
        /* unset REGULAR_SPIRAL half the time */
        if (rndm_chance(2) && (option & REGULAR_SPIRAL)) {
            option -= REGULAR_SPIRAL;
        } else {
            option -= FIT_SPIRAL;
        }
    }

    /* fine spiral */
    xscale = yscale = (float) MAX_FINE;

    /* choose the spiral pitch */
    if (!(option & FINE_SPIRAL)) {
        float pitch = (float) (RANDOM() % 5) / 10.0f + 10.0f / 22.0f;

        xscale = yscale = pitch;
    }

    if ((option & FIT_SPIRAL) && (xsize != ysize)) {
        if (xsize > ysize) {
            xscale *= (float) xsize / (float) ysize;
        } else {
            yscale *= (float) ysize / (float) xsize;
        }
    }

    if (option & REGULAR_SPIRAL) {
        float scale = MIN(xscale, yscale);
        xscale = yscale = scale;
    }

    /* cut out the spiral */
    while ((FABS(x) < SizeX) && (FABS(y) < SizeY)) {
        x = parm * (float) cos((double) parm) * xscale;
        y = parm * (float) sin((double) parm) * yscale;

        maze[(int) (ic + x)][(int) (jc + y)] = '\0';

        parm += 0.01f;
    }

    maze[(int) (ic + x + 0.5)][(int) (jc + y + 0.5)] = '<';

    /* Cut out the center in a 2x2 and place the center and downexit */
    maze[ic][jc + 1] = '>';
    maze[ic][jc] = 'C';

    return maze;
}

/**
 * Connects disjoint spirals which may result from the symmetrization
 * process.
 * @param xsize
 * X size of the layout.
 * @param ysize
 * Y size of the layout.
 * @param sym
 * One of the @ref SYM_xxx "SYM_xxx" values.
 * @param layout
 * Layout to alter.
 */
void connect_spirals(int xsize, int ysize, int sym, char **layout)
{
    int i, j, ic = xsize / 2, jc = ysize / 2;

    if (sym == X_SYM) {
        layout[ic][jc] = 0;

        /* go left from map center */
        for (i = ic - 1, j = jc; i > 0 && layout[i][j] == '#'; i--) {
            layout[i][j] = 0;
        }

        /* go right */
        for (i = ic + 1, j = jc; i < xsize - 1 && layout[i][j] == '#'; i++) {
            layout[i][j] = 0;
        }
    }

    if (sym == Y_SYM) {
        layout[ic][jc] = 0;

        /* go up */
        for (i = ic, j = jc - 1; j > 0 && layout[i][j] == '#'; j--) {
            layout[i][j] = 0;
        }

        /* go down */
        for (i = ic, j = jc + 1; j < ysize - 1 && layout[i][j] == '#'; j++) {
            layout[i][j] = 0;
        }
    }

    if (sym == XY_SYM) {
        layout[ic][jc / 2] = 0;
        layout[ic / 2][jc] = 0;
        layout[ic][jc / 2 + jc] = 0;
        layout[ic / 2 + ic][jc] = 0;

        /* go left from map center */
        for (i = ic - 1, j = jc / 2; i > 0 && layout[i][j] == '#'; i--) {
            layout[i][j + jc] = 0;
            layout[i][j] = 0;
        }

        /* go right */
        for (i = ic + 1, j = jc / 2; i < xsize - 1 && layout[i][j] == '#'; i++) {
            layout[i][j + jc] = 0;
            layout[i][j] = 0;
        }

        /* go up */
        for (i = ic / 2, j = jc - 1; j > 0 && layout[i][j] == '#'; j--) {
            layout[i][j] = 0;
            layout[i + ic][j] = 0;
        }

        /* go down */
        for (i = ic / 2, j = jc + 1; j < ysize - 1 && layout[i][j] == '#'; j++) {
            layout[i][j] = 0;
            layout[i + ic][j] = 0;
        }
    }

    /* Get rid of bad doors. */
    for (i = 0; i < xsize; i++) {
        for (j = 0; j < ysize; j++) {
            /* Remove bad door. */
            if (layout[i][j] == 'D') {
                int si = surround_check(layout, i, j, xsize, ysize);

                if (si != 3 && si != 12) {
                    layout[i][j] = 0;

                    /* Back up and recheck any nearby doors */
                    i = 0;
                    j = 0;
                }
            }
        }
    }
}

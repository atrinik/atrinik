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

#include <global.h>

static void expand_misc(char **newlayout, int i, int j, char **layout);
static void expand_wall(char **newlayout, int i, int j, char **layout, int xsize, int ysize);
static void expand_door(char **newlayout, int i, int j, char **layout, int xsize, int ysize);

/**
 * Expands the layout be a factor 2. Doors and walls are taken care of.
 * @param layout Layout to expand. Memory is freed at the end, so pointer
 * becomes invalid.
 * @param xsize X layout size
 * @param ysize Y layout size
 * @return New layout. Must be freed by caller. */
char **expand2x(char **layout, int xsize, int ysize)
{
    int i,j;
    int nxsize = xsize * 2 - 1;
    int nysize = ysize * 2 - 1;

    /* Allocate new layout */
    char **newlayout = (char **) calloc(sizeof(char *), nxsize);

    for (i = 0; i < nxsize; i++) {
        newlayout[i] = (char *) calloc(sizeof(char), nysize);
    }

    for (i = 0; i < xsize; i++) {
        for (j = 0; j < ysize; j++) {
            switch (layout[i][j]) {
                case '#':
                    expand_wall(newlayout, i, j, layout, xsize, ysize);
                    break;

                case 'D':
                    expand_door(newlayout, i, j, layout, xsize, ysize);
                    break;

                default:
                    expand_misc(newlayout, i, j, layout);
            }
        }
    }

    /* Free old layout */
    for (i = 0; i < xsize; i++) {
        free(layout[i]);
    }

    free(layout);

    return newlayout;
}

/**
 * Copy the old tile X into the new one at location (i * 2, j * 2) and
 * fill up the rest of the 2x2 result with 0:
 * X ---> X  0
 *        0  0
 * @param newlayout Nap layout.
 * @param i X spot to expand.
 * @param j Y spot to expand.
 * @param layout Map layout.
 * @note No need to reset rest of 2x2 area to \0 because calloc does that
 * for us.*/
static void expand_misc(char **newlayout, int i, int j, char **layout)
{
    newlayout[i * 2][j * 2] = layout[i][j];
}

/**
 * Returns a bitmap that represents which squares on the right and bottom
 * edges of a square (i, j) match the given character.
 * @param ch Character to look for.
 * @param layout Map layout.
 * @param i X spot where to look.
 * @param j Y spot where to look.
 * @param xsize X layout size.
 * @param ysize Y layout size.
 * @return Combination of the following values:
 * - <b>1</b>: Match on (i + 1, j).
 * - <b>2</b>: Match on (i, j + 1).
 * - <b>4</b>: Match on (i + 1, j + 1). */
static int calc_pattern(char ch, char **layout, int i, int j, int xsize, int ysize)
{
    int pattern = 0;

    if (i + 1 < xsize && layout[i + 1][j] == ch) {
        pattern |= 1;
    }

    if (j + 1 < ysize) {
        if (layout[i][j + 1] == ch) {
            pattern |= 2;
        }

        if (i + 1 < xsize && layout[i + 1][j + 1] == ch) {
            pattern |= 4;
        }
    }

    return pattern;
}

/**
 * Expand a wall. This function will try to sensibly connect the
 * resulting wall to adjacent wall squares, so that the result won't have
 * disconnected walls.
 * @param newlayout Nap layout.
 * @param i X coordinate of wall to expand in non expanded layout.
 * @param j Y coordinate of wall to expand in non expanded layout.
 * @param layout Current (non expanded) layout.
 * @param xsize X size of layout.
 * @param ysize Y size of layout. */
static void expand_wall(char **newlayout, int i, int j, char **layout, int xsize, int ysize)
{
    int wall_pattern = calc_pattern('#', layout, i, j, xsize, ysize);
    int door_pattern = calc_pattern('D', layout, i, j, xsize, ysize);
    int both_pattern = wall_pattern | door_pattern;

    newlayout[i * 2][j * 2] = '#';

    if (i + 1 < xsize) {
        /* join walls/doors to the right */
        if (both_pattern & 1) {
            newlayout[i * 2 + 1][j * 2] = layout[i + 1][j];
        }
    }

    if (j + 1 < ysize) {
        /* join walls/doors to the bottom */
        if (both_pattern & 2) {
            newlayout[i * 2][j * 2 + 1] = layout[i][j + 1];
        }

        if (wall_pattern == 7) {
            /* if orig layout is a 2x2 wall block,
             * we fill the result with walls. */
            newlayout[i * 2 + 1][j * 2 + 1] = '#';
        }
    }
}

/**
 * Expand a door. This function will try to sensibly connect doors so
 * that they meet up with adjacent walls. Note that it will also
 * presumptuously delete (ignore) doors that it doesn't know how to
 * correctly expand.
 * @param newlayout Expanded layout.
 * @param i X coordinate of door to expand in non expanded layout.
 * @param j Y coordinate of door to expand in non expanded layout.
 * @param layout Non expanded layout.
 * @param xsize X size of the non expanded layout
 * @param ysize Y size of the non expanded layout */
static void expand_door(char **newlayout, int i, int j, char **layout, int xsize, int ysize)
{
    int wall_pattern = calc_pattern('#', layout, i, j, xsize, ysize);
    int door_pattern = calc_pattern('D', layout, i, j, xsize, ysize);
    int join_pattern;

    /* Doors "like" to connect to walls more than other doors. If there is
     * a wall and another door, this door will connect to the wall and
     * disconnect from the other door. */
    if (wall_pattern & 3) {
        join_pattern = wall_pattern;
    }
    else {
        join_pattern = door_pattern;
    }

    newlayout[i * 2][j * 2] = 'D';

    if (i + 1 < xsize) {
        /* There is a door/wall to the right */
        if (join_pattern & 1) {
            newlayout[i * 2 + 1][j * 2] = 'D';
        }
    }

    if (j + 1 < ysize) {
        /* There is a door/wall below */
        if (join_pattern & 2) {
            newlayout[i * 2][j * 2 + 1] = 'D';
        }
    }
}

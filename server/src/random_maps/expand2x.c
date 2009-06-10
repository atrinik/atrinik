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
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <stdlib.h>			/* just in case */
#include <expand2x.h>			/* use compiler to do sanity check */


/* PROTOTYPES */

static void expand_misc(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize);
static void expand_wall(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize);
static void expand_door(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize);


/* FUNCTIONS */

char **expand2x(char **layout, int xsize, int ysize) {
  int i,j;
  int nxsize = xsize*2 - 1;
  int nysize = ysize*2 - 1;

  /* Allocate new layout */
  char **newlayout = (char **)calloc(sizeof(char*), nxsize);
  for (i=0; i<nxsize; i++) {
    newlayout[i] = (char *) calloc(sizeof(char), nysize);
  }

  for (i=0; i<xsize; i++) {
    for (j=0; j<ysize; j++) {
      switch(layout[i][j]) {
      case '#':
        expand_wall(newlayout, i,j, layout, xsize, ysize);
        break;
      case 'D':
        expand_door(newlayout, i,j, layout, xsize, ysize);
        break;
      default:
        expand_misc(newlayout, i,j, layout, xsize, ysize);
      }
    }
  }

  /* Dump old layout */
  for (i=0; i<xsize; i++) {
    free(layout[i]);
  }
  free(layout);

  return newlayout;
}

/* Copy the old tile X into the new one at location (i*2, j*2) and
 * fill up the rest of the 2x2 result with \0:
 *	X  --->	X  \0
 *		\0 \0
 */
static void expand_misc(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize) {
  newlayout[i*2][j*2] = layout[i][j];
  /* (Note: no need to reset rest of 2x2 area to \0 because calloc does that
   * for us.) */
}

/* Returns a bitmap that represents which squares on the right and bottom
 * edges of a square (i,j) match the given character:
 *	1	match on (i+1, j)
 *	2	match on (i, j+1)
 *	4	match on (i+1, j+1)
 * and the possible combinations thereof.
 */
static int calc_pattern(char ch, char **layout, int i, int j,
                        int xsize, int ysize) {
  int pattern = 0;

  if (i+1<xsize && layout[i+1][j]==ch)
    pattern |= 1;

  if (j+1<ysize) {
    if (layout[i][j+1]==ch)
      pattern |= 2;
    if (i+1<xsize && layout[i+1][j+1]==ch)
      pattern |= 4;
  }

  return pattern;
}

/* Expand a wall. This function will try to sensibly connect the resulting
 * wall to adjacent wall squares, so that the result won't have disconnected
 * walls.
 */
static void expand_wall(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize) {
  int wall_pattern = calc_pattern('#', layout, i, j, xsize, ysize);
  int door_pattern = calc_pattern('D', layout, i, j, xsize, ysize);
  int both_pattern = wall_pattern | door_pattern;

  newlayout[i*2][j*2] = '#';
  if (i+1 < xsize) {
    if (both_pattern & 1) {		/* join walls/doors to the right */
/*      newlayout[i*2+1][j*2] = '#'; */
      newlayout[i*2+1][j*2] = layout[i+1][j];
    }
  }

  if (j+1 < ysize) {
    if (both_pattern & 2) {		/* join walls/doors to the bottom */
/*      newlayout[i*2][j*2+1] = '#'; */
      newlayout[i*2][j*2+1] = layout[i][j+1];
    }

    if (wall_pattern==7) {		/* if orig layout is a 2x2 wall block,
					 * we fill the result with walls. */
      newlayout[i*2+1][j*2+1] = '#';
    }
  }
}

/* This function will try to sensibly connect doors so that they meet up with
 * adjacent walls. Note that it will also presumptuously delete (ignore) doors
 * that it doesn't know how to correctly expand.
 */
static void expand_door(char **newlayout, int i, int j, char **layout,
                        int xsize, int ysize) {
  int wall_pattern = calc_pattern('#', layout, i, j, xsize, ysize);
  int door_pattern = calc_pattern('D', layout, i, j, xsize, ysize);
  int join_pattern;

  /* Doors "like" to connect to walls more than other doors. If there is
   * a wall and another door, this door will connect to the wall and
   * disconnect from the other door. */
  if (wall_pattern & 3) {
    join_pattern = wall_pattern;
  } else {
    join_pattern = door_pattern;
  }

  newlayout[i*2][j*2] = 'D';
  if (i+1 < xsize) {
    if (join_pattern & 1) {		/* there is a door/wall to the right */
      newlayout[i*2+1][j*2]='D';
    }
  }

  if (j+1 < ysize) {
    if (join_pattern & 2) {		/* there is a door/wall below */
      newlayout[i*2][j*2+1]='D';
    }
  }
}


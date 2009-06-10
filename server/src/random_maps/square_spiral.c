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

/* peterm@langmuir.eecs.berkeley.edu:  this function generates a random
snake-type layout.

input:  xsize, ysize;
output:  a char** array with # and . for closed and open respectively.

a char value of 0 represents a blank space:  a '#' is
a wall.

*/


#include <stdio.h>
#include <global.h>
#include <time.h>

#include <maze_gen.h>
#include <room_gen.h>
#include <random_map.h>
#include <sproto.h>
#include <rproto.h>


char **map_gen_onion(int xsize, int ysize, int option, int layers);


/* These are some helper functions which help with
   manipulating a centered onion and turning it into
   a square spiral */

/* this starts from within a centered onion layer (or between two layers),
   and looks up until it finds a wall, and then looks right until it
   finds a vertical wall, i.e., the corner.  It sets cx and cy to that.
   it also starts from cx and cy. */

void find_top_left_corner(char **maze,int *cx, int *cy) {

  (*cy)--;
  /* find the top wall. */
  while(maze[*cx][*cy]==0) (*cy)--;
  /* proceed right until a corner is detected */
  while(maze[*cx][*cy+1]==0) (*cx)++;

  /* cx and cy should now be the top-right corner of the onion layer */
}


char **make_square_spiral_layout(int xsize, int ysize,int options) {
  int i,j;
  int cx=0,cy=0;
  int tx,ty;

  /* generate and allocate a doorless, centered onion */
  char **maze = map_gen_onion(xsize,ysize,OPT_CENTERED | OPT_NO_DOORS,0);

  /* find the layout center.  */
  for(i=0;i<xsize;i++)
    for(j=0;j<ysize;j++) {
      if(maze[i][j]=='C' ) {
        cx = i; cy=j;
      }
    }
  tx = cx; ty = cy;
  while(1) {
    find_top_left_corner(maze,&tx,&ty);

    if(ty < 2 || tx < 2 || tx > xsize -2 || ty > ysize-2 ) break;
    make_wall(maze,tx,ty-1,1);  /* make a vertical wall with a door */

    maze[tx][ty-1]='#'; /* convert the door that make_wall puts here to a wall */
    maze[tx-1][ty]='D';/* make a doorway out of this layer */

    /* walk left until we find the top-left corner */
    while(maze[tx-1][ty]) tx--;

    make_wall(maze,tx-1,ty,0);     /* make a horizontal wall with a door */

    /* walk down until we find the bottom-left corner */
    while(maze[tx][ty+1]) ty++;

    make_wall(maze,tx,ty+1,1);    /* make a vertical wall with a door */

    /* walk rightuntil we find the bottom-right corner */
    while(maze[tx+1][ty]) tx++;

    make_wall(maze,tx+1,ty,0);   /* make a horizontal wall with a door */
    tx++;  /* set up for next layer. */
  }

  /* place the exits.  */

  if(RANDOM() %2) {
    maze[cx][cy]='>';
    maze[xsize-2][1]='<';
  }
  else {
    maze[cx][cy]='<';
    maze[xsize-2][1]='>';
  }

  return maze;
}




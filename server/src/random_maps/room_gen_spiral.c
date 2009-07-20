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


/*  The onion room generator:
Onion rooms are like this:

char **map_gen_spiral(int xsize, int ysize, int option);


*/
#include <global.h>
#include <random_map.h>

#define RANDOM_OPTIONS 0  /* Pick random options below */
#define REGULAR_SPIRAL 1  /* Regular spiral--distance increases constantly*/
#define FINE_SPIRAL 2     /* uses the min. separation:  most coiling */
#define FIT_SPIRAL 4      /* scale to a rectangular region, not square */
#define MAX_SPIRAL_OPT 8  /* this should be 2x the last real option */
#include <math.h>

/*
int *free_x_list;
int *free_y_list;
*/

#ifndef MIN
#define MIN(x,y) (((x)<(y))? (x):(y))
#endif
#ifndef MAX
#define MAX(x,y) (((x)<(y))? (y):(x))
#endif

#define MINDIST 3

#define MAX_FINE .454545

extern int surround_check(char **maze,int i, int j, int xsize, int ysize);

char **map_gen_spiral(int xsize, int ysize, int option) {
  int i,j;
  float parm=0;
  float x=0,y=0;
  int ic,jc;
  float SizeX,SizeY;
  float xscale,yscale;

  /* allocate that array, set it up */
  char **maze = (char **)calloc(sizeof(char*),xsize);
  for(i=0;i<xsize;i++) {
    maze[i] = (char *) calloc(sizeof(char),ysize);
  }

  /* slightly easier to fill and then cut */
  for(i=0;i<xsize;i++)  for(j=0;j<ysize;j++) maze[i][j] = '#';

  ic = xsize/2;
  jc = ysize/2;
  SizeX = (float) xsize/2.0f - 2.0f;
  SizeY = (float) ysize/2.0f - 2.0f;

  /* select random options if necessary */
  if(option==0) {
    option=RANDOM()%MAX_SPIRAL_OPT;
  }

  /* the order in which these are evaluated matters*/

  /* the following two are mutually exclusive.
     pick one if they're both set. */
  if((option & REGULAR_SPIRAL) && (option & FIT_SPIRAL))
    {
      /* unset REGULAR_SPIRAL half the time */
      if(RANDOM()%2 && (option & REGULAR_SPIRAL))
        option -= REGULAR_SPIRAL;
      else
        option -= FIT_SPIRAL;
    }

  xscale=yscale=(float)MAX_FINE;  /* fine spiral */

  /* choose the spiral pitch */
  if(! (option & FINE_SPIRAL) ) {
    float pitch = (float)(RANDOM() %5)/10.0f + 10.0f/22.0f;
    xscale=yscale=pitch;
  }

  if((option & FIT_SPIRAL) &&  (xsize!=ysize) ) {
    if(xsize > ysize) xscale *= (float)xsize/(float)ysize;
    else yscale *= (float)ysize/(float)xsize;
  }

  if(option & REGULAR_SPIRAL) {
    float scale = MIN(xscale,yscale);
    xscale=yscale=scale;
  }

  /* cut out the spiral */
  while( (FABS(x) < SizeX) && (FABS(y) < SizeY) )
    {
      x = parm * (float)cos((double)parm)*xscale;
      y = parm * (float)sin((double)parm)*yscale;
      maze[(int)(ic + x )][(int)(jc + y )]='\0';
      parm+=0.01f;
    };

maze[(int)(ic + x+0.5)][(int)(jc + y+0.5)]='<';


  /* cut out the center in a 2x2 and place the center and downexit */
  maze[ic][jc+1]='>';
  maze[ic][jc]='C';


  return maze;
}

/* the following function connects disjoint spirals which may
   result from the symmetrization process. */
void connect_spirals(int xsize,int ysize,int sym, char **layout) {

  int i,j,ic=xsize/2,jc=ysize/2;

  if(sym==X_SYM) {
    layout[ic][jc] = 0;
    /* go left from map center */
    for(i=ic-1,j=jc; i>0 && layout[i][j]=='#'  ;i--)
      layout[i][j]=0;
    /* go right */
    for(i=ic+1,j=jc; i<xsize-1 && layout[i][j]=='#'  ;i++)
      layout[i][j]=0;
  }

  if(sym==Y_SYM) {

    layout[ic][jc] = 0;
    /* go up */
    for(i=ic,j=jc-1; j>0 && layout[i][j]=='#'   ;j--)
      layout[i][j]=0;
    /* go down */
    for(i=ic,j=jc+1; j<ysize-1 && layout[i][j]=='#'  ;j++)
      layout[i][j]=0;
  }

  if(sym==XY_SYM) {
    /* go left from map center */
    layout[ic][jc/2]=0;
    layout[ic/2][jc]=0;
    layout[ic][jc/2+jc]=0;
    layout[ic/2+ic][jc]=0;
    for(i=ic-1,j=jc/2; i>0 && layout[i][j]=='#'  ;i--) {
      layout[i][j + jc]=0;
      layout[i][j]=0;
    }
    /* go right */
    for(i=ic+1,j=jc/2; i<xsize-1 && layout[i][j]=='#'  ;i++) {
      layout[i][j+jc]=0;
      layout[i][j]=0;
    }
    /* go up */
    for(i=ic/2,j=jc-1; j>0 && layout[i][j]=='#'   ;j--) {
      layout[i][j]=0;
      layout[i+ic][j]=0;
    }
    /* go down */
    for(i=ic/2,j=jc+1; j<ysize-1 && layout[i][j]=='#'  ;j++) {
      layout[i][j]=0;
      layout[i+ic][j]=0;
    }

  }

  /* get rid of bad doors. */
  for(i=0;i<xsize;i++)
    for(j=0;j<ysize;j++) {
      if(layout[i][j]=='D') {  /* remove bad door. */
        int si = surround_check(layout,i,j,xsize,ysize);
        if(si!=3 && si!=12) {
          layout[i][j]=0;
          /* back up and recheck any nearby doors */
          i=0;j=0;
        }
      }
    }



}


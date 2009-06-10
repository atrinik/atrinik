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

#include <stdio.h>
#include <global.h>
#include <expand2x.h>


/* this is a testing program for layouts.  It's
   included here for convenience only.  */
char **map_gen_spiral(int,int,int);
char **roguelike_layout_gen(int xsize, int ysize, int options);
char **make_snake_layout(int xsize, int ysize, int options );
char **make_square_spiral_layout(int xsize, int ysize, int options );
char **gen_corridor_rooms(int, int, int);

void dump_layout(char **layout, int Xsize, int Ysize) {
  int i,j;

  for(j=0;j<Ysize;j++) {
    for(i=0;i<Xsize;i++) {
      if(layout[i][j]==0) layout[i][j]=' ';
      printf("%c",layout[i][j]);
    }
    printf("\n");
  }
}

main() {
  int Xsize, Ysize;
  char **layout, **biglayout;
  SRANDOM(time(0));

  Xsize= RANDOM() %30 + 10;
  Ysize= RANDOM() %20 + 10;


  /* put your layout here */
  layout = roguelike_layout_gen(Xsize,Ysize,0);
  /*layout = make_snake_layout(Xsize,Ysize,0); */
  /*layout = make_square_spiral_layout(Xsize,Ysize,0); */
  /*layout = gen_corridor_rooms(Xsize, Ysize, 1); */
  /*layout = maze_gen(Xsize,Ysize,0); */
  /*layout = map_gen_onion(Xsize,Ysize,0,0);*/

  dump_layout(layout, Xsize, Ysize);
  printf("\nExpanding layout...\n");

  biglayout = expand2x(layout, Xsize, Ysize);
  dump_layout(biglayout, Xsize*2-1, Ysize*2-1);
}


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

/* we need to maintain a list of wall points to generate
   reasonable mazes:  a straightforward recursive random walk maze
   generator would generate a map with a trivial circle-the-outer-wall solution */

#include <stdio.h>
#include <global.h>
/*#include <random_map.h>*/
#include <maze_gen.h>
#include <time.h>


/* this include solely, and only, is needed for the definition of RANDOM */



/* global variables that everyone needs:  don't want to pass them in
   as parameters every time. */
int *wall_x_list=0;
int *wall_y_list=0;
int wall_free_size=0;

/*  heuristically, we need to change wall_chance based on the size of
    the maze. */

int wall_chance;

/* the outsize interface routine:  accepts sizes, returns a char
** maze.  option is a flag for either a sparse or a full maze. Sparse
mazes have sizable rooms. option = 1, full, 0, sparse.*/

char **maze_gen(int xsize, int ysize,int option)
{
	int i,j;

	/* allocate that array, set it up */
	char **maze = (char **)calloc(sizeof(char*),xsize);
	for (i=0;i<xsize;i++)
	{
		maze[i] = (char *) calloc(sizeof(char),ysize);
	}

	/* write the outer walls */
	for (i=0;i<xsize;i++)
		maze[i][0] = maze[i][ysize-1] = '#';
	for (j=0;j<ysize;j++)
		maze[0][j] = maze[xsize-1][j] = '#';


	/* find how many free wall spots there are */
	wall_free_size = 2 * (xsize-4) + 2*(ysize-4 );

	make_wall_free_list(xsize,ysize);

	/* return the empty maze */
	if (wall_free_size <=0 ) return maze;

	/* recursively generate the walls of the maze */
	/* first pop a random starting point */
	while (wall_free_size > 0)
	{
		pop_wall_point(&i,&j);
		if (option) fill_maze_full(maze,i,j,xsize,ysize);
		else fill_maze_sparse(maze,i,j,xsize,ysize);
	}

	/* clean up our intermediate data structures. */

	free(wall_x_list);
	free(wall_y_list);

	return maze;
}



/*  the free wall points are those outer points which aren't corners or
    near corners, and don't have a maze wall growing out of them already. */

void make_wall_free_list(int xsize, int ysize)
{
	int i,j,count;

	count = 0;  /* entries already placed in the free list */
	/*allocate it*/
	if (wall_free_size < 0) return;
	wall_x_list = (int *) calloc(sizeof(int),wall_free_size);
	wall_y_list = (int *) calloc(sizeof(int),wall_free_size);


	/* top and bottom wall */
	for (i = 2; i<xsize-2; i++)
	{
		wall_x_list[count] = i;
		wall_y_list[count] = 0;
		count++;
		wall_x_list[count] = i;
		wall_y_list[count] = ysize-1;
		count++;
	}

	/* left and right wall */
	for (j = 2; j<ysize-2; j++)
	{
		wall_x_list[count] = 0;
		wall_y_list[count] = j;
		count++;
		wall_x_list[count] = xsize-1;
		wall_y_list[count] = j;
		count++;
	}
}



/* randomly returns one of the elements from the wall point list */

void pop_wall_point(int *x,int *y)
{
	int index = RANDOM() % wall_free_size;
	*x = wall_x_list[index];
	*y = wall_y_list[index];
	/* write the last array point here */
	wall_x_list[index]=wall_x_list[wall_free_size-1];
	wall_y_list[index]=wall_y_list[wall_free_size-1];
	wall_free_size--;
}



/* find free point:  randomly look for a square adjacent to this one where
we can place a new block without closing a path.  We may only look
up, down, right, or left. */

int find_free_point(char **maze,int *x, int *y,int xc,int yc, int xsize, int ysize)
{

	/* we will randomly pick from this list, 1=up,2=down,3=right,4=left */
	int dirlist[4];
	int count = 0;  /* # elements in dirlist */

	/* look up */
	if (yc < ysize-2 && xc > 2 && xc < xsize-2) /* it is valid to look up */
	{
		int cleartest = (int) maze[xc][yc+1] + (int)maze[xc-1][yc+1]
						+ (int) maze[xc+1][yc+1];
		cleartest += (int) maze[xc][yc+2] + (int)maze[xc-1][yc+2]
					 + (int) maze[xc+1][yc+2];

		if (cleartest == 0)
		{
			dirlist[count] = 1;
			count++;
		}
	}


	/* look down */
	if (yc > 2 && xc > 2 && xc < xsize-2) /* it is valid to look down */
	{
		int cleartest = (int) maze[xc][yc-1] + (int)maze[xc-1][yc-1]
						+ (int) maze[xc+1][yc-1];
		cleartest += (int) maze[xc][yc-2] + (int)maze[xc-1][yc-2]
					 + (int) maze[xc+1][yc-2];

		if (cleartest == 0)
		{
			dirlist[count] = 2;
			count++;
		}
	}


	/* look right */
	if (xc < xsize- 2 && yc > 2 && yc < ysize-2) /* it is valid to look left */
	{
		int cleartest = (int) maze[xc+1][yc] + (int)maze[xc+1][yc-1]
						+ (int) maze[xc+1][yc+1];
		cleartest += (int) maze[xc+2][yc] + (int)maze[xc+2][yc-1]
					 + (int) maze[xc+2][yc+1];

		if (cleartest == 0)
		{
			dirlist[count] = 3;
			count++;
		}
	}


	/* look left */
	if (xc > 2 && yc > 2 && yc < ysize-2) /* it is valid to look down */
	{
		int cleartest = (int) maze[xc-1][yc] + (int)maze[xc-1][yc-1]
						+ (int) maze[xc-1][yc+1];
		cleartest += (int) maze[xc-2][yc] + (int)maze[xc-2][yc-1]
					 + (int) maze[xc-2][yc+1];

		if (cleartest == 0)
		{
			dirlist[count] = 4;
			count++;
		}
	}

	if (count==0) return -1; /* failed to find any clear points */

	/* choose a random direction */
	if (count > 1) count = RANDOM() % count;
	else count=0;
	switch (dirlist[count])
	{
		case 1: /* up */
		{
			*y = yc +1;
			*x = xc;
			break;
		};
		case 2: /* down */
		{
			*y = yc-1;
			*x = xc;
			break;
		};
		case 3: /* right */
		{
			*y = yc;
			*x = xc+1;
			break;
		}
		case 4: /* left */
		{
			*x = xc-1;
			*y = yc;
			break;
		}
		default: /* ??? */
		{
			return -1;
		}
	}
	return 1;
}

/* recursive routine which will fill every available space in the maze
	with walls*/

void fill_maze_full(char **maze, int x, int y, int xsize, int ysize )
{
	int xc,yc;

	/* write a wall here */
	maze[x][y] = '#';

	/* decide if we're going to pick from the wall_free_list */
	if (RANDOM()%4 && wall_free_size > 0)
	{
		pop_wall_point(&xc,&yc);
		fill_maze_full(maze,xc,yc,xsize,ysize);
	}

	/* change the if to a while for a complete maze.  */
	while (find_free_point(maze,&xc,&yc,x,y,xsize,ysize)!=-1)
	{
		fill_maze_full(maze,xc,yc,xsize,ysize);
	}
}


/* recursive routine which will fill much of the maze, but will leave
	some free spots (possibly large) toward the center.*/

void fill_maze_sparse(char **maze, int x, int y, int xsize, int ysize )
{
	int xc,yc;

	/* write a wall here */
	maze[x][y] = '#';

	/* decide if we're going to pick from the wall_free_list */
	if (RANDOM()%4 && wall_free_size > 0)
	{
		pop_wall_point(&xc,&yc);
		fill_maze_sparse(maze,xc,yc,xsize,ysize);
	}

	/* change the if to a while for a complete maze.  */
	if (find_free_point(maze,&xc,&yc,x,y,xsize,ysize)!=-1)
	{
		fill_maze_sparse(maze,xc,yc,xsize,ysize);
	}
}









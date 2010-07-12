/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

/**
 * @file
 * Line of sight related functions. */

#include <global.h>
#include <math.h>

/**
 * Distance must be less than this for the object to be blocked.
 * An object is 1.0 wide, so if set to 0.5, it means the object
 * that blocks half the view (0.0 is complete block) will
 * block view in our tables.
 * .4 or less lets you see through walls.  .5 is about right. */
#define SPACE_BLOCK	0.5

typedef struct blstr
{
	int x[4], y[4];
	int index;
} blocks;

static blocks block[MAP_CLIENT_X][MAP_CLIENT_Y];

/* lightning system */
#define MAX_MASK_SIZE 81
#define NR_LIGHT_MASK 10
#define MAX_LIGHT_SOURCE 13

static int lmask_x[MAX_MASK_SIZE] =
{
	0, 0, 1, 1, 1, 0, -1, -1, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1,
	0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -3, -3, -3, -3, -2, -1,
	0, 1, 2, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 2, 1, 0, -1, -2, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -2, -1
};

static int lmask_y[MAX_MASK_SIZE]=
{
	0, -1, -1, 0, 1, 1, 1, 0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2,
	-3, -3, -3, -3, -2, -1, 0, 1, 2, 3, 3, 3, 3, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3,
	4, 4, 4, 4, 4, 3, 2, 1, 0, -1, -2, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3, -2, -1, 0, 1, 2, 3, 4, 4, 4, 4
};

static int light_mask[MAX_LIGHT_SOURCE + 1] =
{
	0,
	1,
	2, 3,
	4, 5, 6, 6,
	7, 7, 8, 8,
	8, 9
};

static int light_mask_width[NR_LIGHT_MASK] =
{
	0, 1, 2, 2, 3,
	3, 3, 4, 4, 4
};

static int light_mask_size[NR_LIGHT_MASK] =
{
	0, 9, 25, 25, 49,
	49, 49, 81, 81, 81
};

static int light_masks[NR_LIGHT_MASK][MAX_MASK_SIZE] =
{
	{0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
	},
	{40,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{320,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{320,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	},
	{320,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	 20, 20, 20, 20, 20, 20, 20, 20,
	},
	{640,
	 320, 320, 320, 320, 320, 320, 320, 320,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	},
	{1280,
	 640, 640, 640, 640, 640, 640, 640, 640,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 160, 160, 160, 160, 160, 160, 160, 160,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 80, 80, 80, 80, 80, 80, 80, 80,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	 40, 40, 40, 40, 40, 40, 40, 40,
	}

};

static void expand_sight(object *op);

/**
 * Initializes the array used by the LOS routines.
 * This is NOT called for every LOS - only at server start to
 * init the base block structure.
 *
 * Since we are only doing the upper left quadrant, only
 * these spaces could possibly get blocked, since these
 * are the only ones further out that are still possibly in the
 * sightline. */
void init_block()
{
	int x, y, dx, dy, i;
	static const int block_x[3] = {-1, -1, 0}, block_y[3] = {-1, 0, -1};

	for (x = 0; x < MAP_CLIENT_X; x++)
	{
		for (y = 0; y < MAP_CLIENT_Y; y++)
		{
			block[x][y].index = 0;
		}
	}

	/* The table should be symmetric, so only do the upper left
	 * quadrant - makes the processing easier. */
	for (x = 1; x <= MAP_CLIENT_X / 2; x++)
	{
		for (y = 1; y <= MAP_CLIENT_Y / 2; y++)
		{
			for (i = 0; i < 3; i++)
			{
				dx = x + block_x[i];
				dy = y + block_y[i];

				/* Center space never blocks */
				if (x == MAP_CLIENT_X / 2 && y == MAP_CLIENT_Y / 2)
				{
					continue;
				}

				/* If its a straight line, it's blocked */
				if ((dx == x && x == MAP_CLIENT_X / 2) || (dy == y && y == MAP_CLIENT_Y / 2))
				{
					/* For simplicity, we mirror the coordinates to block the other
					 * quadrants. */
					set_block(x, y, dx, dy);

					if (x == MAP_CLIENT_X / 2)
					{
						set_block(x, MAP_CLIENT_Y - y -1, dx, MAP_CLIENT_Y - dy - 1);
					}
					else if (y == MAP_CLIENT_Y / 2)
					{
						set_block(MAP_CLIENT_X - x -1, y, MAP_CLIENT_X - dx - 1, dy);
					}
				}
				else
				{
					float d1, r, s, l;

					/* We use the algorihm that found out how close the point
					 * (x, y) is to the line from dx, dy to the center of the viewable
					 * area.  l is the distance from x, y to the line.
					 * r is more a curiosity - it lets us know what direction (left/right)
					 * the line is off */
					d1 = (float) (pow(MAP_CLIENT_X / 2 - dx, 2) + pow(MAP_CLIENT_Y / 2 - dy, 2));
					r = (float) ((dy - y) * (dy - MAP_CLIENT_Y / 2) - (dx - x) * (MAP_CLIENT_X / 2 - dx)) / d1;
					s = (float) ((dy - y) * (MAP_CLIENT_X / 2 - dx) - (dx - x) * (MAP_CLIENT_Y / 2 - dy)) / d1;
					l = (float) FABS(sqrt(d1) * s);

					if (l <= SPACE_BLOCK)
					{
						/* For simplicity, we mirror the coordinates to block the other
						 * quadrants. */
						set_block(x, y, dx, dy);
						set_block(MAP_CLIENT_X - x -1, y, MAP_CLIENT_X - dx - 1, dy);
						set_block(x, MAP_CLIENT_Y - y -1, dx, MAP_CLIENT_Y - dy - 1);
						set_block(MAP_CLIENT_X - x - 1, MAP_CLIENT_Y - y - 1, MAP_CLIENT_X - dx - 1, MAP_CLIENT_Y - dy - 1);
					}
				}
			}
		}
	}
}

/**
 * Used to initialize the array used by the LOS routines.
 * What this sets if that x, y blocks the view of bx, by
 * This then sets up a relation - for example, something
 * at 5, 4 blocks view at 5, 3 which blocks view at 5, 2
 * etc. So when we check 5, 4 and find it block, we have
 * the data to know that 5, 3 and 5, 2 and 5, 1 should
 * also be blocked.
 * @param x X position
 * @param y Y position
 * @param bx Blocked X position
 * @param by Blocked Y position */
void set_block(int x, int y, int bx, int by)
{
	int index = block[x][y].index, i;

	/* Due to flipping, we may get duplicates - better safe than sorry. */
	for (i = 0; i < index; i++)
	{
		if (block[x][y].x[i] == bx && block[x][y].y[i] == by)
		{
			return;
		}
	}

	block[x][y].x[index] = bx;
	block[x][y].y[index] = by;
	block[x][y].index++;

#ifdef LOS_DEBUG
	LOG(llevInfo, "DEBUG: setblock: added %d %d -> %d %d (%d)\n", x, y, bx, by, block[x][y].index);
#endif
}

/**
 * Used to initialize the array used by the LOS routines.
 * x, y are indexes into the blocked[][] array.
 * This recursively sets the blocked line of sight view.
 * From the blocked[][] array, we know for example
 * that if some particular space is blocked, it blocks
 * the view of the spaces 'behind' it, and those blocked
 * spaces behind it may block other spaces, etc.
 * In this way, the chain of visibility is set.
 * @param op Player object
 * @param x X position
 * @param y Y position */
static void set_wall(object *op, int x, int y)
{
	int i, xt, yt;

	xt = (MAP_CLIENT_X - CONTR(op)->socket.mapx) / 2;
	yt = (MAP_CLIENT_Y - CONTR(op)->socket.mapy) / 2;

	for (i = 0; i < block[x][y].index; i++)
	{
		int dx = block[x][y].x[i], dy = block[x][y].y[i], ax, ay;

		/* ax, ay are the values as adjusted to be in the
		 * socket look structure. */
		ax = dx - xt;
		ay = dy - yt;

		if (ax < 0 || ax >= CONTR(op)->socket.mapx || ay < 0 || ay >= CONTR(op)->socket.mapy)
		{
			continue;
		}

		/* We need to adjust to the fact that the socket
		 * code wants the los to start from the 0, 0
		 * and not be relative to middle of los array. */

		/* This tile can't be seen */
		if (!(CONTR(op)->blocked_los[ax][ay] & BLOCKED_LOS_OUT_OF_MAP))
		{
			CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKED;
		}

		set_wall(op, dx, dy);
	}
}

/**
 * Used to initialise the array used by the LOS routines.
 * Instead of light values, blocked_los[][] now tells the client
 * update function what kind of tile we have: visible, sight
 * blocked, blocksview trigger or out of map.
 * @param op The player object
 * @param x X position based on MAP_CLIENT_X
 * @param y Y position based on MAP_CLIENT_Y */
static void check_wall(object *op, int x, int y)
{
	int ax, ay, flags;

	/* ax, ay are coordinates as indexed into the look window */
	ax = x - (MAP_CLIENT_X - CONTR(op)->socket.mapx) / 2;
	ay = y - (MAP_CLIENT_Y - CONTR(op)->socket.mapy) / 2;

	/* this skips the "edges" of view area, the border tiles.
	 * Naturally, this tiles can't block any view - there is
	 * nothing behind them. */
	if (!block[x][y].index)
	{
		/* to handle the "blocksview update" right, we give this special
		 * tiles a "never use it to trigger a los_update()" flag.
		 * blockview changes to this tiles will have no effect. */

		/* mark the space as OUT_OF_MAP. */
		if (blocks_view(op->map,op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2) & P_OUT_OF_MAP)
		{
			CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
		}
		/* ignore means ignore for LOS */
		else
		{
			CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_IGNORE;
		}

		return;
	}


	/* If the converted coordinates are outside the viewable
	 * area for the client, return now. */
	if (ax < 0 || ay < 0 || ax >= CONTR(op)->socket.mapx || ay >= CONTR(op)->socket.mapy)
	{
		return;
	}

	/* If this space is already blocked, prune the processing - presumably
	 * whatever has set this space to be blocked has done the work and already
	 * done the dependency chain.
	 * but check for get_map_from_coord to speedup our client map draw function. */
	if (CONTR(op)->blocked_los[ax][ay] & (BLOCKED_LOS_BLOCKED | BLOCKED_LOS_OUT_OF_MAP))
	{
		if (CONTR(op)->blocked_los[ax][ay] & BLOCKED_LOS_BLOCKED)
		{
			if ((flags = blocks_view(op->map, op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2)))
			{
				/* mark the space as OUT_OF_MAP. */
				if (flags & P_OUT_OF_MAP)
				{
					CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
				}
				else
				{
					CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKSVIEW;
				}
			}
		}
		return;
	}

	if ((flags = blocks_view(op->map, op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2)))
	{
		set_wall(op, x, y);

		/* out of map clears all other flags! */

		/* Mark the space as OUT_OF_MAP. */
		if (flags & P_OUT_OF_MAP)
		{
			CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
		}
		else
		{
			CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKSVIEW;
		}
	}
}

/**
 * Sets all veiwable squares to blocked except for the central one that
 * the player occupies. A little odd that you can see yourself (and what
 * you're standing on), but really need for any reasonable game play.
 * @param op Player's object for which to reset los. */
static void blinded_sight(object *op)
{
	int x, y;

	for (x = 0; x < CONTR(op)->socket.mapx; x++)
	{
		for (y = 0; y < CONTR(op)->socket.mapy; y++)
		{
			CONTR(op)->blocked_los[x][y] |= BLOCKED_LOS_BLOCKED;
		}
	}

	CONTR(op)->blocked_los[CONTR(op)->socket.mapx / 2][CONTR(op)->socket.mapy / 2] &= ~BLOCKED_LOS_BLOCKED;
}

/**
 * Recalculates the array which specifies what is visible
 * for the given player object.
 * @param op The player object */
void update_los(object *op)
{
	int dx = CONTR(op)->socket.mapx_2, dy = CONTR(op)->socket.mapy_2, x, y;

	if (QUERY_FLAG(op, FLAG_REMOVED))
	{
		return;
	}

#ifdef DEBUG_CORE
	LOG(llevDebug, "LOS - %s\n", query_name(op));
#endif

	clear_los(op);

	if (QUERY_FLAG(op, FLAG_WIZ))
	{
		return;
	}

	/* For larger maps, this is more efficient than the old way which
	 * used the chaining of the block array.  Since many space views could
	 * be blocked by different spaces in front, this mean that a lot of spaces
	 * could be examined multile times, as each path would be looked at. */
	for (x = (MAP_CLIENT_X - CONTR(op)->socket.mapx) / 2; x < (MAP_CLIENT_X + CONTR(op)->socket.mapx) / 2; x++)
	{
		for (y = (MAP_CLIENT_Y - CONTR(op)->socket.mapy) / 2; y < (MAP_CLIENT_Y + CONTR(op)->socket.mapy) / 2; y++)
		{
			check_wall(op, x, y);
		}
	}

	if (QUERY_FLAG(op, FLAG_BLIND))
	{
		blinded_sight(op);
	}
	else
	{
		expand_sight(op);

		/* Give us an area we can look through when we have xray - this
		 * stacks to normal LOS. */
		if (QUERY_FLAG(op, FLAG_XRAYS))
		{
			int x, y;

			for (x = -3; x <= 3; x++)
			{
				for (y = -3; y <= 3; y++)
				{
					if (CONTR(op)->blocked_los[dx + x][dy + y] & BLOCKED_LOS_BLOCKED)
					{
						CONTR(op)->blocked_los[dx + x][dy + y] &= ~BLOCKED_LOS_BLOCKED;
					}
				}
			}
		}
	}
}

/**
 * Clears/initializes the LOS array associated to the player
 * controlling the object.
 * @param op The player object. */
void clear_los(object *op)
{
	(void) memset((void *) CONTR(op)->blocked_los, BLOCKED_LOS_VISIBLE, sizeof(CONTR(op)->blocked_los));
}

#define BLOCKED_LOS_EXPAND 0x20

/**
 * This goes through the array of what the given player is able to
 * see, and expands the visible area a bit, so the player will, to
 * a certain degree, be able to see into corners.
 * This is somewhat suboptimal, would be better to improve the
 * formula.
 * @param op The player object.
 * @todo Improve the formula. */
static void expand_sight(object *op)
{
	int i, x, y, dx, dy;

	/* loop over inner squares */
	for (x = 1; x < CONTR(op)->socket.mapx - 1; x++)
	{
		for (y = 1; y < CONTR(op)->socket.mapy - 1; y++)
		{
			/* if visible and not blocksview */
			if (CONTR(op)->blocked_los[x][y] <= BLOCKED_LOS_BLOCKSVIEW && !(CONTR(op)->blocked_los[x][y] & BLOCKED_LOS_BLOCKSVIEW))
			{
				/* mark all directions */
				for (i = 1; i <= 8; i += 1)
				{
					dx = x + freearr_x[i];
					dy = y + freearr_y[i];

					if (dx < 0 || dy < 0 || dx > CONTR(op)->socket.mapx || dy > CONTR(op)->socket.mapy)
					{
						continue;
					}

					if (CONTR(op)->blocked_los[dx][dy] & BLOCKED_LOS_BLOCKED)
					{
						CONTR(op)->blocked_los[dx][dy] |= BLOCKED_LOS_EXPAND;
					}
				}
			}
		}
	}

	for (x = 0; x < CONTR(op)->socket.mapx; x++)
	{
		for (y = 0; y < CONTR(op)->socket.mapy; y++)
		{
			if (CONTR(op)->blocked_los[x][y] & BLOCKED_LOS_EXPAND)
			{
				CONTR(op)->blocked_los[x][y] &= ~(BLOCKED_LOS_BLOCKED | BLOCKED_LOS_EXPAND);
			}
		}
	}
}

static int get_real_light_source_value(int l)
{
	if (l > MAX_LIGHT_SOURCE)
		return light_mask[MAX_LIGHT_SOURCE];

	if (l < -MAX_LIGHT_SOURCE)
		return -light_mask[MAX_LIGHT_SOURCE];

	if (l < 0)
		return -light_mask[-l];

	return light_mask[l];
}

static void remove_light_mask(mapstruct *map, int x, int y, int mid)
{
	MapSpace *msp;
	mapstruct *m;
	int xt, yt, i, mlen;

	/* Light masks */
	if (mid > 0)
	{
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];

			if (!(m = get_map_from_coord2(map, &xt, &yt)))
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value -= light_masks[mid][i];
		}
	}
	/* Handle shadow mask */
	else
	{
		mid = -mid;
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];

			if (!(m = get_map_from_coord2(map, &xt, &yt)))
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value += light_masks[mid][i];
		}
	}
}

static int add_light_mask(mapstruct *map, int x, int y, int mid)
{
	MapSpace *msp;
	mapstruct *m;
	int xt, yt, i, mlen, map_flag = 0;

	if (mid > 0)
	{
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];

			if (!(m = get_map_from_coord2(map, &xt, &yt)))
			{
				if (xt)
				{
					map_flag = 1;
				}

				continue;
			}

			/* This light mask crosses some tiled map borders */
			if (m != map)
			{
				map_flag = 1;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value += light_masks[mid][i];
		}
	}
	/* Shadow masks */
	else
	{
		mid = -mid;
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];

			if (!(m = get_map_from_coord2(map, &xt, &yt)))
			{
				if (xt)
				{
					map_flag = 1;
				}

				continue;
			}

			/* This light mask crosses some tiled map borders */
			if (m != map)
			{
				map_flag = 1;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value -= light_masks[mid][i];
		}
	}

	return map_flag;
}

/**
 * Add or remove a light source to a map space.
 * Adjust the light source map counter and apply
 * the area of light it invokes around it.
 * @param map The map of this light
 * @param x X position of light
 * @param y Y position of light
 * @param light Glow radius of the light */
void adjust_light_source(mapstruct *map, int x, int y, int light)
{
	int nlm, olm;
	MapSpace *msp1 = GET_MAP_SPACE_PTR(map,x,y);

	/* this happens, we don't change the intense of the old light mask */
	/* old mask */
	olm = get_real_light_source_value(msp1->light_source);
	msp1->light_source += light;
	/* new mask */
	nlm = get_real_light_source_value(msp1->light_source);

	/* Old mask same as new one? not much to do */
	if (nlm == olm)
	{
		return;
	}

	if (olm)
	{
		/* Remove the old light mask */
		remove_light_mask(map, x, y, olm);

		/* Perhaps we are in this list - perhaps we are not */
		if (msp1->prev_light)
		{
			msp1->prev_light->next_light = msp1->next_light;
		}
		else
		{
			/* We are the list head */
			if (map->first_light == msp1)
			{
				map->first_light = msp1->next_light;
			}
		}

		/* Handle next link */
		if (msp1->next_light)
		{
			msp1->next_light->prev_light = msp1->prev_light;
		}

		msp1->prev_light = NULL;
		msp1->next_light = NULL;
	}

	if (nlm)
	{
		/* Add new light mask */
		if (add_light_mask(map, x, y, nlm))
		{
			/* Don't chain if we are chained previous */
			if (msp1->next_light || msp1->prev_light || map->first_light == msp1)
			{
				return;
			}

			/* We should be always unlinked here - so link it now */
			msp1->next_light = map->first_light;

			if (map->first_light)
			{
				msp1->next_light->prev_light = msp1;
			}

			map->first_light = msp1;
		}
	}
}

/**
 * After loading a map, we check here all possible connected
 * maps for overlapping light sources. When we find one, we
 * add the overlapping area to our new loaded map.
 * @param restore_map Map to restore from
 * @param map Map
 * @param x X position
 * @param y Y position
 * @param mid Light value */
static void restore_light_mask(mapstruct *restore_map, mapstruct *map, int x, int y, int mid)
{
	MapSpace *msp;
	mapstruct *m;
	int xt, yt, i, mlen;

	if (mid > 0)
	{
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];

			m = get_map_from_coord2(map, &xt ,&yt);

			/* Only handle parts inside our calling (restore) map */
			if (restore_map != m)
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value += light_masks[mid][i];
		}
	}
	/* Shadow masks */
	else
	{
		mid = -mid;
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];
			m = get_map_from_coord2(map, &xt, &yt);

			/* Only handle parts inside our calling (restore) map */
			if (restore_map != m)
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value -= light_masks[mid][i];
		}
	}
}

/**
 * Check light source list of specified map.
 * This will also check all tiled maps attached
 * to the map.
 * @param map The map to check. */
void check_light_source_list(mapstruct *map)
{
	mapstruct *t_map;
	MapSpace *tmp;
	int x, y, mid;

	if ((t_map = map->tile_map[0]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if (y + light_mask_width[abs(mid)] < MAP_HEIGHT(t_map))
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[1]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if (x - light_mask_width[abs(mid)] >= 0)
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[2]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if (y - light_mask_width[abs(mid)] >= 0)
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[3]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if (x + light_mask_width[abs(mid)] < MAP_WIDTH(t_map))
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[4]) && (t_map->in_memory==MAP_IN_MEMORY || t_map->in_memory==MAP_LOADING) &&  t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid=get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* only light sources reaching in this map */
			if ((y + light_mask_width[abs(mid)]) < MAP_HEIGHT(t_map) || (x - light_mask_width[abs(mid)]) >= 0)
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[5]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if ((x - light_mask_width[abs(mid)]) >= 0 || (y - light_mask_width[abs(mid)]) >= 0)
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[6]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if ((y - light_mask_width[abs(mid)]) >= 0 || (x + light_mask_width[abs(mid)]) < MAP_WIDTH(t_map))
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}

	if ((t_map = map->tile_map[7]) && (t_map->in_memory == MAP_IN_MEMORY || t_map->in_memory == MAP_LOADING) && t_map->first_light)
	{
		/* Check this light source list */
		for (tmp = t_map->first_light; tmp; tmp = tmp->next_light)
		{
			if (!tmp->first)
			{
				LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", t_map->path ? t_map->path : "NO T_MAP PATH?");
				continue;
			}

			mid = get_real_light_source_value(tmp->light_source);
			/* There MUST be at least one object in this tile - grab it and
			 * get the x/y offset from it! */
			x = tmp->first->x;
			y = tmp->first->y;

			/* Only light sources reaching in this map */
			if ((y + light_mask_width[abs(mid)]) < MAP_HEIGHT(t_map) || (x + light_mask_width[abs(mid)]) < MAP_WIDTH(t_map))
			{
				continue;
			}

			restore_light_mask(map, t_map, x, y, mid);
		}
	}
}

/* Remove mask part from OTHER in memory maps! */
static void remove_light_mask_other(mapstruct *map, int x, int y, int mid)
{
	MapSpace *msp;
	mapstruct *m;
	int xt, yt, i, mlen;

	if (mid > 0)
	{
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];
			m = get_map_from_coord2(map, &xt, &yt);

			/* Only legal OTHER maps */
			if (!m || m == map)
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value -= light_masks[mid][i];
		}
	}
	else
	{
		mid = -mid;
		mlen = light_mask_size[mid];

		for (i = 0; i < mlen; i++)
		{
			xt = x + lmask_x[i];
			yt = y + lmask_y[i];
			m = get_map_from_coord2(map, &xt, &yt);

			/* Only legal OTHER maps */
			if (!m || m == map)
			{
				continue;
			}

			msp = GET_MAP_SPACE_PTR(m, xt, yt);
			msp->light_value += light_masks[mid][i];
		}
	}
}

/**
 * Remove light sources list from a map.
 * @param map The map to remove from */
void remove_light_source_list(mapstruct *map)
{
	MapSpace *tmp;

	for (tmp = map->first_light; tmp; tmp = tmp->next_light)
	{
		/* There MUST be at least ONE object in this map space */
		if (!tmp->first)
		{
			LOG(llevBug, "BUG: remove_light_source_list() map:>%s< - no object in mapspace of light source!\n", map->path ? map->path : "NO MAP PATH?");
			continue;
		}

		remove_light_mask_other(map, tmp->first->x, tmp->first->y, get_real_light_source_value(tmp->light_source));
	}

	map->first_light = NULL;
}

/**
 * Check if object is in line of sight, based on rv_vector.
 * This will check also check for blocksview 1 objects.
 *
 * Uses the Bresenham line algorithm.
 * @param obj The object to check
 * @param rv rv_vector to get distances, map, etc from
 * @return 1 if in line of sight, 0 otherwise */
int obj_in_line_of_sight(object *obj, rv_vector *rv)
{
	/* Bresenham variables */
	int fraction, dx2, dy2, stepx, stepy;
	/* Stepping variables */
	mapstruct *m = rv->part->map;
	int x = rv->part->x, y = rv->part->y;

	BRESENHAM_INIT(rv->distance_x, rv->distance_y, fraction, stepx, stepy, dx2, dy2);

	while (1)
	{
		if (x == obj->x && y == obj->y && m == obj->map)
		{
			return 1;
		}

		if (m == NULL || GET_MAP_FLAGS(m, x, y) & P_BLOCKSVIEW)
		{
			return 0;
		}

		BRESENHAM_STEP(x, y, fraction, stepx, stepy, dx2, dy2);

		m = get_map_from_coord(m, &x, &y);
	}
}

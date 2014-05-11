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

/**
 * @file
 * Line of sight related functions. */

#include <global.h>

/**
 * Distance must be less than this for the object to be blocked.
 * An object is 1.0 wide, so if set to 0.5, it means the object
 * that blocks half the view (0.0 is complete block) will
 * block view in our tables.
 * .4 or less lets you see through walls.  .5 is about right. */
#define SPACE_BLOCK 0.5

typedef struct blstr
{
    int x[4], y[4];
    int index;
} blocks;

static blocks block[MAP_CLIENT_X][MAP_CLIENT_Y];

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
void init_block(void)
{
    int x, y, dx, dy, i;
    static const int block_x[3] = {-1, -1, 0}, block_y[3] = {-1, 0, -1};

    for (x = 0; x < MAP_CLIENT_X; x++) {
        for (y = 0; y < MAP_CLIENT_Y; y++) {
            block[x][y].index = 0;
        }
    }

    /* The table should be symmetric, so only do the upper left
     * quadrant - makes the processing easier. */
    for (x = 1; x <= MAP_CLIENT_X / 2; x++) {
        for (y = 1; y <= MAP_CLIENT_Y / 2; y++) {
            for (i = 0; i < 3; i++) {
                dx = x + block_x[i];
                dy = y + block_y[i];

                /* Center space never blocks */
                if (x == MAP_CLIENT_X / 2 && y == MAP_CLIENT_Y / 2) {
                    continue;
                }

                /* If its a straight line, it's blocked */
                if ((dx == x && x == MAP_CLIENT_X / 2) || (dy == y && y == MAP_CLIENT_Y / 2)) {
                    /* For simplicity, we mirror the coordinates to block the
                     * other
                     * quadrants. */
                    set_block(x, y, dx, dy);

                    if (x == MAP_CLIENT_X / 2) {
                        set_block(x, MAP_CLIENT_Y - y -1, dx, MAP_CLIENT_Y - dy - 1);
                    }
                    else if (y == MAP_CLIENT_Y / 2) {
                        set_block(MAP_CLIENT_X - x -1, y, MAP_CLIENT_X - dx - 1, dy);
                    }
                }
                else {
                    float d1, s, l;

                    /* We use the algorithm that found out how close the point
                     * (x, y) is to the line from dx, dy to the center of the
                     * viewable
                     * area.  l is the distance from x, y to the line.
                     * r is more a curiosity - it lets us know what direction
                     * (left/right)
                     * the line is off */
                    d1 = (float) (pow(MAP_CLIENT_X / 2 - dx, 2) + pow(MAP_CLIENT_Y / 2 - dy, 2));
                    s = (float) ((dy - y) * (MAP_CLIENT_X / 2 - dx) - (dx - x) * (MAP_CLIENT_Y / 2 - dy)) / d1;
                    l = (float) FABS(sqrt(d1) * s);

                    if (l <= SPACE_BLOCK) {
                        /* For simplicity, we mirror the coordinates to block
                         * the other
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
    int idx = block[x][y].index, i;

    /* Due to flipping, we may get duplicates - better safe than sorry. */
    for (i = 0; i < idx; i++) {
        if (block[x][y].x[i] == bx && block[x][y].y[i] == by) {
            return;
        }
    }

    block[x][y].x[idx] = bx;
    block[x][y].y[idx] = by;
    block[x][y].index++;
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

    for (i = 0; i < block[x][y].index; i++) {
        int dx = block[x][y].x[i], dy = block[x][y].y[i], ax, ay;

        /* ax, ay are the values as adjusted to be in the
         * socket look structure. */
        ax = dx - xt;
        ay = dy - yt;

        if (ax < 0 || ax >= CONTR(op)->socket.mapx || ay < 0 || ay >= CONTR(op)->socket.mapy) {
            continue;
        }

        /* We need to adjust to the fact that the socket
        * code wants the los to start from the 0, 0
        * and not be relative to middle of los array. */

        /* This tile can't be seen */
        if (!(CONTR(op)->blocked_los[ax][ay] & BLOCKED_LOS_OUT_OF_MAP)) {
            CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKED;
        }

        set_wall(op, dx, dy);
    }
}

/**
 * Used to initialize the array used by the LOS routines.
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
    if (!block[x][y].index) {
        /* to handle the "blocksview update" right, we give this special
         * tiles a "never use it to trigger a los_update()" flag.
         * blockview changes to this tiles will have no effect. */

        /* mark the space as OUT_OF_MAP. */
        if (blocks_view(op->map,op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2) & P_OUT_OF_MAP) {
            CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
        }
        /* ignore means ignore for LOS */
        else {
            CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_IGNORE;
        }

        return;
    }


    /* If the converted coordinates are outside the viewable
     * area for the client, return now. */
    if (ax < 0 || ay < 0 || ax >= CONTR(op)->socket.mapx || ay >= CONTR(op)->socket.mapy) {
        return;
    }

    /* If this space is already blocked, prune the processing - presumably
     * whatever has set this space to be blocked has done the work and already
     * done the dependency chain.
     * but check for get_map_from_coord to speedup our client map draw function.
     * */
    if (CONTR(op)->blocked_los[ax][ay] & (BLOCKED_LOS_BLOCKED | BLOCKED_LOS_OUT_OF_MAP)) {
        if (CONTR(op)->blocked_los[ax][ay] & BLOCKED_LOS_BLOCKED) {
            if ((flags = blocks_view(op->map, op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2))) {
                /* mark the space as OUT_OF_MAP. */
                if (flags & P_OUT_OF_MAP) {
                    CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
                }
                else {
                    CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKSVIEW;
                }
            }
        }
        return;
    }

    if ((flags = blocks_view(op->map, op->x + x - MAP_CLIENT_X / 2, op->y + y - MAP_CLIENT_Y / 2))) {
        set_wall(op, x, y);

        /* out of map clears all other flags! */

        /* Mark the space as OUT_OF_MAP. */
        if (flags & P_OUT_OF_MAP) {
            CONTR(op)->blocked_los[ax][ay] = BLOCKED_LOS_OUT_OF_MAP;
        }
        else {
            CONTR(op)->blocked_los[ax][ay] |= BLOCKED_LOS_BLOCKSVIEW;
        }
    }
}

/**
 * Sets all viewable squares to blocked except for the central one that
 * the player occupies. A little odd that you can see yourself (and what
 * you're standing on), but really need for any reasonable game play.
 * @param op Player's object for which to reset los. */
static void blinded_sight(object *op)
{
    int x, y;

    for (x = 0; x < CONTR(op)->socket.mapx; x++) {
        for (y = 0; y < CONTR(op)->socket.mapy; y++) {
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

    if (QUERY_FLAG(op, FLAG_REMOVED)) {
        return;
    }

    clear_los(op);

    if (CONTR(op)->tls) {
        return;
    }

    /* For larger maps, this is more efficient than the old way which
     * used the chaining of the block array.  Since many space views could
     * be blocked by different spaces in front, this mean that a lot of spaces
     * could be examined multile times, as each path would be looked at. */
    for (x = (MAP_CLIENT_X - CONTR(op)->socket.mapx) / 2; x < (MAP_CLIENT_X + CONTR(op)->socket.mapx) / 2; x++) {
        for (y = (MAP_CLIENT_Y - CONTR(op)->socket.mapy) / 2; y < (MAP_CLIENT_Y + CONTR(op)->socket.mapy) / 2; y++) {
            check_wall(op, x, y);
        }
    }

    if (QUERY_FLAG(op, FLAG_BLIND)) {
        blinded_sight(op);
    }
    else {
        expand_sight(op);

        /* Give us an area we can look through when we have xray - this
         * stacks to normal LOS. */
        if (QUERY_FLAG(op, FLAG_XRAYS)) {
            for (x = -3; x <= 3; x++) {
                for (y = -3; y <= 3; y++) {
                    if (CONTR(op)->blocked_los[dx + x][dy + y] & BLOCKED_LOS_BLOCKED) {
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
    (void) memset(CONTR(op)->blocked_los, BLOCKED_LOS_VISIBLE, sizeof(CONTR(op)->blocked_los));
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
    for (x = 1; x < CONTR(op)->socket.mapx - 1; x++) {
        for (y = 1; y < CONTR(op)->socket.mapy - 1; y++) {
            /* if visible and not blocksview */
            if (CONTR(op)->blocked_los[x][y] <= BLOCKED_LOS_BLOCKSVIEW && !(CONTR(op)->blocked_los[x][y] & BLOCKED_LOS_BLOCKSVIEW)) {
                /* mark all directions */
                for (i = 1; i <= 8; i += 1) {
                    dx = x + freearr_x[i];
                    dy = y + freearr_y[i];

                    if (dx < 0 || dy < 0 || dx > CONTR(op)->socket.mapx || dy > CONTR(op)->socket.mapy) {
                        continue;
                    }

                    if (CONTR(op)->blocked_los[dx][dy] & BLOCKED_LOS_BLOCKED) {
                        CONTR(op)->blocked_los[dx][dy] |= BLOCKED_LOS_EXPAND;
                    }
                }
            }
        }
    }

    for (x = 0; x < CONTR(op)->socket.mapx; x++) {
        for (y = 0; y < CONTR(op)->socket.mapy; y++) {
            if (CONTR(op)->blocked_los[x][y] & BLOCKED_LOS_EXPAND) {
                CONTR(op)->blocked_los[x][y] &= ~(BLOCKED_LOS_BLOCKED | BLOCKED_LOS_EXPAND);
            }
        }
    }
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

    while (1) {
        if (x == obj->x && y == obj->y && m == obj->map) {
            return 1;
        }

        if (m == NULL || GET_MAP_FLAGS(m, x, y) & P_BLOCKSVIEW) {
            return 0;
        }

        BRESENHAM_STEP(x, y, fraction, stepx, stepy, dx2, dy2);

        m = get_map_from_coord(m, &x, &y);
    }
}

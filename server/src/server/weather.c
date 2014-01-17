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
 * This file controls weather functions, like ticking the clock
 * and initializing the world darkness. */

#include <global.h>

const int season_timechange[SEASONS_PER_YEAR][HOURS_PER_DAY] =
{
    {0, 0, 0, 0, -1, -1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, -1, -1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 0, 0},
    {0, 0, 0, 0, 0, -1, -1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, -1, -1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 0}
};

/**
 * Initializes the world darkness value. */
void init_world_darkness(void)
{
    int i;
    timeofday_t tod;

    world_darkness = MAX_DARKNESS;
    get_tod(&tod);

    for (i = HOURS_PER_DAY / 2; i < HOURS_PER_DAY; i++) {
        world_darkness -= season_timechange[tod.season][i];
    }

    for (i = 0; i < tod.hour + 1; i++) {
        world_darkness -= season_timechange[tod.season][i];
    }
}

/**
 * This performs the basic function of advancing the clock one tick
 * forward. Every 20 ticks, the clock is saved to disk. It is also
 * saved on shutdown. Any time dependant functions should be called
 * from this function, and probably be passed tod as an argument.
 * Please don't modify tod in the dependant function. */
void tick_the_clock(void)
{
    timeofday_t tod;

    todtick++;

    if (todtick % 20 == 0) {
        write_todclock();
    }

    get_tod(&tod);
    world_darkness -= season_timechange[tod.season][tod.hour];
}

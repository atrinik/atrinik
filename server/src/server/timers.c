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

/**
 * @file
 * Timer related functions. */

#include <timers.h>
#include <sproto.h>

/**
 * Processes all timers. */
void cftimer_process_timers()
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (timers_table[i].mode == TIMER_MODE_CYCLES)
		{
			timers_table[i].delay --;

			if (timers_table[i].delay == 0)
			{
				/* Call object timer event */
				timers_table[i].mode = TIMER_MODE_DEAD;
			}
		}
		else if (timers_table[i].mode == TIMER_MODE_SECONDS)
		{
			if (timers_table[i].delay <= seconds())
			{
				/* Call object timer event */
				timers_table[i].mode = TIMER_MODE_DEAD;
			}
		}
	}
}

/**
 * Creates a new timer.
 * @param id Desired timer identifier
 * @param delay Desired timer delay
 * @param ob Object that will be linked to this timer
 * @param mode Count mode (seconds or cycles). See timers.h
 * @retval TIMER_ERR_NONE Timer was successfully created.
 * @retval TIMER_ERR_ID Invalid ID.
 * @retval TIMER_ERR_MODE Invalid mode.
 * @retval TIMER_ERR_OBJ Invalid object. */
int cftimer_create(int id, long delay, object* ob, int mode)
{
	if (id >= MAX_TIMERS)
	{
		return TIMER_ERR_ID;
	}

	if (id < 0)
	{
		return TIMER_ERR_ID;
	}

	if (timers_table[id].mode != TIMER_MODE_DEAD)
	{
		return TIMER_ERR_ID;
	}

	if ((mode != TIMER_MODE_SECONDS) && (mode != TIMER_MODE_CYCLES))
	{
		return TIMER_ERR_MODE;
	}

	if (ob == NULL)
	{
		return TIMER_ERR_OBJ;
	}

	timers_table[id].mode = mode;
	timers_table[id].ob = ob;

	if (mode == TIMER_MODE_CYCLES)
	{
		timers_table[id].delay = delay;
	}
	else
	{
		timers_table[id].delay = seconds() + delay;
	}

	return TIMER_ERR_NONE;
}

/**
 * Destroys an existing timer.
 * @param id Identifier of the timer to destroy.
 * @retval TIMER_ERR_NONE No problem encountered.
 * @retval TIMER_ERR_ID Unknown ID: timer not found. */
int cftimer_destroy(int id)
{
	if (id >= MAX_TIMERS)
	{
		return TIMER_ERR_ID;
	}

	if (id < 0)
	{
		return TIMER_ERR_ID;
	}

	timers_table[id].mode = TIMER_MODE_DEAD;
	return TIMER_ERR_NONE;
}

/**
 * Finds a free ID for a new timer.
 * @return TIMER_ERR_ID if no free ID is available, a nonzero free ID
 * otherwise. */
int cftimer_find_free_id()
{
	int i;

	for (i = 0; i < MAX_TIMERS; i++)
	{
		if (timers_table[i].mode == TIMER_MODE_DEAD)
		{
			return i;
		}
	}

	return TIMER_ERR_ID;
}

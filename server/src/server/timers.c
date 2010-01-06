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
 * Timer related functions. */

#include <timers.h>
#include <sproto.h>

cftimer timers_table[MAX_TIMERS];

static void cftimer_process_event(object *ob);

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
				cftimer_process_event(timers_table[i].ob);
			}
		}
		else if (timers_table[i].mode == TIMER_MODE_SECONDS)
		{
			if (timers_table[i].delay <= seconds())
			{
				/* Call object timer event */
				timers_table[i].mode = TIMER_MODE_DEAD;
				cftimer_process_event(timers_table[i].ob);
			}
		}
	}
}

/**
 * Triggers the ::EVENT_TIMER of the given object.
 * @param ob Object to trigger the event for. */
static void cftimer_process_event(object *ob)
{
	if (ob)
	{
		trigger_event(EVENT_TIMER, NULL, ob, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL);
	}
}

/**
 * Creates a new timer.
 * @param id Desired timer identifier.
 * @param delay Desired timer delay.
 * @param ob Object that will be linked to this timer.
 * @param mode Unit for delay, should be ::TIMER_MODE_SECONDS or
 * ::TIMER_MODE_CYCLES. See timers.h.
 * @retval TIMER_ERR_NONE Timer was successfully created.
 * @retval TIMER_ERR_ID Invalid ID.
 * @retval TIMER_ERR_MODE Invalid mode.
 * @retval TIMER_ERR_OBJ Invalid object. */
int cftimer_create(int id, long delay, object *ob, int mode)
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

	if (!ob || !get_event_object(ob, EVENT_TIMER))
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

/**
 * Initialize timers. */
void cftimer_init()
{
	memset(&timers_table[0], 0, sizeof(cftimer) * MAX_TIMERS);
}

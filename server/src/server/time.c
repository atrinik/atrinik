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
 * In-game time functions. */

#include <global.h>

#ifndef WIN32
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#endif

long max_time = MAX_TIME;
long pticks;

/** In-game seasons. */
const char *season_name[SEASONS_PER_YEAR + 1] =
{
	"The Season of New Year",
	"The Season of Growth",
	"The Season of Harvest",
	"The Season of Decay",
	"The Season of the Blizzard",
	"\n"
};

/** Days of the week. */
const char *weekdays[DAYS_PER_WEEK] =
{
	"the Day of the Moon",
	"the Day of the Bull",
	"the Day of the Deception",
	"the Day of Thunder",
	"the Day of Freedom",
	"the Day of the Great Gods",
	"the Day of the Sun"
};

/** Months. */
const char *month_name[MONTHS_PER_YEAR] =
{
	"Month of the Ice Dragon",
	"Month of the Frost Giant",
	"Month of the Clouds",
	"Month of Gaea",
	"Month of the Harvest",
	"Month of Futility",
	"Month of the Dragon",
	"Month of the Sun",
	"Month of the Falling",
	"Month of the Dark Shades",
	"Month of the Great Infernus",
	"Month of the Ancient Darkness",
};

/** Periods of day. */
const char *periodsofday[PERIODS_PER_DAY] = {
	"Night",
	"Dawn",
	"Morning",
	"Noon",
	"Evening",
	"Dusk"
};

/**
 * Initialize all variables used in the timing routines. */
void reset_sleep()
{
	(void) GETTIMEOFDAY(&last_time);
}

/**
 * Generic function for simple timeval arithmetic. */
static void add_time(struct timeval *dst, struct timeval *a, struct timeval *b)
{
	dst->tv_sec = a->tv_sec + b->tv_sec;
	dst->tv_usec = a->tv_usec + b->tv_usec;

	if (dst->tv_sec < 0 || (dst->tv_sec == 0 && dst->tv_usec < 0))
	{
		while (dst->tv_usec < -1000000)
		{
			dst->tv_sec -= 1;
			dst->tv_usec += 1000000;
		}
	}
	else
	{
		while (dst->tv_usec < 0)
		{
			dst->tv_sec -= 1;
			dst->tv_usec += 1000000;
		}

		while (dst->tv_usec > 1000000)
		{
			dst->tv_sec += 1;
			dst->tv_usec -= 1000000;
		}
	}
}

/**
 * Calculate time until the next tick. */
static int time_until_next_tick(struct timeval *out)
{
	struct timeval now, next_tick, tick_time;

	tick_time.tv_sec = 0;
	tick_time.tv_usec = max_time;

	add_time(&next_tick, &last_time, &tick_time);

	GETTIMEOFDAY(&now);

	if (next_tick.tv_sec < now.tv_sec || (next_tick.tv_sec == now.tv_sec && next_tick.tv_usec <= now.tv_usec))
	{
		last_time.tv_sec = now.tv_sec;
		last_time.tv_usec = now.tv_usec;
		out->tv_sec = 0;
		out->tv_usec = 0;

		return 0;
	}

	now.tv_sec = -now.tv_sec;
	now.tv_usec = -now.tv_usec;
	add_time(out, &next_tick, &now);

	return 1;
}

/**
 * Checks how much time has elapsed since last tick.
 * If it is less than max_time, the remaining time is slept with
 * select(). */
void sleep_delta()
{
	struct timeval timeout;

	while (time_until_next_tick(&timeout))
	{
		doeric_server(SOCKET_UPDATE_CLIENT, &timeout);
	}
}

/**
 * Sets the max speed. Can be called by a DM through the /speed
 * command.
 * @param t New speed. */
void set_max_time(long t)
{
	max_time = t;
}

/**
 * Computes the in-game time of the day.
 * @param tod Where to store information. Must not be NULL. */
void get_tod(timeofday_t *tod)
{
	tod->year = todtick / HOURS_PER_YEAR;
	tod->month = (todtick / HOURS_PER_MONTH) % MONTHS_PER_YEAR;
	tod->day = (todtick % HOURS_PER_MONTH) / DAYS_PER_MONTH;
	tod->dayofweek = tod->day % DAYS_PER_WEEK;
	tod->hour = todtick % HOURS_PER_DAY;
	tod->minute = (pticks % PTICKS_PER_CLOCK) / (PTICKS_PER_CLOCK / 58);

	/* it's imprecise at best anyhow */
	if (tod->minute > 58)
	{
		tod->minute = 58;
	}

	tod->weekofmonth = tod->day / WEEKS_PER_MONTH;

	if (tod->month < 3)
	{
		tod->season = 0;
	}
	else if (tod->month < 6)
	{
		tod->season = 1;
	}
	else if (tod->month < 9)
	{
		tod->season = 2;
	}
	else if (tod->month < 12)
	{
		tod->season = 3;
	}
	else
	{
		tod->season = 4;
	}

	/* Until 4:59 */
	if (tod->hour < 5)
	{
		tod->periodofday = 0;
	}
	else if (tod->hour < 8)
	{
		tod->periodofday = 1;
	}
	else if (tod->hour < 13)
	{
		tod->periodofday = 2;
	}
	else if (tod->hour < 15)
	{
		tod->periodofday = 3;
	}
	else if (tod->hour < 20)
	{
		tod->periodofday = 4;
	}
	else if (tod->hour < 23)
	{
		tod->periodofday = 5;
	}
	/* Back to night */
	else
	{
		tod->periodofday = 0;
	}
}

/**
 * Prints the time.
 * @param op Player who requested time. */
void print_tod(object *op)
{
	timeofday_t tod;
	char *suf;
	int day;

	get_tod(&tod);
	new_draw_info_format(NDI_UNIQUE, op, "It is %d minute%s past %d o'clock %s, on %s", tod.minute + 1, ((tod.minute + 1 < 2) ? "" : "s"), ((tod.hour % (HOURS_PER_DAY / 2) == 0) ? (HOURS_PER_DAY / 2) : ((tod.hour) % (HOURS_PER_DAY / 2))), ((tod.hour >= (HOURS_PER_DAY / 2)) ? "pm" : "am"), weekdays[tod.dayofweek]);

	day = tod.day + 1;

	if (day == 1 || ((day % 10) == 1 && day > 20))
	{
		suf = "st";
	}
	else if (day == 2 || ((day % 10) == 2 && day > 20))
	{
		suf = "nd";
	}
	else if (day == 3 || ((day % 10) == 3 && day > 20))
	{
		suf = "rd";
	}
	else
	{
		suf = "th";
	}

	new_draw_info_format(NDI_UNIQUE, op, "The %d%s Day of the %s, Year %d", day, suf, month_name[tod.month], tod.year + 1);
	new_draw_info_format(NDI_UNIQUE, op, "Time of Year: %s", season_name[tod.season]);
}

/**
 * Players wants to know the time. Called through the /time command.
 * @param op Player who requested time. */
void time_info(object *op)
{
	print_tod(op);
}

/**
 * Gets the seconds.
 * @return Seconds. */
long seconds()
{
	struct timeval now;

	(void) GETTIMEOFDAY(&now);
	return now.tv_sec;
}

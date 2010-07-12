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

/** Size of history buffer. */
#define PBUFLEN 100

/** Historic data. */
static long process_utime_save[PBUFLEN];
/** Index in ::process_utime_save. */
static long psaveind;
/** Longest cycle time. */
static long process_max_utime = 0;
/** Shortest cycle time. */
static long process_min_utime = 999999999;
static long process_tot_mtime;
long pticks;
static long process_utime_long_count;

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
	int i;

	for (i = 0; i < PBUFLEN; i++)
	{
		process_utime_save[i] = 0;
	}

	psaveind = 0;
	process_max_utime = 0;
	process_min_utime = 999999999;
	process_tot_mtime = 0;
	pticks = 0;

	(void) GETTIMEOFDAY(&last_time);
}

/**
 * Adds time to our history list. */
static void log_time(long process_utime)
{
	pticks++;

	if (++psaveind >= PBUFLEN)
	{
		psaveind = 0;
	}

	process_utime_save[psaveind] = process_utime;

	if (process_utime > process_max_utime)
	{
		process_max_utime = process_utime;
	}

	if (process_utime < process_min_utime)
	{
		process_min_utime = process_utime;
	}

	process_tot_mtime += process_utime / 1000;
}

/**
 * Checks how much time has elapsed since last tick.
 * If it is less than max_time, the remaining time is slept with
 * select(). */
void sleep_delta()
{
	static struct timeval new_time;
	static struct timeval def_time = {0, 100000};
	static struct timeval sleep_time;
	long sleep_sec, sleep_usec;

	(void) GETTIMEOFDAY(&new_time);

	sleep_sec = last_time.tv_sec - new_time.tv_sec;
	sleep_usec = max_time - (new_time.tv_usec - last_time.tv_usec);

	/* This is very ugly, but probably the fastest for our use: */
	while (sleep_usec < 0)
	{
		sleep_usec += 1000000;
		sleep_sec -= 1;
	}

	while (sleep_usec > 1000000)
	{
		sleep_usec -= 1000000;
		sleep_sec += 1;
	}

	log_time((new_time.tv_sec - last_time.tv_sec) * 1000000 + new_time.tv_usec - last_time.tv_usec);

	if (sleep_sec >= 0 && sleep_usec > 0)
	{
		sleep_time.tv_sec = sleep_sec;
		sleep_time.tv_usec = sleep_usec;

		/*LOG(llevDebug, "SLEEP-Time: %ds and %dus\n", sleep_time.tv_sec, sleep_time.tv_usec);*/
		/* we ignore seconds to sleep - there is NO reason to put the server
		 * for even a single second to sleep when there is someone connected. */
		if (sleep_time.tv_sec || sleep_time.tv_usec > 500000)
		{
			LOG(llevBug, "BUG: sleep_delta(): sleep delta out of range! (%"FMT64U"s %"FMT64U"us)\n", (uint64) sleep_time.tv_sec, (uint64) sleep_time.tv_usec);
		}

#ifndef WIN32
		/* 'select' doesn't work on Windows, 'Sleep' is used instead */
		if (sleep_time.tv_sec || sleep_time.tv_usec > 500000)
		{
			select(0, NULL, NULL, NULL, &def_time);
		}
		else
		{
			select(0, NULL, NULL, NULL, &sleep_time);
		}
#else
		if (sleep_time.tv_usec >= 1000)
		{
			Sleep((int) (sleep_time.tv_usec / 1000));
		}
		else if (sleep_time.tv_usec)
		{
			Sleep(1);
		}
#endif
	}
	else
	{
		process_utime_long_count++;
	}

	/* Set last_time to when we're expected to wake up: */
	last_time.tv_usec += max_time;

	while (last_time.tv_usec > 1000000)
	{
		last_time.tv_usec -= 1000000;
		last_time.tv_sec++;
	}

	/* Don't do too much catching up:
	 * (Things can still get jerky on a slow/loaded computer) */
	if (last_time.tv_sec * 1000000 + last_time.tv_usec < new_time.tv_sec * 1000000 + new_time.tv_usec)
	{
		last_time.tv_sec = new_time.tv_sec;
		last_time.tv_usec = new_time.tv_usec;
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
	int tot = 0, maxt = 0, mint = 99999999, long_count = 0, i;

	print_tod(op);

	if (!QUERY_FLAG(op, FLAG_WIZ))
	{
		return;
	}

	new_draw_info_format(NDI_UNIQUE, op, "Total time:\nticks=%ld  time=%ld.%2ld", pticks, process_tot_mtime / 1000, process_tot_mtime % 1000);
	new_draw_info_format(NDI_UNIQUE, op, "avg time=%ldms  max time=%ldms  min time=%ldms", process_tot_mtime / pticks, process_max_utime / 1000, process_min_utime / 1000);
	new_draw_info_format(NDI_UNIQUE, op, "ticks longer than max time (%ldms) = %ld (%ld%%)", max_time / 1000, process_utime_long_count, 100 * process_utime_long_count / pticks);
	new_draw_info_format(NDI_UNIQUE, op, "Time last %ld ticks:", pticks > PBUFLEN ? PBUFLEN : pticks);

	for (i = 0; i < (pticks > PBUFLEN ? PBUFLEN : pticks); i++)
	{
		tot += process_utime_save[i];

		if (process_utime_save[i] > maxt)
		{
			maxt = process_utime_save[i];
		}

		if (process_utime_save[i] < mint)
		{
			mint = process_utime_save[i];
		}

		if (process_utime_save[i] > max_time)
		{
			long_count++;
		}
	}

	new_draw_info_format(NDI_UNIQUE, op, "avg time=%ldms  max time=%dms  min time=%dms", tot / (pticks > PBUFLEN ? PBUFLEN : pticks) / 1000, maxt / 1000, mint / 1000);
	new_draw_info_format(NDI_UNIQUE, op, "ticks longer than max time (%ldms) = %d (%ld%%)", max_time / 1000, long_count, 100 * long_count / (pticks > PBUFLEN ? PBUFLEN : pticks));
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

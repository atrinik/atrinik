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

#include <global.h>
#include <funcpoint.h>
#include <tod.h>

#ifndef WIN32 /* ---win32 exclude header */
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#endif /* win32 */

/* Global variables: */
long max_time = MAX_TIME;

#define PBUFLEN 100
long process_utime_save[PBUFLEN];
long psaveind;
long process_max_utime = 0;
long process_min_utime = 999999999;
long process_tot_mtime;
long pticks;
long process_utime_long_count;

const char *season_name[] =
{
	"The Season of New Year",
	"The Season of Growth",
	"The Season of Harvest",
	"The Season of Decay",
	"The Season of the Blizzard",
	"\n"
};

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

/* Initialise all variables used in the timing routines. */
void reset_sleep()
{
	int i;

	for (i = 0; i < PBUFLEN; i++)
		process_utime_save[i] = 0;

	psaveind = 0;
	process_max_utime = 0;
	process_min_utime = 999999999;
	process_tot_mtime = 0;
	pticks = 0;

	(void) GETTIMEOFDAY(&last_time);
}

void log_time(long process_utime)
{
	pticks++;

	if (++psaveind >= PBUFLEN)
		psaveind = 0;

	process_utime_save[psaveind] = process_utime;

	if (process_utime > process_max_utime)
		process_max_utime = process_utime;

	if (process_utime < process_min_utime)
		process_min_utime = process_utime;

	process_tot_mtime += process_utime / 1000;
}

/* enough_elapsed_time will return true if the time passed since
 * last tick is more than max-time. */
int enough_elapsed_time()
{
	static struct timeval new_time;
	long elapsed_utime;

	(void) GETTIMEOFDAY(&new_time);

	elapsed_utime = (new_time.tv_sec - last_time.tv_sec) * 1000000 + new_time.tv_usec - last_time.tv_usec;

	if (elapsed_utime > max_time)
	{
		log_time(elapsed_utime);
		last_time.tv_sec = new_time.tv_sec;
		last_time.tv_usec = new_time.tv_usec;
		return 1;
	}

	return 0;
}

/* sleep_delta checks how much time has elapsed since last tick.
 * If it is less than max_time, the remaining time is slept with select(). */

/* of course - this is wasted time. it would be much nicer to
 * poll the player socket in this time and store thee incoming data
 * in fast access buffers.
 * On the other hand runs some server as "guests" on their system -
 * their is reduced cpu usage very friendly. */
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
			LOG(llevBug, "BUG: sleep_delta(): sleep delta out of range! (%ds %dus)\n", sleep_time.tv_sec, sleep_time.tv_usec);
		}

#ifndef WIN32
		/* 'select' doesn't work on Windows, 'Sleep' is used instead */
		if (sleep_time.tv_sec || sleep_time.tv_usec > 500000)
			select(0, NULL, NULL, NULL, &def_time);
		else
			select(0, NULL, NULL, NULL, &sleep_time);
#else
		if (sleep_time.tv_usec >= 1000)
			Sleep((int)(sleep_time.tv_usec / 1000));
		else if (sleep_time.tv_usec)
			Sleep(1);
#endif
	}
	else
		process_utime_long_count++;

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

void set_max_time(long t)
{
	max_time = t;
}

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
		tod->minute = 58;

	tod->weekofmonth = tod->day / WEEKS_PER_MONTH;

	if (tod->month < 3)
		tod->season = 0;
	else if (tod->month < 6)
		tod->season = 1;
	else if (tod->month < 9)
		tod->season = 2;
	else if (tod->month < 12)
		tod->season = 3;
	else
		tod->season = 4;
}

void print_tod(object *op)
{
	timeofday_t tod;
	char *suf;
	int day;

	get_tod(&tod);
	sprintf(errmsg, "It is %d minute%s past %d o'clock %s,", tod.minute + 1, ((tod.minute + 1 < 2) ? "" : "s"), ((tod.hour % (HOURS_PER_DAY / 2) == 0) ? (HOURS_PER_DAY / 2) : ((tod.hour) % (HOURS_PER_DAY / 2))), ((tod.hour >= (HOURS_PER_DAY / 2)) ? "pm" : "am"));
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);

	sprintf(errmsg, "on %s", weekdays[tod.dayofweek]);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);

	day = tod.day + 1;
	if (day == 1 || ((day % 10) == 1 && day > 20))
		suf = "st";
	else if (day == 2 || ((day % 10) == 2 && day > 20))
		suf = "nd";
	else if (day == 3 || ((day % 10) == 3 && day > 20))
		suf = "rd";
	else
		suf = "th";

	sprintf(errmsg, "The %d%s Day of the %s, Year %d", day, suf, month_name[tod.month], tod.year + 1);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);

	sprintf(errmsg, "Time of Year: %s", season_name[tod.season]);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);
}

void time_info(object *op)
{
	int tot = 0, maxt = 0, mint = 99999999, long_count = 0, i;

	print_tod(op);

	if (!QUERY_FLAG(op, FLAG_WIZ))
		return;

	(*draw_info_func) (NDI_UNIQUE, 0, op, "Total time:");
	sprintf(errmsg, "ticks=%ld  time=%ld.%2ld", pticks, process_tot_mtime / 1000, process_tot_mtime % 1000);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);
	sprintf(errmsg, "avg time=%ldms  max time=%ldms  min time=%ldms", process_tot_mtime / pticks, process_max_utime / 1000, process_min_utime / 1000);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);
	sprintf(errmsg, "ticks longer than max time (%ldms) = %ld (%ld%%)", max_time / 1000, process_utime_long_count, 100 * process_utime_long_count / pticks);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);

	sprintf(errmsg, "Time last %ld ticks:", pticks > PBUFLEN ? PBUFLEN : pticks);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);

	for (i = 0; i < (pticks > PBUFLEN ? PBUFLEN : pticks); i++)
	{
		tot += process_utime_save[i];

		if (process_utime_save[i] > maxt)
			maxt = process_utime_save[i];

		if (process_utime_save[i] < mint)
			mint = process_utime_save[i];

		if (process_utime_save[i] > max_time)
			long_count++;
	}

	sprintf(errmsg,"avg time=%ldms  max time=%dms  min time=%dms", tot / (pticks > PBUFLEN ? PBUFLEN : pticks) / 1000, maxt / 1000, mint / 1000);
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);
	sprintf(errmsg,"ticks longer than max time (%ldms) = %d (%ld%%)", max_time / 1000, long_count, 100 * long_count / (pticks > PBUFLEN ? PBUFLEN : pticks));
	(*draw_info_func) (NDI_UNIQUE, 0, op, errmsg);
}

long seconds()
{
	struct timeval now;

	(void) GETTIMEOFDAY(&now);
	return now.tv_sec;
}

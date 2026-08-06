/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Defines for in-game clock and ticks management.
 */

#ifndef TOD_H
#define TOD_H

/** Ticks per clock. */
#define PTICKS_PER_CLOCK 1500

/** Hours per day. */
#define HOURS_PER_DAY 24
/** Days per week. */
#define DAYS_PER_WEEK 7
/** Weeks per month. */
#define WEEKS_PER_MONTH 4
/** Months per year. */
#define MONTHS_PER_YEAR 12
/** Seasons per year. */
#define SEASONS_PER_YEAR 4
/** Periods per day. */
#define PERIODS_PER_DAY 10

/** Weeks per year. */
#define WEEKS_PER_YEAR (WEEKS_PER_MONTH * MONTHS_PER_YEAR)
/** Days per month. */
#define DAYS_PER_MONTH (DAYS_PER_WEEK * WEEKS_PER_MONTH)
/** Days per year. */
#define DAYS_PER_YEAR (DAYS_PER_MONTH * MONTHS_PER_YEAR)
/** Hours per week. */
#define HOURS_PER_WEEK (HOURS_PER_DAY * DAYS_PER_WEEK)
/** Hours per month. */
#define HOURS_PER_MONTH (HOURS_PER_WEEK * WEEKS_PER_MONTH)
/** Hours per year. */
#define HOURS_PER_YEAR (HOURS_PER_MONTH * MONTHS_PER_YEAR)
/** Months per season. */
#define MONTHS_PER_SEASON (MONTHS_PER_YEAR / SEASONS_PER_YEAR)

/** Represents the in-game time. */
typedef struct _timeofday {
    /** Year */
    int year;

    /** Month */
    int month;

    /** Day */
    int day;

    /** Day of week */
    int dayofweek;

    /** Hour */
    int hour;

    /** Minute */
    int minute;

    /** Week of month */
    int weekofmonth;

    /** Season */
    int season;

    /** Period of day. */
    int periodofday;
} timeofday_t;

/** Public API implemented in src/server/time.c. */

extern long max_time;

extern int max_time_multiplier;

extern long pticks;

extern struct timeval last_time;

extern const char *season_name[4];

extern const char *weekdays[7];

extern const char *month_name[12];

extern const char *periodsofday[10];

extern const int periodsofday_hours[24];

extern void reset_sleep(void);

extern void sleep_delta(void);

extern void set_max_time(long t);

extern void set_max_time_multiplier(int t);

extern void get_tod(timeofday_t *tod);

extern void print_tod(object *op);

extern void time_info(object *op);

extern long seconds(void);

#endif

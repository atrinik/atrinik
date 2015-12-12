/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * In-game time functions. */

#include <global.h>

long max_time = MAX_TIME;
int max_time_multiplier = MAX_TIME_MULTIPLIER;

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
/** Used for main loop timing. */
struct timeval last_time;

/** In-game seasons. */
const char *season_name[SEASONS_PER_YEAR] = {
    "Season of the Blizzard",
    "Season of Growth",
    "Season of Harvest",
    "Season of Decay",
};

/** Days of the week. */
const char *weekdays[DAYS_PER_WEEK] = {
    "Day of the Moon",
    "Day of the Bull",
    "Day of the Deception",
    "Day of Thunder",
    "Day of Freedom",
    "Day of the Great Gods",
    "Day of the Sun"
};

/** Months. */
const char *month_name[MONTHS_PER_YEAR] = {
    "Month of the Winter",
    "Month of the Ice Dragon",
    "Month of the Frost Giant",
    "Month of Terria",
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
    "midnight",
    "late night",
    "dawn",
    "morning",
    "late morning",
    "noon",
    "afternoon",
    "dusk",
    "evening",
    "night"
};

/**
 * Period of the day at each hour in the day. */
const int periodsofday_hours[HOURS_PER_DAY] = {
    /* 24: Midnight */
    0,
    /* 1 - 4: Late night */
    1, 1, 1, 1,
    /* 5: Dawn */
    2,
    /* 6 - 9: Morning */
    3, 3, 3, 3,
    /* 10 - 11: Late morning */
    4, 4,
    /* 12: Noon */
    5,
    /* 13 - 17: Afternoon */
    6, 6, 6, 6, 6,
    /* 18: Dusk */
    7,
    /* 19 - 21: Evening */
    8, 8, 8,
    /* 22 - 23: Night */
    9, 9
};

/**
 * Initialize all variables used in the timing routines. */
void reset_sleep(void)
{
    int i;

    for (i = 0; i < PBUFLEN; i++) {
        process_utime_save[i] = 0;
    }

    psaveind = 0;
    process_max_utime = 0;
    process_min_utime = 999999999;
    process_tot_mtime = 0;
    pticks = 1;

    (void) GETTIMEOFDAY(&last_time);
}

/**
 * Adds time to our history list. */
static void log_time(long process_utime)
{
    if (++psaveind >= PBUFLEN) {
        psaveind = 0;
    }

    process_utime_save[psaveind] = process_utime;

    if (process_utime > process_max_utime) {
        process_max_utime = process_utime;
    }

    if (process_utime < process_min_utime) {
        process_min_utime = process_utime;
    }

    process_tot_mtime += process_utime / 1000;
}

/**
 * Checks how much time has elapsed since last tick.
 * If it is less than max_time, the remaining time is slept with
 * select(). */
void sleep_delta(void)
{
    static struct timeval new_time;
    long sleep_sec, sleep_usec;

    GETTIMEOFDAY(&new_time);

    sleep_sec = last_time.tv_sec - new_time.tv_sec;
    sleep_usec = max_time / max_time_multiplier - (new_time.tv_usec - last_time.tv_usec);

    /* This is very ugly, but probably the fastest for our use: */
    while (sleep_usec < 0) {
        sleep_usec += 1000000;
        sleep_sec -= 1;
    }

    while (sleep_usec > 1000000) {
        sleep_usec -= 1000000;
        sleep_sec += 1;
    }

    log_time((new_time.tv_sec - last_time.tv_sec) * 1000000 + new_time.tv_usec - last_time.tv_usec);

    if (sleep_sec >= 0 && sleep_usec > 0) {
        static struct timeval sleep_time;

        sleep_time.tv_sec = sleep_sec;
        sleep_time.tv_usec = sleep_usec;

#ifndef WIN32
        select(0, NULL, NULL, NULL, &sleep_time);
#else

        if (sleep_time.tv_sec) {
            Sleep(sleep_time.tv_sec * 1000);
        }

        Sleep((int) (sleep_time.tv_usec / 1000.0));
#endif
    } else {
        process_utime_long_count++;
    }

    /* Set last_time to when we're expected to wake up: */
    last_time.tv_usec += max_time / max_time_multiplier;

    while (last_time.tv_usec > 1000000) {
        last_time.tv_usec -= 1000000;
        last_time.tv_sec++;
    }

    /* Don't do too much catching up:
     * (Things can still get jerky on a slow/loaded computer) */
    if ((last_time.tv_sec - new_time.tv_sec) * 1000000 + (last_time.tv_usec - new_time.tv_usec) < 0) {
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
 * Sets the max speed multiplier.
 * @param t New speed multiplier.
 */
void set_max_time_multiplier(long t)
{
    max_time_multiplier = t;
}

/**
 * Computes the in-game time of the day.
 * @param tod Where to store information. Must not be NULL. */
void get_tod(timeofday_t *tod)
{
    tod->year = todtick / HOURS_PER_YEAR;
    tod->month = (todtick / HOURS_PER_MONTH) % MONTHS_PER_YEAR;
    tod->season = tod->month / MONTHS_PER_SEASON;
    tod->day = (todtick % HOURS_PER_MONTH) / DAYS_PER_MONTH;
    tod->dayofweek = tod->day % DAYS_PER_WEEK;
    tod->weekofmonth = tod->day / WEEKS_PER_MONTH;
    tod->hour = todtick % HOURS_PER_DAY;
    tod->periodofday = periodsofday_hours[tod->hour];
    tod->minute = (pticks % PTICKS_PER_CLOCK) / (PTICKS_PER_CLOCK / 58);

    if (tod->minute > 59) {
        tod->minute = 59;
    }
}

/**
 * Prints the time.
 * @param op Player who requested time. */
void print_tod(object *op)
{
    timeofday_t tod;
    get_tod(&tod);
    draw_info_format(COLOR_WHITE, op, "It is %s, %d minute%s past %d o'clock %s, on the %s.", periodsofday[tod.periodofday], tod.minute, ((tod.minute == 1) ? "" : "s"), ((tod.hour % (HOURS_PER_DAY / 2) == 0) ? (HOURS_PER_DAY / 2) : ((tod.hour) % (HOURS_PER_DAY / 2))), ((tod.hour >= (HOURS_PER_DAY / 2)) ? "pm" : "am"), weekdays[tod.dayofweek]);

    int day = tod.day + 1;

    const char *suf;
    if (day == 1 || ((day % 10) == 1 && day > 20)) {
        suf = "st";
    } else if (day == 2 || ((day % 10) == 2 && day > 20)) {
        suf = "nd";
    } else if (day == 3 || ((day % 10) == 3 && day > 20)) {
        suf = "rd";
    } else {
        suf = "th";
    }

    draw_info_format(COLOR_WHITE, op, "The %d%s Day of the %s, Year %d, in the %s.", day, suf, month_name[tod.month], tod.year + 1, season_name[tod.season]);
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
long seconds(void)
{
    struct timeval now;

    (void) GETTIMEOFDAY(&now);
    return now.tv_sec;
}

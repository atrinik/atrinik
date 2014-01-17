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
 * Logger API.
 *
 * @author Alex Tokar */

#include <global.h>
#include <stdarg.h>

/**
 * Name of the API. */
#define API_NAME logger

/**
 * If 1, the API has been initialized. */
static uint8 did_init = 0;

/**
 * Pointer to open log file, if any. */
static FILE *log_fp;

/**
 * The print function. */
static logger_print_func print_func;

/**
 * Initialize the logger API.
 * @internal */
void toolkit_logger_init(void)
{
    TOOLKIT_INIT_FUNC_START(logger)
    {
        log_fp = NULL;
        logger_set_print_func(logger_do_print);
    }
    TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the logger API.
 * @internal */
void toolkit_logger_deinit(void)
{
    TOOLKIT_DEINIT_FUNC_START(logger)
    {
        if (log_fp) {
            fclose(log_fp);
        }
    }
    TOOLKIT_DEINIT_FUNC_END()
}

void logger_open_log(const char *path)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    if (log_fp) {
        fclose(log_fp);
    }

    log_fp = fopen(path, "w");

    if (!log_fp) {
        return;
    }
}

FILE *logger_get_logfile(void)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    return log_fp ? log_fp : stdout;
}

void logger_set_print_func(logger_print_func func)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    print_func = func;
}

void logger_do_print(const char *str)
{
    TOOLKIT_FUNC_PROTECTOR(API_NAME);
    fputs(str, stdout);
}

void logger_print(const char *level, const char *function, uint64 line, const char *format, ...)
{
    char formatted[HUGE_BUF], timebuf[HUGE_BUF], buf[sizeof(formatted) * 2];
    va_list ap;
    struct timeval tv;
    struct tm *tm;

    TOOLKIT_FUNC_PROTECTOR(API_NAME);

    va_start(ap, format);
    vsnprintf(formatted, sizeof(formatted), format, ap);
    va_end(ap);

    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    if (tm) {
        char timebuf2[MAX_BUF];

        strftime(timebuf2, sizeof(timebuf2), "%H:%M:%S", tm);
        snprintf(timebuf, sizeof(timebuf), "[%s.%06"FMT64U "] ", timebuf2, (uint64) tv.tv_usec);
    }
    else {
        timebuf[0] = '\0';
    }

    snprintf(buf, sizeof(buf), LOGGER_ESC_SEQ_BOLD "%s"LOGGER_ESC_SEQ_END LOGGER_ESC_SEQ_RED "%s"LOGGER_ESC_SEQ_END " "LOGGER_ESC_SEQ_CYAN "[%s:%"FMT64U "]"LOGGER_ESC_SEQ_END " "LOGGER_ESC_SEQ_YELLOW "%s"LOGGER_ESC_SEQ_END "\n", timebuf, level, function, line, formatted);
    print_func(buf);

    if (log_fp) {
        fprintf(log_fp, "%s%s [%s:%"FMT64U "] %s\n", timebuf, level, function, line, formatted);
        fflush(log_fp);
    }
}

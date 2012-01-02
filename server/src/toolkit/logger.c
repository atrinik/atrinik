/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Logger API.
 *
 * @author Alex Tokar */

#include <global.h>
#include <stdarg.h>

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
}

void logger_open_log(const char *path)
{
	log_fp = fopen(path, "w");

	if (!log_fp)
	{
		return;
	}
}

FILE *logger_get_logfile(void)
{
	return log_fp ? log_fp : stdout;
}

void logger_set_print_func(logger_print_func func)
{
	print_func = func;
}

void logger_do_print(const char *str)
{
	fputs(str, stdout);
}

void logger_print(const char *level, const char *function, uint64 line, const char *format, ...)
{
	char formatted[HUGE_BUF], timebuf[HUGE_BUF], buf[HUGE_BUF];
	va_list ap;
	struct timeval tv;
	struct tm *tm;

	va_start(ap, format);
	vsnprintf(formatted, sizeof(formatted), format, ap);
	va_end(ap);

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	if (tm)
	{
		char timebuf2[MAX_BUF];

		strftime(timebuf2, sizeof(timebuf2), "%H:%M:%S", tm);
		snprintf(timebuf, sizeof(timebuf), "[%s.%06"FMT64U"] ", timebuf2, (uint64) tv.tv_usec);
	}
	else
	{
		timebuf[0] = '\0';
	}

	snprintf(buf, sizeof(buf), LOGGER_ESC_SEQ_BOLD"%s"LOGGER_ESC_SEQ_END LOGGER_ESC_SEQ_RED"%s"LOGGER_ESC_SEQ_END" "LOGGER_ESC_SEQ_CYAN"[%s:%"FMT64U"]"LOGGER_ESC_SEQ_END" "LOGGER_ESC_SEQ_YELLOW"%s"LOGGER_ESC_SEQ_END"\n", timebuf, level, function, line, formatted);

	print_func(buf);

	if (log_fp)
	{
		fputs(buf, log_fp);
		fprintf(log_fp, "%s%s [%s:%"FMT64U"] %s\n", timebuf, level, function, line, formatted);
		fflush(log_fp);
	}
}

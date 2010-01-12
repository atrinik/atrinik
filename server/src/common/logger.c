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
 * The LOG system is more than a logger - it works also as error and bug
 * counter system.
 *
 * Every llevBug LOG will increase the bug counter of the server - if too
 * many errors occur, the server will start an emergency shutdown. This
 * will avoid bug loops or every round LOGs, which will fill the log file
 * quickly.
 *
 * llevError is always irrecoverable and fatal - if it happens, the
 * server is not stable anymore and it will shutdown immediately. */

#include <stdarg.h>
#include <global.h>
#include <sproto.h>

/** How often to print out timestamps, in seconds. */
#define TIMESTAMP_INTERVAL 600

/** Last timestamp. */
static struct timeval last_timestamp = {0, 0};

/**
 * Put a string to either stderr or logfile.
 * @param buf String to put. */
static void do_print(char *buf)
{
#ifdef WIN32
	if (logfile)
	{
		/* Write to file or stdout */
		fputs(buf, logfile);
	}
	else
	{
		fputs(buf, stderr);
	}

#ifdef DEBUG
	if (logfile)
	{
		fflush(logfile);
	}
#endif

	if (logfile && logfile != stderr)
	{
		fputs(buf, stderr);
	}
#else
	if (logfile)
	{
		fputs(buf, logfile);
	}
	else
	{
		fputs(buf, stderr);
	}
#endif
}

/**
 * Check if timestamp is due. */
static void check_timestamp()
{
	struct timeval now;

	GETTIMEOFDAY(&now);

	if (now.tv_sec >= last_timestamp.tv_sec + TIMESTAMP_INTERVAL)
	{
		struct tm *tim;
		char buf[256];
		time_t temp_time = now.tv_sec;

		last_timestamp.tv_sec = now.tv_sec;
		tim = localtime(&temp_time);
		snprintf(buf, sizeof(buf), "\n*** TIMESTAMP: %02d:%02d:%02d %02d-%02d-%4d ***\n\n", tim->tm_hour, tim->tm_min, tim->tm_sec, tim->tm_mday, tim->tm_mon + 1, tim->tm_year + 1900);
		do_print(buf);
	}
}

/**
 * Logs a message to stderr, or to file, and/or even to socket.
 * Or discards the message if it is of no importance, and none have
 * asked to hear messages of that logLevel.
 *
 * See include/logger.h for possible logLevels. Messages with llevSystem
 * and llevError are always printed, regardless of debug mode.
 *
 * Additionally, llevError message will cause the server to exit.
 * @param logLevel Log level of the message
 * @param format Format specifiers
 * @param ... Arguments for the format */
void LOG(LogLevel logLevel, const char *format, ...)
{
	static int fatal_error = 0;
	char buf[HUGE_BUF * 2];

	va_list ap;
	va_start(ap, format);
	check_timestamp();

	if (logLevel <= settings.debug)
	{
		vsnprintf(buf, sizeof(buf), format, ap);
		do_print(buf);
	}

	va_end(ap);

	if (logLevel == llevBug)
	{
		nroferrors++;
	}

	if (nroferrors > MAX_ERRORS || logLevel == llevError)
	{
		if (fatal_error == 0)
		{
			fatal_error = 1;
			fatal(logLevel);
		}
	}
}

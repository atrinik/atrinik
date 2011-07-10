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

/**
 * Human-readable names of log levels. */
static const char *const loglevel_names[] =
{
	"[System] ",
	"[Error]  ",
	"[Bug]    ",
	"[Chat]   ",
	"[Info]   ",
	"[Debug]  "
};

/**
 * If 1, will not print one of ::loglevel_names on the next LOG() call. */
static uint8 loglevel_name_disable = 0;

/**
 * Put a string to either stderr or logfile.
 * @param buf String to put. */
static void do_print(const char *buf)
{
	fputs(buf, logfile);

	/* Is logfile a custom file, and not stderr (default)? */
	if (logfile != stderr)
	{
		/* Log to stderr as well. */
		fputs(buf, stderr);
		/* Flush the log file. */
		fflush(logfile);
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

	if (logLevel <= settings.debug)
	{
		va_list ap;
		int written;

		/* If loglevel name was disabled but this is a bug or an error,
		 * make an exception and put it onto the next line. */
		if (loglevel_name_disable && (logLevel == llevBug || logLevel == llevError))
		{
			loglevel_name_disable = 0;
			do_print("\n");
		}

		if (!loglevel_name_disable)
		{
			/* Prefix with timestamp. */
			if (settings.timestamp)
			{
				snprintf(buf, sizeof(buf), "[%"FMT64U"] ", (uint64) time(NULL));
				do_print(buf);
			}

			do_print(loglevel_names[logLevel]);
		}

		va_start(ap, format);
		written = vsnprintf(buf, sizeof(buf), format, ap);
		do_print(buf);

		if (written > 0)
		{
			/* If the last character isn't a newline, mark the next LOG
			 * call to have the loglevel name disabled. */
			if (buf[written - 1] != '\n')
			{
				loglevel_name_disable = 1;
			}
			else
			{
				loglevel_name_disable = 0;
			}
		}

		va_end(ap);
	}

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

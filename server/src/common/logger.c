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
* the Free Software Foundation; either version 3 of the License, or     *
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

#include <stdarg.h>
#include <global.h>
#include <funcpoint.h>

/*
 * Logs a message to stderr, or to file, and/or even to socket.
 * Or discards the message if it is of no importanse, and none have
 * asked to hear messages of that logLevel.
 *
 * See include/logger.h for possible logLevels.  Messages with llevSystem
 * and llevError are always printed, regardless of debug mode.
 */

void LOG(LogLevel logLevel, const char *format, ...)
{
  	static int fatal_error = FALSE;
	/* This needs to be really really big - larger
	 * than any other buffer, since that buffer may
	 * need to be put in this one. */
  	char buf[20480];

  	va_list ap;
  	va_start(ap, format);

	buf[0] = '\0';
	if (logLevel <= settings.debug)
	{
		vsprintf(buf, format, ap);
#ifdef WIN32
		/* ---win32 change log handling for win32 */
		if (logfile)
			/* wrote to file or stdout */
			fputs(buf, logfile);
		else
			fputs(buf, stderr);

	/* if we have a debug version, we want see ALL output */
	#ifdef DEBUG
		/* so flush this! */
		if (logfile)
			fflush(logfile);
	#endif

		/* if was it a logfile wrote it to screen too */
		if (logfile && logfile != stderr)
			fputs(buf, stderr);
#else
		if (logfile)
			fputs(buf, logfile);
		else
			fputs(buf, stderr);
#endif
	}

	va_end(ap);

	if (logLevel == llevBug)
		++nroferrors;

	if (nroferrors > MAX_ERRORS || logLevel == llevError)
	{
		exiting = 1;
		if(fatal_error == FALSE)
		{
			fatal_error = TRUE;
			fatal(logLevel);
		}
	}
}

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
 * Cross-platform support. */

#include <global.h>

/**
 * Initialize the porting API.
 * @internal */
void toolkit_porting_init(void)
{
	TOOLKIT_INIT_FUNC_START(porting)
	{
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the porting API.
 * @internal */
void toolkit_porting_deinit(void)
{
}

#ifndef __CPROTO__

#	ifndef HAVE_STRTOK_R
/**
 * Re-entrant string tokenizer; glibc version, licensed under GNU LGPL
 * version 2.1. */
char *strtok_r(char *s, const char *delim, char **save_ptr)
{
	char *token;

	if (s == NULL)
	{
		s = *save_ptr;
	}

	/* Scan leading delimiters.  */
	s += strspn(s, delim);

	if (*s == '\0')
	{
		*save_ptr = s;
		return NULL;
	}

	/* Find the end of the token.  */
	token = s;
	s = strpbrk(token, delim);

	if (s == NULL)
	{
		/* This token finishes the string.  */
		*save_ptr = strchr(token, '\0');
	}
	else
	{
		/* Terminate the token and make *SAVE_PTR point past it.  */
		*s = '\0';
		*save_ptr = s + 1;
	}

	return token;
}
#	endif

#	ifndef HAVE_TEMPNAM
static uint32 curtmp = 0;

char *tempnam(const char *dir, const char *pfx)
{
	char *name;
	pid_t pid = getpid();

	if (!pfx)
	{
		pfx = "tmp.";
	}

	/* This is a pretty simple method - put the pid as a hex digit and
	 * just keep incrementing the last digit. Check to see if the file
	 * already exists - if so, we'll just keep looking - eventually we
	 * should find one that is free. */
	if (dir)
	{
		if (!(name = (char *) malloc(MAXPATHLEN)))
		{
			return NULL;
		}

		do
		{
			snprintf(name, MAXPATHLEN, "%s/%s%hx.%d", dir, pfx, pid, curtmp);
			curtmp++;
		}
		while (access(name, F_OK) != -1);

		return name;
	}

	return NULL;
}
#	endif

#	ifndef HAVE_STRDUP
char *strdup(const char *s)
{
	size_t len = strlen(s) + 1;
	void *new = malloc(len);

	if (!new)
	{
		return NULL;
	}

	return (char *) memcpy(new, s, len);
}
#	endif

#	ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n)
{
	size_t len;
	char *new;

	len = strlen(s);

	if (n < len)
	{
		len = n;
	}

	new = malloc(len + 1);

	if (!new)
	{
		return NULL;
	}

	new[len] = '\0';

	return (char *) memcpy(new, s, len);
}
#	endif

#	ifndef HAVE_STRERROR
char *strerror(int errnum)
{
	return "";
}
#endif

#	ifndef HAVE_STRCASESTR
const char *strcasestr(const char *haystack, const char *needle)
{
	char c, sc;
	size_t len;

	if ((c = *needle++) != 0)
	{
		c = tolower(c);
		len = strlen(needle);

		do
		{
			do
			{
				if ((sc = *haystack++) == 0)
				{
					return NULL;
				}
			}
			while (tolower(sc) != c);
		}
		while (strncasecmp(haystack, needle, len) != 0);

		haystack--;
	}

	return haystack;
}
#	endif

#	ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
#ifdef WIN32
	FILETIME time;
	double timed;

	GetSystemTimeAsFileTime(&time);

	/* Apparently Win32 has units of 1e-7 sec (tenths of microsecs)
	 * 4294967296 is 2^32, to shift high word over
	 * 11644473600 is the number of seconds between
	 * the Win32 epoch 1601-Jan-01 and the Unix epoch 1970-Jan-01
	 * Tests found floating point to be 10x faster than 64bit int math. */
	timed = ((time.dwHighDateTime * 4294967296e-7) - 11644473600.0) + (time.dwLowDateTime * 1e-7);

	tv->tv_sec = (long) timed;
	tv->tv_usec = (long) ((timed - tv->tv_sec) * 1e6);

	/* Get the timezone, if they want it. */
	if (tz)
	{
		_tzset();

		tz->tz_minuteswest = _timezone;
		tz->tz_dsttime = _daylight;
	}

	return 0;
#else
	(void) tv;
	(void) tz;
	return 0;
#endif
}
#	endif

#endif

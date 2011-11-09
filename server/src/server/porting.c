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
 * This file contains various functions that are not really unique for
 * Atrinik, but rather provides what should be standard functions for
 * systems that do not have them. In this way, most of the nasty system
 * dependent stuff is contained here, with the program calling these
 * functions. */

#include <global.h>

/** Used to generate temporary unique name. */
static uint32 curtmp = 0;

/**
 * A replacement for the tempnam() function since it's not defined
 * at some UNIX variants.
 * @param dir Directory where to create the file. Can be NULL, in which
 * case NULL is returned.
 * @param pfx prefix to create unique name. Can be NULL.
 * @return Path to temporary file, or NULL if failure. Must be freed by
 * caller. */
char *tempnam_local(const char *dir, const char *pfx)
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

/**
 * A replacement of strdup(), since it's not defined at some
 * UNIX variants.
 * @param str String to duplicate.
 * @return Copy, needs to be freed by caller. NULL on memory allocation
 * error. */
char *strdup_local(const char *str)
{
	size_t len = strlen(str) + 1;
	void *new = malloc(len);

	if (!new)
	{
		return NULL;
	}

	return (char *) memcpy(new, str, len);
}

/**
 * Takes an error number and returns a string with a description of the
 * error.
 * @param errnum The error number.
 * @return The description of the error. */
char *strerror_local(int errnum)
{
#if defined(HAVE_STRERROR)
	return strerror(errnum);
#else
#	error Missing strerror().
#endif
}

/**
 * Computes the integer square root.
 * @param n Number of which to compute the root.
 * @return Integer square root. */
unsigned long isqrt(unsigned long n)
{
	unsigned long op = n, res = 0, one;

	/* "one" starts at the highest power of four <= than the argument. */
	one = 1 << 30;

	while (one > op)
	{
		one >>= 2;
	}

	while (one != 0)
	{
		if (op >= res + one)
		{
			op -= res + one;
			/* Faster than 2 * one. */
			res += one << 1;
		}

		res >>= 1;
		one >>= 2;
	}

	return res;
}

/**
 * Checks if any directories in the given path doesn't exist, and creates
 * if necessary.
 * @param filename File path we'll want to access. Can be NULL. */
void make_path_to_file(char *filename)
{
	char buf[MAX_BUF], *cp = buf;
	struct stat statbuf;

	if (!filename || !*filename)
	{
		return;
	}

	strcpy(buf, filename);

	while ((cp = strchr(cp + 1, '/')))
	{
		*cp = '\0';

		if (stat(buf, &statbuf) || !S_ISDIR(statbuf.st_mode))
		{
			if (mkdir(buf, 0777))
			{
				LOG(llevBug, "Cannot mkdir %s: %s\n", buf, strerror_local(errno));
				return;
			}
		}

		*cp = '/';
	}
}

/**
 * Finds a substring in a string, in a case-insensitive manner.
 * @param s String we're searching in.
 * @param find String we're searching for.
 * @return Pointer to first occurrence of find in s, NULL if not found. */
const char *strcasestr_local(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0)
	{
		c = tolower(c);
		len = strlen(find);

		do
		{
			do
			{
				if ((sc = *s++) == 0)
				{
					return NULL;
				}
			}
			while (tolower(sc) != c);
		}
		while (strncasecmp(s, find, len) != 0);

		s--;
	}

	return s;
}

#ifndef __CPROTO__

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

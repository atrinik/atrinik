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
 * Win32 related compatibility functions. */

#include <global.h>

#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <mmsystem.h>

struct itimerval
{
	/* next value */
	struct timeval it_interval;
	/* current value */
	struct timeval it_value;
};

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_VIRT    1
#define ITIMER_PROF    2

/**
 * Gets the time of the day.
 * @param tv Will receive the time of the day.
 * @param timezone_Info Will receive the timezone info.
 * @return 0. */
int gettimeofday(struct timeval *tv, struct timezone *timezone_Info)
{
	FILETIME time;
	double timed;

	GetSystemTimeAsFileTime(&time);

	/* Apparently Win32 has units of 1e-7 sec (tenths of microsecs)
	 * 4294967296 is 2^32, to shift high word over
	 * 11644473600 is the number of seconds between
	 * the Win32 epoch 1601-Jan-01 and the Unix epoch 1970-Jan-01
	 * Tests found floating point to be 10x faster than 64bit int math. */
	timed = ((time.dwHighDateTime * 4294967296e-7) - 11644473600.0) + (time.dwLowDateTime * 1e-7);

	tv->tv_sec  = (long) timed;
	tv->tv_usec = (long) ((timed - tv->tv_sec) * 1e6);

	/* Get the timezone, if they want it */
	if (timezone_Info != NULL)
	{
		_tzset();

		timezone_Info->tz_minuteswest = _timezone;
		timezone_Info->tz_dsttime = _daylight;
	}

	return 0;
}

/**
 * Opens a directory for reading. The handle should be disposed through closedir().
 * @param dir Directory path.
 * @return Directory handle, NULL if failure. */
DIR *opendir(const char *dir)
{
	DIR *dp;
	char *filespec;
	long handle;
	int index;

	filespec = malloc(strlen(dir) + 2 + 1);
	strcpy(filespec, dir);
	index = strlen(filespec) - 1;

	if (index >= 0 && (filespec[index] == '/' || filespec[index] == '\\'))
	{
		filespec[index] = '\0';
	}

	strcat(filespec, "/*");

	dp = (DIR *) malloc(sizeof(DIR));
	dp->offset = 0;
	dp->finished = 0;
	dp->dir = strdup(dir);

	if ((handle = _findfirst(filespec, &(dp->fileinfo))) < 0)
	{
		free(filespec);
		free(dp);
		return NULL;
	}

	dp->handle = handle;
	free(filespec);

	return dp;
}

/**
 * Returns the next file/directory for specified directory handle,
 * obtained through a call to opendir().
 * @param dp Handle.
 * @return Next file/directory, NULL if end reached. */
struct dirent *readdir(DIR *dp)
{
	if (!dp || dp->finished)
	{
		return NULL;
	}

	if (dp->offset != 0)
	{
		if (_findnext(dp->handle, &(dp->fileinfo)) < 0)
		{
			dp->finished = 1;
			return NULL;
		}
	}

	dp->offset++;

	strncpy(dp->dent.d_name, dp->fileinfo.name, _MAX_FNAME);
	dp->dent.d_ino = 1;
	dp->dent.d_reclen = strlen(dp->dent.d_name);
	dp->dent.d_off = dp->offset;

	return &(dp->dent);
}

/**
 * Dispose of a directory handle.
 * @param dp Handle to free. Will become invalid.
 * @return 0. */
int closedir(DIR *dp)
{
	if (!dp)
	{
		return 0;
	}

	_findclose(dp->handle);

	if (dp->dir)
	{
		free(dp->dir);
	}

	if (dp)
	{
		free(dp);
	}

	return 0;
}

/**
 * Restart a directory listing from the beginning.
 * @param dir_Info Handle to rewind. */
void rewinddir(DIR *dir_Info)
{
	/* Re-set to the beginning */
	char *filespec;
	long handle;
	int index;

	dir_Info->handle = 0;
	dir_Info->offset = 0;
	dir_Info->finished = 0;

	filespec = malloc(strlen(dir_Info->dir) + 2 + 1);
	strcpy(filespec, dir_Info->dir);
	index = strlen(filespec) - 1;

	if (index >= 0 && (filespec[index] == '/' || filespec[index] == '\\'))
	{
		filespec[index] = '\0';
	}

	strcat(filespec, "/*");

	if ((handle = _findfirst(filespec, &(dir_Info->fileinfo))) < 0)
	{
		if (errno == ENOENT)
		{
			dir_Info->finished = 1;
		}
	}

	dir_Info->handle = handle;
	free(filespec);
}

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

#include <global.h>

#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <mmsystem.h>

struct timezone
{
	int tz_minuteswest;
	int tz_dsttime;
};


struct itimerval
{
	/* next value */
	struct timeval it_interval;
	/* current value */
	struct timeval it_value;
};

#define ITIMER_REAL    0		/*generates sigalrm */
#define ITIMER_VIRTUAL 1		/*generates sigvtalrm */
#define ITIMER_VIRT    1		/*generates sigvtalrm */
#define ITIMER_PROF    2		/*generates sigprof */


/* Functions to capsule or serve linux style function
 * for Windows Visual C++ */
int gettimeofday(struct timeval *time_Info, struct timezone *timezone_Info)
{
	/* remarks: a DWORD is an unsigned long */
	static DWORD time_t0, time_delta, mm_t0;
	static int t_initialized = 0;
	DWORD mm_t, delta_t;

	if (!t_initialized)
	{
		time_t0 = time(NULL);
		time_delta = 0;
		mm_t0 = timeGetTime();
		t_initialized = 1;
	}

	/* Get the time, if they want it */
	if (time_Info != NULL)
	{
		/* timeGetTime() returns the system time in milliseconds */
		mm_t = timeGetTime();

		/* handle wrap around of system time (happens every
		  * 2^32 milliseconds = 49.71 days) */
		if (mm_t < mm_t0 )
			delta_t = (0xffffffff - mm_t0) + mm_t + 1;
		else
			delta_t = mm_t - mm_t0;

		mm_t0 = mm_t;

		time_delta += delta_t;
		if (time_delta >= 1000 )
		{
			time_t0 += time_delta / 1000;
			time_delta = time_delta % 1000;
		}
		time_Info->tv_sec = time_t0;
		time_Info->tv_usec = time_delta * 1000;
	}

	/* Get the timezone, if they want it */
	if (timezone_Info != NULL)
	{
		_tzset();
		timezone_Info->tz_minuteswest = _timezone;
		timezone_Info->tz_dsttime = _daylight;
	}
	/* And return */
	return 0;
}


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
		filespec[index] = '\0';
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

struct dirent *readdir(DIR * dp)
{
	if (!dp || dp->finished)
		return NULL;

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

int closedir(DIR * dp)
{
	if (!dp)
		return 0;

	_findclose(dp->handle);
	if (dp->dir)
		free(dp->dir);
	if (dp)
		free(dp);

	return 0;
}

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
		filespec[index] = '\0';
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

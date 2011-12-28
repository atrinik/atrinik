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
 * Standard includes. */

#ifndef INCLUDES_H
#define INCLUDES_H

#include <config.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pthread.h>

#include <porting.h>
#include <toolkit.h>

#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#ifdef HAVE_TIME_H
#	include <time.h>
#endif

#ifdef HAVE_STDDEF_H
#	include <stddef.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#ifdef LINUX
#	include <netdb.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#endif

#ifdef HAVE_DIRENT_H
#	include <dirent.h>
#	define NAMLEN(dirent) (strlen((dirent)->d_name))
#elif defined(HAVE_SYS_NDIR_H) || defined(HAVE_SYS_DIR_H) || defined(HAVE_NDIR_H)
#	define dirent direct
#	define NAMLEN(dirent) ((dirent)->d_namlen)
#	ifdef HAVE_SYS_NDIR_H
#		include <sys/ndir.h>
#	endif
#	ifdef HAVE_SYS_DIR_H
#		include <sys/dir.h>
#	endif
#	ifdef HAVE_NDIR_H
#		include <ndir.h>
#	endif
#endif

#ifdef HAVE_SRANDOM
#	define RANDOM() random()
#	define SRANDOM(xyz) srandom(xyz)
#else
#	ifdef HAVE_SRAND48
#		define RANDOM() lrand48()
#		define SRANDOM(xyz) srand48(xyz)
#	else
#		ifdef HAVE_SRAND
#			define RANDOM() rand()
#			define SRANDOM(xyz) srand(xyz)
#		else
#			error "Could not find a usable random routine"
#		endif
#	endif
#endif

#ifdef HAVE_STRICMP
#	define strcasecmp(_s1_, _s2_) stricmp(_s1_, _s2_)
#endif

#ifdef HAVE_STRNICMP
#	define strncasecmp(_s1_, _s2_, _nrof_) strnicmp(_s1_, _s2_, _nrof_)
#endif

#ifndef MIN
#	define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#	define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef FABS
#	define FABS(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef ABS
#	define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

/* Make sure M_PI is defined. */
#ifndef M_PI
#	define M_PI 3.141592654
#endif

#ifndef F_OK
#	define F_OK 6
#endif

#ifndef R_OK
#	define R_OK 6
#endif

#ifndef W_OK
#	define W_OK 2
#endif

#ifndef MSG_DONTWAIT
#	define MSG_DONTWAIT 0
#endif

#ifndef HAVE_GETTIMEOFDAY
struct timezone
{
	/* Minutes west of Greenwich. */
	int tz_minuteswest;
	/* Type of DST correction. */
	int tz_dsttime;
};

extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#include <uthash.h>
#include <define.h>
#include <version.h>
#include <logger.h>
#include <newclient.h>

#endif

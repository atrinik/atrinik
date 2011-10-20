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

/* Include this first, because it lets us know what we are missing */
#ifdef WIN32
#	include "win32.h"
#else
#	include <cmake.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>

#ifdef HAVE_FCNTL_H
#	include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

#if defined(HAVE_TIME_H)
#	include <time.h>
#endif

/* stddef is for offsetof */
#ifdef HAVE_STDDEF_H
#	include <stddef.h>
#endif

#ifndef WIN32
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <uthash.h>
#include <pthread.h>

#include "config.h"
#include "define.h"
#include "version.h"
#include "logger.h"
#include "newclient.h"

#ifdef HAVE_STRICMP
#	define strcasecmp(_s1_, _s2_) stricmp(_s1_, _s2_)
#endif

#ifdef HAVE_STRNICMP
#	define strncasecmp(_s1_, _s2_, _nrof_) strnicmp(_s1_, _s2_, _nrof_)
#endif

#if defined(vax) || defined(ibm032)
size_t strftime(char *, size_t, const char *, const struct tm *);
time_t mktime(struct tm *);
#endif

#ifndef WIN32
#	ifdef HAVE_DIRENT_H
#		include <dirent.h>
#		define NAMLEN(dirent) strlen((dirent)->d_name)
#	else
#		define dirent direct
#		define NAMLEN(dirent) (dirent)->d_namlen
#		ifdef HAVE_SYS_NDIR_H
#			include <sys/ndir.h>
#		endif
#		ifdef HAVE_SYS_DIR_H
#			include <sys/dir.h>
#		endif
#		ifdef HAVE_NDIR_H
#			include <ndir.h>
#		endif
#	endif
#endif

#endif

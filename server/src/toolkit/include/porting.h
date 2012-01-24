/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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
 * Cross-platform support header file. */

#ifndef PORTING_H
#define PORTING_H

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

/* If we're not using GNU C, ignore __attribute__ */
#ifndef __GNUC__
#	define  __attribute__(x)
#endif

#ifdef WIN32
#	ifndef STRICT
#		define STRICT
#	endif

#	if _MSC_VER > 1000
#		pragma once
#	endif

#	define WIN32_LEAN_AND_MEAN

#	include <windows.h>
#	include <windowsx.h>
#	include <mmsystem.h>
#	include <winsock2.h>
#	include <io.h>
#	include <malloc.h>
#	include <direct.h>
#	include <shellapi.h>

#	ifdef MINGW
#		define HAVE_DIRENT_H
#		define _set_fmode(_mode) \
		{ \
			_fmode = (_mode); \
		}
#	endif

#	define mkdir(__a, __b) _mkdir(__a)
#	define socklen_t int
#	define sleep(_x) Sleep((_x) * 1000)

#	define HAVE_STRICMP
#	define HAVE_STRNICMP
#	define HAVE_ZLIB
#	define HAVE_STRERROR
#	define HAVE_SRAND
#	define HAVE_FCNTL_H
#	define HAVE_TIME_H
#	define HAVE_STDDEF_H

#	define PLUGIN_SUFFIX ".dll"
#else
#	include <cmake.h>
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <pthread.h>

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

/** uint32 */
typedef unsigned int uint32;
#ifndef UINT32_MAX
#	define UINT32_MAX (4294967295U)
#endif

/** sint32 */
typedef signed int sint32;
#define SINT32_MIN (-2147483647 - 1)
#define SINT32_MAX 2147483647

/** uint16 */
typedef unsigned short uint16;
#ifndef UINT16_MAX
#	define UINT16_MAX (65535U)
#endif

/** sint16 */
typedef signed short sint16;
#define SINT16_MIN (-32767 - 1)
#define SINT16_MAX (32767)

/** uint8 */
typedef unsigned char uint8;
#ifndef UINT8_MAX
#	define UINT8_MAX (255U)
#endif

/** sint8 */
typedef signed char sint8;
#define SINT8_MIN (-128)
#define SINT8_MAX (127)

/** Used for faces. */
typedef unsigned short Fontindex;

/** Object unique IDs. */
typedef unsigned int tag_t;

#ifdef WIN32
	typedef unsigned __int64            uint64;
	typedef signed __int64              sint64;
#	define atoll                        _atoi64

#	define FMT64                        "I64d"
#	define FMT64U                       "I64u"
#	define FMT64HEX                     "I64x"
#else
#	if SIZEOF_LONG == 8
		typedef unsigned long           uint64;
		typedef signed long             sint64;
#		define FMT64                    "ld"
#		define FMT64U                   "lu"
#		define FMT64HEX                 "lx"

#	elif SIZEOF_LONG_LONG == 8
		typedef unsigned long long      uint64;
		typedef signed long long        sint64;
#		define FMT64                    "lld"
#		define FMT64U                   "llu"
#		define FMT64HEX                 "llx"

#	else
#		error Do not know how to get a 64 bit value on this system.
#		error Correct and send email to the Atrinik Team on how to do this.
#	endif
#endif

#ifndef UINT64_MAX
#	define UINT64_MAX (18446744073709551615LLU)
#endif

#define SINT64_MIN (-9223372036854775807LL - 1)
#define SINT64_MAX (9223372036854775807LL)

/* Only C99 has lrint. */
#ifndef _ISOC99_SOURCE
#	define lrint(x) (floor((x) + ((x) > 0) ? 0.5 : -0.5))
#endif

#ifndef HAVE_STRTOK_R
extern char *strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#ifndef HAVE_TEMPNAM
extern char *tempnam(const char *dir, const char *pfx);
#endif

#ifndef HAVE_STRDUP
extern char *strdup(const char *s);
#endif

#ifndef HAVE_STRNDUP
extern char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_STRERROR
extern char *strerror(int errnum);
#endif

#ifndef HAVE_STRCASESTR
extern const char *strcasestr(const char *haystack, const char *needle);
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

#ifndef HAVE_GETLINE
extern ssize_t getline(char **lineptr, size_t *n, FILE *stream);
#endif

#endif

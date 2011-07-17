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
 * The main include file, included by most C files. */

#ifndef GLOBAL_H
#define GLOBAL_H

/* If we're not using GNU C, ignore __attribute__ */
#ifndef __GNUC__
#	define  __attribute__(x)
#endif

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <curl/curl.h>
#include <zlib.h>

#ifndef WIN32
#	include <cmake.h>
#else
#	include "win32.h"
#endif

#include <config.h>

/** Unsigned 32-bit integer. */
typedef unsigned int uint32;
/** Signed 32-bit integer. */
typedef signed int sint32;
/** Unsigned 16-bit integer. */
typedef unsigned short uint16;
/** Signed 16-bit integer. */
typedef signed short sint16;
/** Unsigned 8-bit integer. */
typedef unsigned char uint8;
/** Signed 8-bit integer. */
typedef signed char sint8;

#ifdef WIN32
	typedef unsigned __int64            uint64;
	typedef signed __int64              sint64;
#	define atoll                        _atoi64
#	define FMT64                        "I64d"
#	define FMT64U                       "I64u"
#else
#	if SIZEOF_LONG == 8
		typedef unsigned long           uint64;
		typedef signed long             sint64;
#		define FMT64                    "ld"
#		define FMT64U                   "lu"

#	elif SIZEOF_LONG_LONG == 8
		typedef unsigned long long      uint64;
		typedef signed long long        sint64;
#		define FMT64                    "lld"
#		define FMT64U                   "llu"

#	else
#		error Do not know how to get a 64 bit value on this system.
#		error Correct and send email to the Atrinik Team on how to do this.
#	endif
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

/* Include standard headers. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef HAVE_STDDEF_H
#	include <stddef.h>
#endif

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

#ifdef LINUX
#	include <netdb.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#ifdef HAVE_SDL_MIXER
#	include <SDL_mixer.h>
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

#ifndef _ISOC99_SOURCE
#	define lrint(x) (floor((x) + ((x) > 0) ? 0.5 : -0.5))
#endif

/** The log levels. */
typedef enum LogLevel
{
	/** An irrecoverable error. */
	llevError,
	/** Bug report. */
	llevBug,
	/** Debugging message. */
	llevDebug,
	/** Information. */
	llevInfo
} LogLevel;

#include <item.h>
#include <text.h>
#include <curl.h>
#include <book.h>
#include <commands.h>
#include <main.h>
#include <client.h>
#include <effects.h>
#include <sprite.h>
#include <widget.h>
#include <textwin.h>
#include <player.h>
#include <party.h>
#include <misc.h>
#include <event.h>
#include <ignore.h>
#include <sound.h>
#include <map.h>
#include <scripts.h>
#include <inventory.h>
#include <menu.h>
#include <list.h>
#include <button.h>
#include <popup.h>
#include <server_settings.h>
#include <server_files.h>
#include <image.h>
#include <settings.h>
#include <keybind.h>
#include <binreloc.h>
#include <sha1.h>
#include <progress.h>

#ifndef __CPROTO__
#	include <proto.h>
#endif

#endif

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

#ifndef INCLUDE_H
#define INCLUDE_H

/* If we're not using GNU C, ignore __attribute__ */
#ifndef __GNUC__
#	define  __attribute__(x)
#endif

#ifndef WIN32
#	include "define.h"
#else
#	include "win32.h"
#endif

#include "config.h"

/* This is for the DevCpp IDE */
#ifndef __WIN_32
#	ifdef WIN32
#		define __WIN_32
#	endif
#endif

typedef unsigned int uint32;
typedef signed int sint32;
typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned char uint8;
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

#define RANDOM() rand()
#define SRANDOM(xyz) srand(xyz)

#define FABS(x) ((x) < 0 ? -(x) : (x))

#include <SDL_mixer.h>

#ifdef WIN32
#	include "win32.h"
#else
#	ifdef HAVE_SYS_STAT_H
#		include <sys/stat.h>
#	endif

#	ifdef HAVE_SYS_TIME_H
#		include <sys/time.h>
#	endif

#	include <time.h>

#	ifdef HAVE_STRING_H
#		include <string.h>
#	endif

#	ifdef HAVE_UNISTD_H
#		include <unistd.h>
#	endif

#	ifdef HAVE_FCNTL_H
#		include <fcntl.h>
#	endif

#	ifdef HAVE_DMALLOC_H
#		include <dmalloc.h>
#	endif

#	include <sys/types.h>
#	include <errno.h>
#	include <ctype.h>
#	include <stdarg.h>
#	include <stddef.h>
#	include <netdb.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <arpa/inet.h>
#	include <SDL.h>
#	include <SDL_main.h>
#	include <SDL_image.h>
#	include <SDL_ttf.h>

	typedef int SOCKET;
#endif

#if !defined(WIN32) || defined(MINGW)
#	include <dirent.h>
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

#include <signal.h>
#include <curl/curl.h>

#include <zlib.h>
#include <item.h>

#include <text.h>
#include <curl.h>
#include <book.h>
#include <sdlsocket.h>
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
#include <dialog.h>
#include <list.h>
#include <button.h>
#include <popup.h>
#include <server_settings.h>
#include <server_files.h>
#include <image.h>

#ifndef __CPROTO__
#	include <proto.h>
#endif

#endif

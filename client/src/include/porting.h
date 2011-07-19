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
 * Porting header file. */

#ifndef PORTING_H
#define PORTING_H

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
#	endif

#	define mkdir(__a, __b) _mkdir(__a)
#	define socklen_t int

#	define HAVE_STRICMP
#	define HAVE_STRNICMP
#	define HAVE_SDL
#	define HAVE_SDL_IMAGE
#	define HAVE_SDL_TTF
#	define HAVE_CURL
#	define HAVE_ZLIB
#	define HAVE_SDL_MIXER
#	define HAVE_STRERROR
#	define HAVE_SRAND
#	define HAVE_FCNTL_H
#	define HAVE_TIME_H
#	define HAVE_STDDEF_H
#else
#	include <cmake.h>
#endif

/* Figure out the size of 64-bit integer. */
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

/* Only C99 has lrint. */
#ifndef _ISOC99_SOURCE
#	define lrint(x) (floor((x) + ((x) > 0) ? 0.5 : -0.5))
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

#ifndef HAVE_STRTOK_R
char *strtok_r(char *s, const char *delim, char **save_ptr);
#endif

#endif

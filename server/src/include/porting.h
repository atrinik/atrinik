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

/** This should be used to differentiate shared strings from normal strings. */
typedef const char shstr;

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

#endif

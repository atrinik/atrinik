/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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

#ifndef MIN
#	define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#	define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define FABS(x) ((x) < 0 ? -(x) : (x))

/* Just so that the entries in proto.h don't error on this... */
#ifndef INSTALL_SOUND
#define Mix_Chunk int
#define Mix_Music int
#endif

#ifdef INSTALL_SOUND
#	include <SDL_mixer.h>
#endif

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
#	include <arpa/inet.h>
#	include <SDL.h>
#	include <SDL_main.h>
#	include <SDL_image.h>

#	define SOCKET int
#endif

/** The log levels. */
typedef enum LogLevel
{
	/** A message. */
	llevMsg,
	/** An error. */
	llevError,
	/** Debugging message. */
	llevDebug
} LogLevel;

/** Default log level. */
#define LOGLEVEL llevDebug

#ifdef INSTALL_SOUND
Mix_Chunk *Mix_LoadWAV_wrapper(const char *fname);
Mix_Music *Mix_LoadMUS_wrapper(const char *file);
#endif

#include <signal.h>
#include <curl/curl.h>

#include <zlib.h>
#include <item.h>

#include <book.h>
#include <client.h>
#include <sdlsocket.h>
#include <commands.h>
#include <main.h>
#include <metaserver.h>
#include <player.h>
#include <party.h>
#include <misc.h>
#include <event.h>
#include <ignore.h>
#include <sound.h>
#include <map.h>
#include <sprite.h>
#include <player_shop.h>
#include <scripts.h>
#include <textwin.h>
#include <inventory.h>
#include <menu.h>
#include <dialog.h>
#include <widget.h>

#ifndef __CPROTO__
#	include <proto.h>
#endif

#endif

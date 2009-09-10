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

#if !defined(__INCLUDE_H)
#define __INCLUDE_H

#ifdef __LINUX
#include "define.h"
#else
#include "win32.h"
#endif

#include "config.h"

/* Just some handy ones I like to use */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* This is for the DevCpp IDE */
#ifndef __WIN_32
#ifdef WIN32
#define __WIN_32
#endif
#endif

typedef unsigned int uint32;
typedef signed int sint32;
typedef unsigned short uint16;
typedef signed short sint16;
typedef unsigned char uint8;
typedef signed char sint8;

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifdef INSTALL_SOUND
#include <SDL_mixer.h>
#endif
#include <wrapper.h>
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
#include <sound.h>
#include <map.h>
#include <sprite.h>
#include <player_shop.h>
#include <textwin.h>
#include <inventory.h>
#include <menu.h>
#include <dialog.h>
#include <widget.h>
#endif

/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * The main include file, included by most C files.
 */

#ifndef GLOBAL_H
#define GLOBAL_H

#ifndef WINVER
#define WINVER 0x502
#endif

/* Include standard headers. */
#include <SDL.h>
#include <SDL_main.h>
#include <SDL_syswm.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <zlib.h>
#include <pthread.h>
#include <config.h>
#include <toolkit/toolkit.h>
#include <toolkit/socket.h>
#include <toolkit/shstr.h>
#include <toolkit/memory.h>
#include <toolkit/mempool.h>

#ifdef HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

#include <version.h>
#include <scrollbar.h>
#include <item.h>
#include <text.h>
#include <text_input.h>
#include <texture.h>
#include <toolkit/curl.h>
#include <book.h>
#include <interface.h>
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
#include <inventory.h>
#include <menu.h>
#include <list.h>
#include <button.h>
#include <color_picker.h>
#include <popup.h>
#include <server_settings.h>
#include <server_files.h>
#include <image.h>
#include <settings.h>
#include <keybind.h>
#include <toolkit/sha1.h>
#include <progress.h>
#include <updater.h>

/* TODO: Remove from here eventually */
struct packet_struct;

/* TODO: move to appropriate header */
FILE *
client_fopen_wrapper(const char *fname, const char *mode);

#ifndef __CPROTO__
#include <proto.h>
#endif

#endif

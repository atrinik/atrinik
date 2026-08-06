/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

static inline SDL_MouseButtonFlags mouse_get_state(int *x, int *y) {
    float mouse_x, mouse_y;
    SDL_MouseButtonFlags state =
        SDL_GetMouseState(x != NULL ? &mouse_x : NULL, y != NULL ? &mouse_y : NULL);

    if (x != NULL) {
        *x = (int)mouse_x;
    }
    if (y != NULL) {
        *y = (int)mouse_y;
    }

    return state;
}
#include <zlib.h>
#include <pthread.h>
#include <config.h>
#include <toolkit/toolkit.h>
#include <toolkit/socket.h>
#include <toolkit/shstr.h>
#include <toolkit/memory.h>
#include <toolkit/mempool.h>

/* Shared opaque types used by component API declarations. */
typedef struct Animations Animations;
typedef struct Player_Struct Client_Player;
typedef struct _anim_table _anim_table;
typedef struct clioption_settings_struct clioption_settings_struct;
typedef struct client_socket client_socket_t;
typedef struct command_buffer command_buffer;
typedef struct server_struct server_struct;
typedef struct widgetdata widgetdata;
struct packet_struct;
struct packet_reader;

#ifdef HAVE_SDL_MIXER
#include <SDL3_mixer/SDL_mixer.h>
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
#include <surface_primitives.h>
#include <widget.h>
#include <textwin.h>
#include <player.h>
#include <party.h>
#include <misc.h>
#include <event.h>
#include <ignore.h>
#include <sound.h>
#include <map.h>
#include <lighting.h>
#include <render_profiler.h>
#include <inventory.h>
#include <menu.h>
#include <list.h>
#include <button.h>
#include <color_picker.h>
#include <popup.h>
#include <server_settings.h>
#include <server_files.h>
#include <asset.h>
#include <asset_source.h>
#include <image.h>
#include <settings.h>
#include <keybind.h>
#include <progress.h>

#endif

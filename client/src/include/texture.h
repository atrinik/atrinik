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
 * Texture system header file.
 */

#ifndef TEXTURE_H
#define TEXTURE_H

/**
 * Texture types.
 */
typedef enum texture_type_t {
    TEXTURE_TYPE_SOFTWARE,
    TEXTURE_TYPE_CLIENT,

    TEXTURE_TYPE_NUM
} texture_type_t;

typedef struct texture_struct {
    char *name;

    texture_type_t type;

    time_t last_used;

    SDL_Surface *surface;

    UT_hash_handle hh;
} texture_struct;

#define TEXTURE_FALLBACK_NAME "texture_fallback"

#define TEXTURE_CLIENT(_name) (texture_surface(texture_get(TEXTURE_TYPE_CLIENT, (_name))))

#define TEXTURE_GC_MAX_TIME 100000
#define TEXTURE_GC_CHANCE 500
#define TEXTURE_GC_FREE_TIME 60 * 15

/** Public API implemented in src/client/texture.c. */

extern void texture_init(void);

extern void texture_deinit(void);

extern void texture_delete(texture_struct *texture);

extern void texture_reload(void);

extern void texture_gc(void);

extern texture_struct *texture_get(texture_type_t type, const char *name);

extern SDL_Surface *texture_surface(texture_struct *texture);

/** Public API implemented in src/gui/widgets/texture.c. */

extern void widget_texture_init(widgetdata *widget);

#endif

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
 * Sprite header file.
 */

#ifndef SPRITE_H
#define SPRITE_H

#define SPRITE_CACHE_GC_MAX_TIME 100000
#define SPRITE_CACHE_GC_CHANCE 500
#define SPRITE_CACHE_GC_FREE_TIME 60 * 15

/**
 * Size of the glow effect in pixels.
 */
#define SPRITE_GLOW_SIZE 2

/**
 * Used to pass data to surface_show_effects().
 */
typedef struct sprite_effects {
    uint32_t flags; ///< Bit combination of @ref SPRITE_FLAG_xxx.
    uint8_t dark_level; ///< Dark level.
    uint8_t alpha; ///< Alpha value.
    uint32_t stretch; ///< Tile stretching value.
    int16_t zoom_x; ///< Horizontal zoom.
    int16_t zoom_y; ///< Vertical zoom.
    int16_t rotate; ///< Rotate value.
    char glow[COLOR_BUF];
    uint8_t glow_speed;
    uint8_t glow_state;
} sprite_effects_t;

#define SPRITE_EFFECTS_NEED_RENDERING(_effects)                                \
    ((_effects)->flags != 0 || (_effects)->alpha != 0 ||                       \
    (_effects)->stretch != 0 || ((_effects)->zoom_x != 0 &&                    \
    (_effects)->zoom_x != 100) || ((_effects)->zoom_y != 0 &&                  \
    (_effects)->zoom_y != 100) || (_effects)->rotate != 0 ||                   \
    (_effects)->glow[0] != '\0')

/**
 * @defgroup SPRITE_FLAG_xxx Sprite drawing flags
 * Sprite drawing flags.
 *@{*/
/** Use darkness. */
#define SPRITE_FLAG_DARK 0
/** Fog of war. */
#define SPRITE_FLAG_FOW 1
/** Red. */
#define SPRITE_FLAG_RED 2
/** Gray. */
#define SPRITE_FLAG_GRAY 3
/** Weather effects overlay. */
#define SPRITE_FLAG_EFFECTS 4
/*@}*/

/** Sprite structure. */
typedef struct sprite_struct {
    /** Rows of blank pixels before first color information. */
    int border_up;

    /** Border down. */
    int border_down;

    /** Border left. */
    int border_left;

    /** Border right. */
    int border_right;

    /** The sprite's bitmap. */
    SDL_Surface *bitmap;
} sprite_struct;

#define BORDER_CREATE_TOP(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x), (_y), (_w), (_thickness), (_color))
#define BORDER_CREATE_BOTTOM(_surface, _x, _y, _w, _h, _color, _thickness)\
    border_create_line((_surface), (_x), (_y) + (_h) - (_thickness), (_w), \
            (_thickness), (_color))
#define BORDER_CREATE_LEFT(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x), (_y), (_thickness), (_h), (_color))
#define BORDER_CREATE_RIGHT(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x) + (_w) - (_thickness), (_y),\
            (_thickness), (_h), (_color))

#endif

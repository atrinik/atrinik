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
    char outline[COLOR_BUF]; ///< Outline-only effect color.
    uint8_t glow_speed;
    uint8_t glow_state;
    int32_t smooth_dark_y; ///< Lightmap row used for smooth structural lighting.
} sprite_effects_t;

#define SPRITE_EFFECTS_NEED_RENDERING(_effects)                                                 \
    (((_effects)->flags &                                                                       \
      ~(BIT_MASK(SPRITE_FLAG_SMOOTH_DARK) | BIT_MASK(SPRITE_FLAG_SMOOTH_DARK_SURFACE))) != 0 || \
     (_effects)->alpha != 0 || (_effects)->stretch != 0 ||                                      \
     ((_effects)->zoom_x != 0 && (_effects)->zoom_x != 100) ||                                  \
     ((_effects)->zoom_y != 0 && (_effects)->zoom_y != 100) || (_effects)->rotate != 0 ||       \
     (_effects)->glow[0] != '\0' || (_effects)->outline[0] != '\0')

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
/** Smooth darkness sampled along an object's map-space base. */
#define SPRITE_FLAG_SMOOTH_DARK 5
/** Smooth darkness sampled at each projected sprite pixel. */
#define SPRITE_FLAG_SMOOTH_DARK_SURFACE 6
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

/**
 * Return whether a surface pixel belongs to its visible silhouette.
 *
 * The caller must lock the surface first when SDL_MUSTLOCK() is true.
 */
bool surface_pixel_visible(SDL_Surface *surface, int x, int y);

#define BORDER_CREATE_TOP(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x), (_y), (_w), (_thickness), (_color))
#define BORDER_CREATE_BOTTOM(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x), (_y) + (_h) - (_thickness), (_w), (_thickness), (_color))
#define BORDER_CREATE_LEFT(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x), (_y), (_thickness), (_h), (_color))
#define BORDER_CREATE_RIGHT(_surface, _x, _y, _w, _h, _color, _thickness) \
    border_create_line((_surface), (_x) + (_w) - (_thickness), (_y), (_thickness), (_h), (_color))

/** Public API implemented in src/client/sprite.c. */

extern SDL_Surface *FormatHolder;

extern void sprite_init_system(void);

extern sprite_struct *sprite_load_file(char *fname, uint32_t flags);

extern sprite_struct *sprite_tryload_file(char *fname, uint32_t flag, SDL_IOStream *rwop);

extern void sprite_free_sprite(sprite_struct *sprite);

extern void sprite_cache_free_all(void);

extern void sprite_cache_gc(void);

extern void surface_show(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src);

extern void surface_show_fill(SDL_Surface *surface,
                              int x,
                              int y,
                              SDL_Rect *srcsize,
                              SDL_Surface *src,
                              SDL_Rect *box);

extern void surface_show_effects(SDL_Surface *surface,
                                 int x,
                                 int y,
                                 SDL_Rect *srcrect,
                                 SDL_Surface *src,
                                 const sprite_effects_t *effects);

extern Uint32 getpixel(SDL_Surface *surface, int x, int y);

extern void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);

extern int surface_borders_get(SDL_Surface *surface,
                               int *top,
                               int *bottom,
                               int *left,
                               int *right,
                               uint32_t color);

extern int
sprite_collision(int x, int y, int x2, int y2, sprite_struct *sprite1, sprite_struct *sprite2);

extern void surface_pan(SDL_Surface *surface, SDL_Rect *box);

extern void draw_frame(SDL_Surface *surface, int x, int y, int w, int h);

extern void border_create(SDL_Surface *surface, int x, int y, int w, int h, int color, int size);

extern void border_create_line(SDL_Surface *surface, int x, int y, int w, int h, uint32_t color);

extern void
border_create_sdl_color(SDL_Surface *surface, SDL_Rect *coords, int thickness, SDL_Color *color);

extern void border_create_color(SDL_Surface *surface,
                                SDL_Rect *coords,
                                int thickness,
                                const char *color_notation);

extern void
border_create_texture(SDL_Surface *surface, SDL_Rect *coords, int thickness, SDL_Surface *texture);

extern void
rectangle_create(SDL_Surface *surface, int x, int y, int w, int h, const char *color_notation);

extern void surface_set_alpha(SDL_Surface *surface, uint8_t alpha);

extern int
polygon_check_coords(double x, double y, double corners_x[], double corners_y[], int corners_num);

/** Public API implemented in src/client/tilestretcher.c. */

extern int tilestretcher_coords_in_tile(uint32_t stretch, int x, int y);

extern void copy_pixel_to_pixel(SDL_Surface *src,
                                SDL_Surface *dest,
                                int x,
                                int y,
                                int x2,
                                int y2,
                                double brightness);

extern void copy_vertical_line(SDL_Surface *src,
                               SDL_Surface *dest,
                               int src_x,
                               int src_sy,
                               int src_ey,
                               int dest_x,
                               int dest_sy,
                               int dest_ey,
                               double brightness,
                               _Bool extra);

extern SDL_Surface *tile_stretch(SDL_Surface *src, int n, int e, int s, int w);

#endif

/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

#ifndef LIGHTING_H
#define LIGHTING_H

/** One light sample projected into map-widget coordinates. */
typedef struct lighting_vertex {
    int x;
    int y;
    uint8_t level;
} lighting_vertex_t;

bool lighting_begin(int width, int height, uint64_t cache_key);
bool lighting_needs_update(void);
void lighting_draw_quad(const lighting_vertex_t vertices[4]);
void lighting_render(SDL_Surface *destination);
void lighting_show_surface(SDL_Surface *destination,
                           int x,
                           int y,
                           SDL_Rect *srcrect,
                           SDL_Surface *source,
                           int sample_y);
void lighting_deinit(void);

#endif

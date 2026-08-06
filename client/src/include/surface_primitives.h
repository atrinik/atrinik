/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2026 Atrinik Development Team                    *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

#ifndef SURFACE_PRIMITIVES_H
#define SURFACE_PRIMITIVES_H

extern SDL_Surface *surface_create_rgb(Uint32 flags,
                                       int width,
                                       int height,
                                       int depth,
                                       Uint32 red_mask,
                                       Uint32 green_mask,
                                       Uint32 blue_mask,
                                       Uint32 alpha_mask);
extern Uint32 pixel_format_map_rgb(SDL_PixelFormat format, Uint8 red, Uint8 green, Uint8 blue);
extern Uint32
pixel_format_map_rgba(SDL_PixelFormat format, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
extern void
pixel_format_get_rgb(Uint32 pixel, SDL_PixelFormat format, Uint8 *red, Uint8 *green, Uint8 *blue);
extern void pixel_format_get_rgba(Uint32 pixel,
                                  SDL_PixelFormat format,
                                  Uint8 *red,
                                  Uint8 *green,
                                  Uint8 *blue,
                                  Uint8 *alpha);
extern SDL_Surface *surface_to_display(SDL_Surface *surface);
extern SDL_Surface *surface_to_display_alpha(SDL_Surface *surface);
extern bool surface_ensure_blittable(SDL_Surface **surface);
extern bool surface_darken_preserve_alpha(SDL_Surface *surface, Uint8 alpha);
extern Uint32 surface_map_rgb(SDL_Surface *surface, Uint8 red, Uint8 green, Uint8 blue);
extern Uint32
surface_map_rgba(SDL_Surface *surface, Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha);
extern void surface_get_rgba(SDL_Surface *surface,
                             Uint32 pixel,
                             Uint8 *red,
                             Uint8 *green,
                             Uint8 *blue,
                             Uint8 *alpha);
extern int
filledRectAlpha(SDL_Surface *surface, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color);
extern int boxRGBA(SDL_Surface *surface,
                   Sint16 x1,
                   Sint16 y1,
                   Sint16 x2,
                   Sint16 y2,
                   Uint8 red,
                   Uint8 green,
                   Uint8 blue,
                   Uint8 alpha);
extern int lineRGBA(SDL_Surface *surface,
                    Sint16 x1,
                    Sint16 y1,
                    Sint16 x2,
                    Sint16 y2,
                    Uint8 red,
                    Uint8 green,
                    Uint8 blue,
                    Uint8 alpha);
extern void rotozoomSurfaceSizeXY(int width,
                                  int height,
                                  double angle,
                                  double zoom_x,
                                  double zoom_y,
                                  int *destination_width,
                                  int *destination_height);
extern void zoomSurfaceSize(int width,
                            int height,
                            double zoom_x,
                            double zoom_y,
                            int *destination_width,
                            int *destination_height);
extern SDL_Surface *zoomSurface(SDL_Surface *surface, double zoom_x, double zoom_y, int smooth);
extern SDL_Surface *rotozoomSurface(SDL_Surface *surface, double angle, double zoom, int smooth);
extern SDL_Surface *
rotozoomSurfaceXY(SDL_Surface *surface, double angle, double zoom_x, double zoom_y, int smooth);

#endif

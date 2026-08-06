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

#ifndef SDL_ROTOZOOM_H
#define SDL_ROTOZOOM_H

/**
 * @file
 * Public declarations for the corresponding client module.
 */

/** Public API implemented by the bundled SDL helper module. */
extern void rotozoomSurfaceSizeXY(int width,
                                  int height,
                                  double angle,
                                  double zoomx,
                                  double zoomy,
                                  int *dstwidth,
                                  int *dstheight);
extern SDL_Surface *rotozoomSurface(SDL_Surface *src, double angle, double zoom, int smooth);
extern SDL_Surface *
rotozoomSurfaceXY(SDL_Surface *src, double angle, double zoomx, double zoomy, int smooth);
extern void
zoomSurfaceSize(int width, int height, double zoomx, double zoomy, int *dstwidth, int *dstheight);
extern SDL_Surface *zoomSurface(SDL_Surface *src, double zoomx, double zoomy, int smooth);

#endif

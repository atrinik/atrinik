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

#ifndef SDL_GFX_H
#define SDL_GFX_H

/**
 * @file
 * Public declarations for the corresponding client module.
 */

/** Public API implemented by the bundled SDL helper module. */
extern int
filledRectAlpha(SDL_Surface *dst, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2, Uint32 color);
extern int boxRGBA(SDL_Surface *dst,
                   Sint16 x,
                   Sint16 y,
                   Sint16 x2,
                   Sint16 y2,
                   Uint8 r,
                   Uint8 g,
                   Uint8 b,
                   Uint8 a);
extern int lineRGBA(SDL_Surface *dst,
                    Sint16 x,
                    Sint16 y,
                    Sint16 x2,
                    Sint16 y2,
                    Uint8 r,
                    Uint8 g,
                    Uint8 b,
                    Uint8 a);

#endif

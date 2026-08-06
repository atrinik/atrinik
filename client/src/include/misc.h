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
 * Misc definitions.
 */

#ifndef MISC_H
#define MISC_H

#define MAX_INPUT_STR 256

/** Public API implemented in src/client/misc.c. */

extern void browser_open(const char *url);

extern char *package_get_version_full(char *dst, size_t dstlen);

extern char *package_get_version_partial(char *dst, size_t dstlen);

extern int bmp2png(const char *path);

extern void screenshot_create(SDL_Surface *surface);

/** Public API implemented in src/client/upgrader.c. */

extern void upgrader_init(void);

extern char *upgrader_get_version_partial(char *dst, size_t dstlen);

#endif

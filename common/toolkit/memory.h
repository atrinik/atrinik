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
 * Fail-fast allocation helpers.
 */

#ifndef TOOLKIT_MEMORY_H
#define TOOLKIT_MEMORY_H

#include "toolkit.h"

/** Allocate at least one byte, aborting on failure. */
void *xmalloc(size_t size);
/** Allocate an array with checked multiplication. */
void *xmallocarray(size_t nmemb, size_t size);
/** Allocate zeroed storage, checking multiplication and aborting on failure. */
void *xcalloc(size_t nmemb, size_t size);
/**
 * Resize storage, aborting on failure. A zero size frees the storage and
 * returns NULL.
 */
void *xrealloc(void *ptr, size_t size);
/** Resize an array with checked multiplication. */
void *xreallocarray(void *ptr, size_t nmemb, size_t size);
/** Duplicate a non-NULL string, aborting on failure. */
char *xstrdup(const char *str);
/** Duplicate at most max_length bytes of a non-NULL string. */
char *xstrndup(const char *str, size_t max_length);

/**
 * Release storage in the module that allocated it.
 *
 * Core code should call free() directly. This function exists for plugin
 * hooks, where allocation and deallocation can cross a shared-library boundary
 * on Windows.
 */
void xfree(void *ptr);

#endif

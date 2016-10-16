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
 * Path API header file.
 *
 * @author Alex Tokar
 */

#ifndef TOOLKIT_PATH_H
#define TOOLKIT_PATH_H

#include "toolkit.h"

/**
 * Prototype for the ::path_fopen function signature.
 *
 * @param filename
 * Filename.
 * @param modes
 * Modes to open the file in.
 */
typedef FILE *(*path_fopen_t)(const char *filename, const char *modes);

/* Prototypes */

path_fopen_t path_fopen;

TOOLKIT_FUNCS_DECLARE(path);

char *
path_join(const char *path, const char *path2);
char *
path_dirname(const char *path);
char *
path_basename(const char *path);
char *
path_normalize(const char *path);
void
path_ensure_directories(const char *path);
int
path_copy_file(const char *src, FILE *dst, const char *mode);
int
path_exists(const char *path);
int
path_touch(const char *path);
size_t
path_size(const char *path);
char *
path_file_contents(const char *path);
int
path_rename(const char *old, const char *new);

#endif

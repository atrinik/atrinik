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

#ifndef WRAPPER_H
#define WRAPPER_H

/**
 * @file
 * Public declarations for the corresponding client module.
 */

/** Public API implemented in src/client/wrapper.c. */

extern void system_start(void);

extern void system_end(void);

extern void mkdir_ensure(const char *path);

extern void copy_file(const char *filename, const char *filename_out);

extern void copy_if_exists(const char *from, const char *to, const char *src, const char *dst);

extern void copy_rec(const char *src, const char *dst);

extern const char *get_config_dir(void);

extern void get_data_dir_file(char *buf, size_t len, const char *fname);

extern char *file_path(const char *path, const char *mode);

extern char *file_path_player(const char *path);

extern char *file_path_server(const char *path);

extern FILE *client_fopen_wrapper(const char *fname, const char *mode);

extern SDL_Surface *IMG_Load_wrapper(const char *file);

extern TTF_Font *TTF_OpenFont_wrapper(const char *file, int ptsize);

#endif

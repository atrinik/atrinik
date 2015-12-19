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
 * Defines for lexical loaders.
 */

#ifndef LOADER_H
#define LOADER_H

/**
 * @defgroup LL_xxx Lexical loader return codes
 *
 * Codes returned by the lexical loader functions.
 *@{
 */
#define LL_EOF      0 ///< End of file reached.
#define LL_MORE     1 ///< Object has more parts that need loading
#define LL_NORMAL   2 ///< Object was successfully loaded.
#define LL_ERROR    3 ///< Error loading.
/*@}*/

/* see loader.l for more details on this */
#define LO_REPEAT   0
#define LO_LINEMODE 1
#define LO_NEWFILE  2
#define LO_NOREAD   3
#define LO_MEMORYMODE 4

/* Prototypes */
void
free_object_loader(void);
void
delete_loader_buffer(void *buffer);
void *
create_loader_buffer(FILE *fp);
int
load_object(const char *str, object *op, int map_flags);
int
load_object_fp(FILE *fp, object *op, int map_flags);
int
load_object_buffer(void *buffer, object *op, int map_flags);
int
set_variable(object *op, const char *buf);
void
get_ob_diff(StringBuffer *sb, object *op, object *op2);

#endif

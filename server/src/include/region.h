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

#ifndef REGION_H
#define REGION_H

#include <decls.h>

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/server/region.c. */

extern region_struct *first_region;

extern void regions_init(void);

extern void regions_free(void);

extern region_struct *region_find_by_name(const char *region_name);

extern const region_struct *region_find_with_map(const region_struct *region);

extern const char *region_get_longname(const region_struct *region);

extern const char *region_get_msg(const region_struct *region);

extern int region_enter_jail(object *op);

#endif

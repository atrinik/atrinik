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

#ifndef SERVER_ITEM_H
#define SERVER_ITEM_H

#include <decls.h>

/**
 * @file
 * Public declarations for the corresponding server module.
 */

/** Public API implemented in src/server/item.c. */

extern StringBuffer *object_get_material(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_material_s(const object *op, const object *caller);

extern StringBuffer *object_get_title(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_title_s(const object *op, const object *caller);

extern StringBuffer *object_get_name(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_name_s(const object *op, const object *caller);

extern StringBuffer *
object_get_short_name(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_short_name_s(const object *op, const object *caller);

extern StringBuffer *
object_get_material_name(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_material_name_s(const object *op, const object *caller);

extern StringBuffer *object_get_base_name(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_base_name_s(const object *op, const object *caller);

extern StringBuffer *
object_get_description_terrain(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_description_terrain_s(const object *op, const object *caller);

extern StringBuffer *
object_get_description_attacks(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_description_attacks_s(const object *op, const object *caller);

extern StringBuffer *
object_get_description_protections(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_description_protections_s(const object *op, const object *caller);

extern StringBuffer *object_get_description_path(const object *op,
                                                 const object *caller,
                                                 const uint32_t path,
                                                 const char *name,
                                                 StringBuffer *sb);

extern char *object_get_description_path_s(const object *op,
                                           const object *caller,
                                           const uint32_t path,
                                           const char *name);

extern StringBuffer *
object_get_description(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_description_s(const object *op, const object *caller);

extern StringBuffer *
object_get_name_description(const object *op, const object *caller, StringBuffer *sb);

extern char *object_get_name_description_s(const object *op, const object *caller);

extern bool need_identify(const object *op);

extern void identify(object *op);

extern void set_trapped_flag(object *op);

#endif

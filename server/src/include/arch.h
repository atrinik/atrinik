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
 * Arch related structures and definitions.
 */

#ifndef ARCH_H
#define ARCH_H

/**
 * The archetype structure is a set of rules on how to generate and manipulate
 * objects which point to archetypes.
 */
typedef struct archetype {
    struct archetype *head; ///< The main part of a linked object.
    struct archetype *more; ///< Next part of a linked object.

    UT_hash_handle hh; ///< Hash handle.

    shstr *name; ///< More definite name, like "kobold".
    object clone; ///< An object from which to do copy_object().
} archetype_t;

/**
 * IDs of archetype pointers cached in #arches.
 * @anchor ARCH_xxx
 */
enum {
    ARCH_WAYPOINT, ///< The 'waypoint' archetype.
    ARCH_EMPTY_ARCHETYPE, ///< The 'empty_archetype' archetype.
    ARCH_BASE_INFO, ///< The 'base_info' archetype.
    ARCH_LEVEL_UP, ///< The 'level_up' archetype.
    ARCH_RING_NORMAL, ///< The 'ring_normal' archetype.
    ARCH_RING_GENERIC, ///< The 'ring_generic' archetype.
    ARCH_AMULET_NORMAL, ///< The 'amulet_normal' archetype.
    ARCH_AMULET_GENERIC, ///< The 'amulet_generic' archetype.

    ARCH_MAX ///< Maximum number of cached archetype pointers.
};

/* Prototypes */

#ifndef __CPROTO__

archetype_t *arch_table;
archetype_t *arches[ARCH_MAX];
bool arch_in_init;
archetype_t *wp_archetype;

void arch_init(void);
void arch_deinit(void);
void arch_add(archetype_t *at);
archetype_t *arch_find(const char *name);
object *arch_get(const char *name);
object *arch_to_object(archetype_t *at);
archetype_t *arch_clone(archetype_t *at);

#endif

#endif

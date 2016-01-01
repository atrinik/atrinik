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
 * Handles code for @ref KEY "keys".
 *
 * @author Alex Tokar
 */

#include <global.h>
#include <object_methods.h>
#include <object.h>
#include <key.h>

/**
 * Search object for the needed key to open a door/container.
 *
 * @param op
 * Object to search in.
 * @param locked
 * The object to find the key for.
 * @return
 * The key pointer if found, NULL otherwise.
 */
object *
key_match (object *op, const object *locked)
{
    HARD_ASSERT(op != NULL);
    HARD_ASSERT(locked != NULL);

    FOR_INV_PREPARE(op, tmp) {
        if ((tmp->type == KEY || tmp->type == FORCE) &&
            tmp->slaying == locked->slaying) {
            return tmp;
        }

        if (tmp->type == CONTAINER && tmp->inv != NULL) {
            object *key = key_match(tmp, locked);
            if (key != NULL) {
                return key;
            }
        }
    } FOR_INV_FINISH();

    return NULL;
}

/**
 * Initialize the key type object methods.
 */
OBJECT_TYPE_INIT_DEFINE(key)
{
}

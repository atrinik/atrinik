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
 * Handles god related code. */

#include <global.h>
#include <arch.h>

/**
 * Returns pointer to specified god's object through pntr_to_god_obj().
 * @param name God's name.
 * @return Pointer to god's object, NULL if doesn't match any god. */
object *find_god(const char *name)
{
    archetype_t *at;

    at = arch_find(name);

    if (!at) {
        return NULL;
    }

    return &at->clone;
}

/**
 * Determines if op worships a god. Returns the godname if they do or
 * "none" if they have no god. In the case of an NPC, if they have no
 * god, we give them a random one.
 * @param op Object to get name of.
 * @return God name, "none" if nothing suitable. */
const char *determine_god(object *op)
{
    /* spells */
    if ((op->type == CONE || op->type == SWARM_SPELL) && op->title) {
        if (find_god(op->title)) {
            return op->title;
        }
    }

    return shstr_cons.none;
}

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
 * Artifact related structures.
 */

#ifndef ARTIFACT_H
#define ARTIFACT_H

/* Forward declarations */
struct archetype;

/**
 * The artifact structure.
 */
typedef struct artifact {
    struct artifact *next; ///< Next artifact in the list.
    shstr_list_t *allowed; ///< List of allowed archetypes.

    /**
     * Memory block with artifacts parse commands for loader.l.
     */
    char *parse_text;

    /**
     * The base archetype object.
     */
    struct archetype *def_at;

    /**
     * Name from the def_arch attribute.
     */
    shstr *def_at_name;

    /**
     * Treasure style.
     * @see @ref treasure_style
     */
    int t_style;

    uint16_t chance; ///< Chance.
    uint8_t difficulty; ///< Difficulty.

    /**
     * If set, the artifact will be directly copied to the object,
     * instead of just having the extra attributes added.
     */
    bool copy_artifact:1;

    /**
     * If true, artifact::allowed is a list of disallowed archetypes.
     */
    bool disallowed:1;
} artifact_t;

/**
 * Artifact list structure.
 */
typedef struct artifact_list {
    struct artifact_list *next; ///< Next list.

    struct artifact *items; ///< Artifacts in this artifact list.
    uint16_t total_chance; ///< Sum of chance for all artifacts on this list.

    /**
     * Object type that this list represents. 0 represents unique artifacts
     * (defined with 'Allowed none').
     */
    uint8_t type;
} artifact_list_t;

/* Prototypes */

void artifact_init(void);
void artifact_deinit(void);
artifact_list_t *artifact_list_find(uint8_t type);
artifact_t *artifact_find_type(const char *name, uint8_t type);
void artifact_change_object(artifact_t *art, object *op);
bool artifact_generate(object *op, int difficulty, int t_style);

#endif

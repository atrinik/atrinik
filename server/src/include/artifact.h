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
 * Artifact related structures */

#ifndef ARTIFACT_H
#define ARTIFACT_H

/** The artifact structure. */
typedef struct artifactstruct
{
    /** Memory block with artifacts parse commands for loader.l. */
    char *parse_text;

    /** The fake arch name when chained to arch list. */
    const char *name;

    /** We use this as marker for def_at is valid and quick name access. */
    const char *def_at_name;

    /** Next artifact in the list. */
    struct artifactstruct *next;

    /** List of allowed archetypes. */
    linked_char *allowed;

    /** The base archetype object. */
    archetype def_at;

    /**
     * Treasure style.
     * @see @ref treasure_style */
    int t_style;

    /** Chance. */
    uint16 chance;

    /** Difficulty. */
    uint8 difficulty;

    /**
     * If set, the artifact will be directly copied to the object,
     * instead of just having the extra attributes added. */
    uint8 copy_artifact;
} artifact;

/** Artifact list structure. */
typedef struct artifactliststruct
{
    /** Next list. */
    struct artifactliststruct *next;

    /** Items in this artifact list. */
    struct artifactstruct *items;

    /** Sum of chance for all artifacts on this list. */
    uint16 total_chance;

    /**
     * Object type that this list represents.
     * -1 are "Allowed none" items. */
    sint16 type;
} artifactlist;

#endif

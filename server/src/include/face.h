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
 * Face related structures.
 */

#ifndef FACE_H
#define FACE_H

/**
 * Face structure - this enforces the notion that data is face by
 * face only - you can not change the color of an item - you need to instead
 * create a new face with that color.
 */
typedef struct new_face_struct {
    /** Name of the face. */
    const char *name;

    /**
     * This is the image ID. It should be the same value as its position
     * in the array.
     */
    uint16_t number;
} New_Face;

/** Map look structure. */
typedef struct map_look_struct {
    /** Face. */
    New_Face *face;

    /** Flags. */
    uint8_t flags;
} MapLook;

/** Animations structure */
typedef struct {
    /** Name of the animation sequence. */
    shstr *name;

    /** The different animations. */
    Fontindex *faces;

    /** Where we are in the array. */
    uint16_t num;

    /** How many different faces to animate. */
    uint8_t num_animations;

    /**
     * How many facings (9 and 25 are allowed only with the new ext anim
     * system).
     */
    uint8_t facings;
} Animations;

#endif

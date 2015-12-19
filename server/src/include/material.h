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
 * Defines are needed by @ref living.h, so they must be loaded early.
 */

#ifndef MATERIAL_H
#define MATERIAL_H

/** Number of materials */
#define NROFMATERIALS 13

/** Number of materials real */
#define NROFMATERIALS_REAL 64

#define NUM_MATERIALS_REAL NROFMATERIALS * NROFMATERIALS_REAL + 1

/**
 * @defgroup material_types Material types
 * Material types.
 *@{*/

/** No material. */
#define M_NONE          0
/** Paper */
#define M_PAPER         1
/** Iron */
#define M_IRON          2
/** Glass */
#define M_GLASS         4
/** Leather */
#define M_LEATHER       8
/** Wood */
#define M_WOOD          16
/** Organic */
#define M_ORGANIC       32
/** Stone */
#define M_STONE         64
/** Cloth */
#define M_CLOTH         128
/** Adamant */
#define M_ADAMANT       256
/** Liquid */
#define M_LIQUID        512
/** Soft metal */
#define M_SOFT_METAL    1024
/** Bone */
#define M_BONE          2048
/** Ice */
#define M_ICE           4096
/*@}*/

/** 1-64 */
#define M_START_PAPER           0 * 64 + 1
/** 65 - 128 */
#define M_START_IRON            1 * 64 + 1
/** 129 - 192 */
#define M_START_GLASS           2 * 64 + 1
/** 193 - 256 */
#define M_START_LEATHER         3 * 64 + 1
/** 257 - 320 */
#define M_START_WOOD            4 * 64 + 1
/** 321 - 384 */
#define M_START_ORGANIC         5 * 64 + 1
/** 385 - 448 */
#define M_START_STONE           6 * 64 + 1
/** 449 - 512 */
#define M_START_CLOTH           7 * 64 + 1
/** 513 - 576 */
#define M_START_ADAMANT         8 * 64 + 1
/** 577 - 640 */
#define M_START_LIQUID          9 * 64 + 1
/** 641 - 704 */
#define M_START_SOFT_METAL      10 * 64 + 1
/** 705 - 768 */
#define M_START_BONE            11 * 64 + 1
/** 769 - 832 */
#define M_START_ICE             12 * 64 + 1

/** A single material. */
typedef struct {
    /** Name of the material. */
    const char *name;
} materialtype;

/** A real material. */
typedef struct _material_real_struct {
    /** Name of this material. */
    char name[MAX_BUF];

    /** Material base quality. */
    int quality;

    /** Back-reference to material type. */
    int type;

    /** We can assign a default race for this material. */
    int def_race;
} material_real_struct;

#endif

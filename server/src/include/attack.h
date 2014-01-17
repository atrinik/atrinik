/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Attack and defense system macros, defines, etc. */

#ifndef ATTACK_H
#define ATTACK_H

/**
 * The attack IDs. */
typedef enum _attacks
{
    /** Impact. */
    ATNR_IMPACT,
    /** Slash. */
    ATNR_SLASH,
    /** Cleave. */
    ATNR_CLEAVE,
    /** Pierce. */
    ATNR_PIERCE,
    /** Weapon magic. Common for spells like magic bullet. */
    ATNR_WEAPON_MAGIC,

    /** Fire. */
    ATNR_FIRE,
    /** Cold. */
    ATNR_COLD,
    /** Electricity. */
    ATNR_ELECTRICITY,
    /** Poison, */
    ATNR_POISON,
    /** Acid. */
    ATNR_ACID,

    /** Magic. */
    ATNR_MAGIC,
    /** Mind. */
    ATNR_MIND,
    /** Blind. */
    ATNR_BLIND,
    /** Paralyze. Affected object will be rooted to the spot. */
    ATNR_PARALYZE,
    /** Force. */
    ATNR_FORCE,

    /** Godpower. */
    ATNR_GODPOWER,
    /** Chaos. */
    ATNR_CHAOS,
    /** Drain. */
    ATNR_DRAIN,
    /** Slow. */
    ATNR_SLOW,
    /**
     * Confusion. Affected object will move in random directions until
     * the effect wears off. */
    ATNR_CONFUSION,

    /** Used for internal calculations. */
    ATNR_INTERNAL,

    /** Number of the attacks. */
    NROFATTACKS
} _attacks;

/**
 * Last valid protection, used for treasure generation. */
#define LAST_PROTECTION (ATNR_CONFUSION + 1)

/**
 * @defgroup AT_xxx Attack types
 * Attack types. Used to set attacktype of an object.
 * @deprecated In the process of being phased out, do not use.
 *@{*/
/** 1 */
#define AT_PHYSICAL         0x00000001
/** 2 */
#define AT_MAGIC            0x00000002
/** 128 */
#define AT_DRAIN            0x00000080
/** 1024 */
#define AT_POISON           0x00000400
/** 8388608 */
#define AT_INTERNAL         0x00800000
/*@}*/

#endif

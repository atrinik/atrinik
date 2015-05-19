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
 * Structures and defines related to living objects, including stats
 * of objects. */

#ifndef LIVING_H
#define LIVING_H

/**
 * @defgroup STATS Object statistics */
/*@{*/
/** Strength. */
#define STR             0
/** Dexterity. */
#define DEX             1
/** Constitution. */
#define CON             2
/** Wisdom. */
#define WIS             3
/** Charisma. */
#define CHA             4
/** Intelligence. */
#define INT 5
/** Power. */
#define POW             6
/** Number of stats. */
#define NUM_STATS       7
/*@}*/

/* Changed from NO_STAT to NO_STAT_VAL to fix conflict on
 * AIX systems */

/* needed by skills code -b.t. */
#define NO_STAT_VAL 99

/**
 * Calculates damage based on level. */
#define LEVEL_DAMAGE(level) (float) ((level) > 0 ? 0.75 + (level) * 0.25 : 1.0)

/**
 * Mostly used by "alive" objects, but also by other objects like gates,
 * buttons, waypoints and a number of other objects. */
typedef struct liv {
    /** Experience. */
    int64_t exp;

    /** Hit points. */
    int32_t hp;

    /** Max hit points. */
    int32_t maxhp;

    /** Spell points. Used to cast mage spells. */
    int16_t sp;

    /** Max spell points. */
    int16_t maxsp;

    /** How much food in stomach. 0 = starved. */
    int16_t food;

    /** How much damage this object does when hitting. */
    int16_t dam;

    /** Weapon class. */
    int16_t wc;

    /** Armour class. */
    int16_t ac;

    /**
     * Random value range we add to wc value of attacker:
     * wc + (random() % wc_range). If it's higher than
     * defender's AC then we can hit our enemy. */
    uint8_t wc_range;

    /** Strength. */
    int8_t Str;

    /** Dexterity. */
    int8_t Dex;

    /** Constitution. */
    int8_t Con;

    /** Wisdom. */
    int8_t Wis;

    /** Charisma. */
    int8_t Cha;

    /** Intelligence. */
    int8_t Int;

    /** Power. */
    int8_t Pow;
} living;

#endif

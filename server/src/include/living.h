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

/**
 * @file
 * Structures and defines related to living objects, including stats
 * of objects.
 */

#ifndef LIVING_H
#define LIVING_H

/**
 * @defgroup STATS Object statistics
 */
/*@{*/
/** Strength. */
#define STR 0
/** Dexterity. */
#define DEX 1
/** Constitution. */
#define CON 2
/** Intelligence. */
#define INT 3
/** Power. */
#define POW 4
/** Number of stats. */
#define NUM_STATS 5
/*@}*/

/* Changed from NO_STAT to NO_STAT_VAL to fix conflict on
 * AIX systems */

/* needed by skills code -b.t. */
#define NO_STAT_VAL 99

/**
 * Calculates damage based on level.
 */
#define LEVEL_DAMAGE(level) (float)((level) > 0 ? 0.75 + (level) * 0.25 : 1.0)

/**
 * Mostly used by "alive" objects, but also by other objects like gates,
 * buttons, waypoints and a number of other objects.
 */
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
     * defender's AC then we can hit our enemy.
     */
    uint8_t wc_range;

    /** Strength. */
    int8_t Str;

    /** Dexterity. */
    int8_t Dex;

    /** Constitution. */
    int8_t Con;

    /** Intelligence. */
    int8_t Int;

    /** Power. */
    int8_t Pow;
} living;

/** Public API implemented in src/server/living.c. */

extern double dam_bonus[30 + 1];

extern int wc_bonus[30 + 1];

extern float speed_bonus[30 + 1];

extern double falling_mitigation[30 + 1];

extern uint32_t weight_limit[30 + 1];

extern int learn_spell[30 + 1];

extern int monster_signal_chance[30 + 1];

extern int savethrow[115 + 1];

extern const char *const restore_msg[5];

extern const char *const lose_msg[5];

extern const char *const statname[5];

extern const char *const short_stat_name[5];

extern StringBuffer *depletion_get_tooltip(const object *depletion, StringBuffer *sb);

extern object *depletion_get_or_create(object *op);

extern void set_attr_value(living *stats, int attr, int8_t value);

extern void change_attr_value(living *stats, int attr, int8_t value);

extern int8_t get_attr_value(const living *stats, int attr);

extern void check_stat_bounds(living *stats);

extern void drain_stat(object *op);

extern void drain_specific_stat(object *op, int deplete_stats);

extern void living_update_player(object *op);

extern void living_update_monster(object *op);

extern int living_update(object *op);

extern object *living_get_base_info(object *op);

extern object *living_find_base_info(object *op);

extern void set_mobile_speed(object *op, int idx);

#endif

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
*                                                                       *
* This program is free software; you can redistribute it and/or modify  *
* it under the terms of the GNU General Public License as published by  *
* the Free Software Foundation; either version 3 of the License, or     *
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

/* First written 6 Sep 1994, Nick Williams (njw@cs.city.ac.uk)
 *
 * Initially, I was going to do this as spells, but there is so much
 * crap associated with spells (using sp, time to cast, might happen
 * as WONDER, etc) that I decided it was time to implement skills.  A
 * skill at the moment is merely a "flag" which is placed into the
 * player. The idea is that it will develop into having a rating,
 * which can be improved by practice.  A player could potentially
 * "learn" any number of skills, however this would be dependent on
 * their intelligence.  Perhaps skills should also record when they
 * were learnt, and their "rating" should decay as time goes by,
 * making characters have to either practice their skills, or re-learn
 * them.  BTW: Some skills should be dependent on having
 * tools... e.g. lockpicking would work much better if the player is
 * using a lockpick. */

/* Modification March 3 1995 - skills are expanded from stealing to
 * include another 14 skills. + skill code generalized. Some player
 * classes now may start with skills. -b.t. (thomas@nomad.astro.psu.edu) */

/* Modification April 21 1995 - more skills added, detect magic - detect
 * curse to woodsman - b.t. (thomas@nomad.astro.psu.edu) */

/* Modification May/June 1995 -
 *  HTH skills to allow hitting while moving.
 *  Modified the bargaining skill to allow incremental CHA increase (based on level)
 *  Added the inscription skill, finished play-testing meditation skill.
 *  - b.t. (thomas@astro.psu.edu) */

/* Modification June/July 1995 -
 *  1- Expansion of the skills code to fit within a scheme of multiple categories
 *  of experience. Henceforth, 2 categories of skills will exist: "associated"
 *  skills which are associated with one of the categories of experience, and
 *  "miscellaneous" skills, which are not related to any experience category.
 *  2- Moved the attacking and spellcasting player activities into skills.
 *  Now have "hand weapons" "missile weapons" "throwing" and "spellcasting".
 *  see doc/??? for details on this system.
 *  - b.t. */

/* define this if you want to have player skills stored for
 * faster access from a linked list. If skill tools are heavily used
 * calls to malloc from this code can actually make performance worse.
 * -b.t. */

/* #define LINKED_SKILL_LIST */

#define NO_SKILL_READY -1

enum skillnrs {
    /* 0 */
	/* DISABLED: steal from other players/NPCs */
    SK_STEALING,

	/* open doors without having to bash them */
    SK_LOCKPICKING,

	/* player can hide from monsters */
    SK_HIDING,

	/* can auto-ident arms/armour */
    SK_SMITH,

	/* can auto-ident bows/x-bow/arrows/bolts */
    SK_BOWYER,

    /* 5 */
	/* can auto-identify gems */
    SK_JEWELER,

	/* can auto-identify potions/amulets/containers */
    SK_ALCHEMY,

	/* can auto-identify staffs/rods/wands */
    SK_THAUMATURGY,

	/* can auto-identify scrolls/books */
    SK_LITERACY,

	/* sells equip at Cha + level-based bonus (30 max) */
    SK_BARGAINING,

    /* 10 */
	/* player may 'hop' over 1-2 spaces */
    SK_JUMPING,

	/* player may sense magic in handled items */
    SK_DET_MAGIC,

	/* player may charm unaggressive monsters */
    SK_ORATORY,

	/* Player may pacify hostile monsters once */
    SK_MUSIC,

	/* player may sense cursed items in inventory */
    SK_DET_CURSE,

    /* 15 */
	/* player can find traps better */
    SK_FIND_TRAPS,

	/* player can regain sp/hp at a faster rate */
    SK_MEDITATION,

	/* can attack hand-to-hand, see attack_hth() */
    SK_BOXING,

	/* player attack for fireborn characters */
    SK_FLAME_TOUCH,

	/* can attack hand-to-hand, see attack_hth() */
    SK_KARATE,

    /* 20 */
	/* player moves quickly over hills/mountains  */
    SK_CLIMBING,

	/* player moves quickly through jungle/forest */
    SK_WOODSMAN,

	/* player may write spell scrolls */
    SK_INSCRIPTION,

	/* player can attack with melee weapons */
    SK_MELEE_WEAPON,

	/* player can attack with missile weapons */
    SK_MISSILE_WEAPON,

    /* 25 */
	/* player can throw items */
    SK_THROWING,

	/* player can cast magic spells */
    SK_SPELL_CASTING,

	/* player can remove traps */
    SK_REMOVE_TRAP,

	/* player can set traps - not implemented */
    SK_SET_TRAP,

	/* player use wands/horns/rods */
    SK_USE_MAGIC_ITEM,

    /* 30 */
	/* player can cast cleric spells, regen grace points */
    SK_PRAYING,

	/* player attack for troll, dragon characters */
    SK_CLAWING,

	/* skill for players who can fly. */
	SK_LEVITATION,

	/* disarm removes traps and grabs sometimes trap items for set traps */
    SK_DISARM_TRAPS,

    SK_XBOW_WEAP,

	/* 35 */
    SK_SLING_WEAP,

    SK_IDENTIFY,

    SK_SLASH_WEAP,

    SK_CLEAVE_WEAP,

    SK_PIERCE_WEAP,

	/* 40 */
    SK_TWOHANDS,

    SK_POLEARMS,

	/* always last index! */
    NROFSKILLS
};

typedef struct skill_struct {
	/* how to describe it to the player */
	char *name;

	/* pointer to the skill archetype in the archlist */
	archetype *at;

	/* the experience category to which this skill belongs */
	short category;

	/* base number of ticks it takes to use the skill */
	short time;

	/* base exp gain for this skill */
	long bexp;

	/* level multiplier of exp gain for using this skill */
	float lexp;

	/* primary stat effecting use of this skill */
	short stat1;

	/* secondary stat for this skill */
	short stat2;

	/* tertiary stat for this skill */
	short stat3;
} skill;


extern skill skills[];

/* yet more convenience macros. */
#define USING_SKILL(op, skill) \
	((op)->chosen_skill->stats.sp == skill)

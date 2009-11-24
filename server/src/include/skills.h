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
 * Skills header. */

/** Marks no skill being ready. */
#define NO_SKILL_READY -1

/** Skill numbers. */
enum skillnrs
{
	/** Player can attempt alchemical recipes. */
	SK_ALCHEMY,
	/** Can read books */
	SK_LITERACY,
	/** Sells items to shops at Cha + level-based bonus (30 max) */
	SK_BARGAINING,
	/** Player may write spell scrolls */
	SK_INSCRIPTION,

	/** Can attack hand-to-hand, see attack_hth() */
	SK_BOXING,
	/** Can attack hand-to-hand, see attack_hth() */
	SK_KARATE,
	/** Player can throw items */
	SK_THROWING,

	/** Player can cast magic spells */
	SK_SPELL_CASTING,
	/** Player use wands/horns/rods */
	SK_USE_MAGIC_ITEM,
	/** Player can cast cleric spells and regenerate grace points */
	SK_PRAYING,

	/** Player can find traps better */
	SK_FIND_TRAPS,
	/** Player can remove traps */
	SK_REMOVE_TRAP,

	/** Player can use bows. */
	SK_MISSILE_WEAPON,
	/** Player can use crossbows. */
	SK_XBOW_WEAP,
	/** Player can use slings. */
	SK_SLING_WEAP,

	/** Player can use slash weapons. */
	SK_SLASH_WEAP,
	/** Player can use cleave weapons. */
	SK_CLEAVE_WEAP,
	/** Player can use pierce weapons. */
	SK_PIERCE_WEAP,
	/** Player can attack with impact weapons */
	SK_MELEE_WEAPON,

	/** Player can use two-handed weapons. */
	SK_TWOHANDS,
	/** Player can use polearms. */
	SK_POLEARMS,

	/** Number of the skills, always last. */
	NROFSKILLS
};

/** Skill structure for the skills array. */
typedef struct skill_struct
{
	/** How to describe it to the player */
	char *name;

	/** Pointer to the skill archetype in the archlist */
	archetype *at;

	/** The experience category to which this skill belongs */
	short category;

	/** Base number of ticks it takes to use the skill */
	short time;

	/** Base exp gain for this skill */
	long bexp;

	/** Level multiplier of exp gain for using this skill */
	float lexp;

	/** Primary stat affecting use of this skill */
	short stat1;

	/** Secondary stat for this skill */
	short stat2;

	/** Tertiary stat for this skill */
	short stat3;
} skill;

extern skill skills[];

/* yet more convenience macros. */
#define USING_SKILL(op, skill) \
	((op)->chosen_skill->stats.sp == skill)

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
 * This is the order of the skills structure:
 *  char *name;
 *  short category;  - the associated experience category
 *  short time;      - the base number of ticks it takes to execute skill
 *  long  bexp;      - base exp gain for use of this skill
 *  float lexp;      - level multiplier for experience gain
 *  short stat1;     - primary stat, for linking to exp cat.
 *  short stat2;     - secondary stat ...
 *  short stat3;     - tertiary stat ...
 *
 * About time - this  number is the base for the use of the skill. Level
 * and associated stats can modify the amount of time to use the skill.
 * Time to use the skill is only used when 1) op is a player and
 * 2) the skill is called through do_skill().
 * It is strongly recommended that many skills *not* have a time value.
 *
 * About 'stats' and skill.category - a primary use for stats is determining
 * the associated experience category (see link_skills_to_exp () ).
 * Note that the ordering of the stats is important. Stat1 is the 'primary'
 * stat, stat2 the 'secondary' stat, etc. In this scheme the primary stat
 * is most important for determining the associated experience category.
 * If a skill has the primary stat set to NO_STAT_VAL then it defaults to a
 * 'miscellaneous skill'. */

#ifndef SKILLIST_H
#define SKILLIST_H

/**
 * The default skills array, values can be overwritten by init_skills()
 * in skill_util.c.
 *
 * @warning Don't change the order here without changing the skills.h
 * file */
skill_struct skills[NROFSKILLS] =
{
	{"alchemy", NULL, NULL, EXP_PERSONAL, 10},
	{"common literacy", "This skill allows you to read the common language. You get experience by reading unread books.", NULL, EXP_MENTAL, 0},
	{"bargaining", NULL, NULL, EXP_PERSONAL, 0},
	{"construction", "This skill allows you to modify contents of the game, like your apartment. See nearby construction manager NPC for help and explanation of this skill.", NULL, EXP_PERSONAL, 0},

	{"punching", "This skill is the basic hand-to-hand attack skill. You will use this skill automatically when attacking without a weapon.", NULL, EXP_PHYSICAL, 0},
	{"karate", NULL, NULL, EXP_PHYSICAL, 0},
	{"throwing", "Throwing is the skill to throw weapons in a deadly way at your opponent. This skill has a separate entry in the range selection.", NULL, EXP_AGILITY, 1},

	{"wizardry spells", "This skill allows casting wizardry based spells using mana (spellpoints). This skill is auto-used when casting the spell.", NULL, EXP_MAGICAL, 1},
	{"magic devices", "Possessor can use basic magic items like wands and rods. This skill is auto-used when applying the item.", NULL, EXP_MAGICAL, 4},
	{"divine prayers", NULL, NULL, EXP_WISDOM, 0},

	{"find traps", "This skill will find and reveal a trap near the player. The tile under the player and all around are searched.", NULL, EXP_AGILITY, 0},
	{"remove traps", "After a trap is revealed this skill can remove the trap. The success of removing depends on the agility experience category level and the trap level. If the level of the trap is too high the trap can backfire, so be careful!", NULL, EXP_AGILITY, 0},

	{"bow archery", "This skill will allow the use of bows as range weapons. Bows use arrows as ammunition. This skill is auto-used when a bow is used.", NULL, EXP_AGILITY, 0},
	{"crossbow archery", "This skill will allow the use of crossbows as range weapons. Crossbows use bolts as ammunition. This skill is auto-used when a crossbow is used.", NULL, EXP_AGILITY, 0},
	{"sling archery", "This skill will allow the use of slings as range weapons. Slings use sling stones as ammunition. This skill is auto-used when a sling is used.", NULL, EXP_AGILITY, 0},

	{"slash weapons", "This skill allows the use of swords and all other slash weapons. Weapons of this type do 'slash damage'. This skill is auto-used when the right weapon is applied.", NULL, EXP_PHYSICAL, 0},
	{"cleave weapons", "This skill allows the use of axes and all other cleave weapons. Weapons of this type do 'cleave damage'. This skill is auto-used when the right weapon is applied.", NULL, EXP_PHYSICAL, 0},
	{"pierce weapons", "This skill allows the use of daggers, rapiers and other pierce weapons. Weapons of this type do 'pierce damage'. This skill is auto-used when the right weapon is applied.", NULL, EXP_PHYSICAL, 0},
	{"impact weapons", "This skill allows the use of clubs, maces and other impact weapons. Weapons of this type do 'impact damage'. This skill is auto-used when the right weapon is applied.", NULL, EXP_PHYSICAL, 0},

	{"two-hand mastery", "This skill allows the use of two-hand weapons. This skill is auto-used from other skills.", NULL, EXP_PHYSICAL, 0},
	{"polearm mastery", "This skill allows the use of polearm weapons. This skill is auto-used from other skills.", NULL, EXP_PHYSICAL, 0},
	{"inscription", "Allows writing content into books.", NULL, EXP_MENTAL, 0},
};

#endif

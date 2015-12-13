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
skill_struct skills[NROFSKILLS] = {
    {"alchemy", NULL, 10},
    {"literacy", NULL, 0},
    {"bargaining", NULL, 0},
    {"construction", NULL, 0},
    {"unarmed", NULL, 0},
    {"karate", NULL, 0},
    {"throwing", NULL, 1},
    {"wizardry spells", NULL, 1},
    {"magic devices", NULL, 4},
    {"meditation", NULL, 0},
    {"find traps", NULL, 0},
    {"remove traps", NULL, 0},
    {"bow archery", NULL, 0},
    {"crossbow archery", NULL, 0},
    {"sling archery", NULL, 0},
    {"slash weapons", NULL, 0},
    {"cleave weapons", NULL, 0},
    {"pierce weapons", NULL, 0},
    {"impact weapons", NULL, 0},
    {"two-hand mastery", NULL, 0},
    {"polearm mastery", NULL, 0},
    {"inscription", NULL, 0},
};

#endif

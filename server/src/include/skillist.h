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


/* This is the order of the skills structure:
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
 * and associated stats can modify the amound of time to use the skill.
 * Time to use the skill is only used when 1) op is a player and
 * 2) the skill is called through do_skill().
 * It is strongly recogmended that many skills *not* have a time value.
 *
 * About 'stats' and skill.category - a primary use for stats is determining
 * the associated experience category (see link_skills_to_exp () ).
 * Note that the ordering of the stats is important. Stat1 is the 'primary'
 * stat, stat2 the 'secondary' stat, etc. In this scheme the primary stat
 * is most important for determining the associated experience category.
 * If a skill has the primary stat set to NO_STAT_VAL then it defaults to a
 * 'miscellaneous skill'. */

/* Don't change the order here w/o changing the skills.h file */

/* The default skills array, values can be overwritten by init_skills()
 * in skill_util.c */

skill skills[NROFSKILLS] =
{
	/* 0 */
	{"stealing",           	NULL, EXP_NONE, 0, 0, 	0.1f,	DEX,     		INTELLIGENCE, 	NO_STAT_VAL},
	{"pick locks",         	NULL, EXP_NONE, 0, 50, 	1.5f,   DEX,     		INTELLIGENCE, 	NO_STAT_VAL},
	{"hide in shadows",    	NULL, EXP_NONE, 0, 10, 	2.5f,   DEX,     		CHA, 			NO_STAT_VAL},
	{"smithery lore",      	NULL, 2, 		0, 0, 	0.0f,   NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	{"bowyer lore",        	NULL, 2, 		0, 0, 	0.0f,   NO_STAT_VAL,   	NO_STAT_VAL, 	NO_STAT_VAL},
	/* 5 */
	{"jeweler lore",       	NULL, 2, 		0, 0, 	0.0f, 	NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	{"alchemy",            	NULL, EXP_NONE, 10, 1, 	1.0f, 	INTELLIGENCE, 	WIS, 	  		DEX},
	{"magic lore",         	NULL, 2, 		0, 0, 	0.0f, 	NO_STAT_VAL, 	NO_STAT_VAL,    NO_STAT_VAL},
	{"common literacy",    	NULL, 1, 		0, 1, 	1.0f, 	INTELLIGENCE, 	WIS, 			NO_STAT_VAL},
	{"bargaining",         	NULL, EXP_NONE, 0, 0, 	0.0f,  	NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	/* 10 */
	{"jumping",            	NULL, EXP_NONE, 0, 5, 	2.5f,	NO_STAT_VAL,    NO_STAT_VAL, 	NO_STAT_VAL},
	{"sense magic",        	NULL, EXP_NONE, 10, 10, 1.0f,	POW, 	 		INTELLIGENCE, 	NO_STAT_VAL},
	{"oratory",            	NULL, EXP_NONE, 5, 1, 	2.0f,	CHA,     		INTELLIGENCE, 	NO_STAT_VAL},
	{"singing",            	NULL, EXP_NONE, 5, 1, 	2.0f,	CHA,     		INTELLIGENCE, 	NO_STAT_VAL},
	{"sense curse",        	NULL, EXP_NONE, 10, 10, 1.0f,	WIS,     		POW, 			NO_STAT_VAL},
	/* 15 */
	{"find traps",         	NULL, 0, 		0, 1, 	0.0f,  	DEX, 			NO_STAT_VAL, 	NO_STAT_VAL},
	{"meditation",         	NULL, EXP_NONE, 10, 0, 	0.0f, 	WIS,      		POW,     		INTELLIGENCE},
	{"punching",           	NULL, EXP_NONE, 0, 0, 	1.0f, 	STR,      		DEX, 			NO_STAT_VAL},
	{"flame touch", 	    NULL, EXP_NONE, 0, 0, 	1.0f,  	STR,      		DEX,     		INTELLIGENCE},
	{"karate",             	NULL, EXP_NONE, 0, 0, 	1.0f,   STR,      		DEX, 			NO_STAT_VAL},
	/* 20 */
	{"mountaineer",        	NULL, EXP_NONE, 0, 0, 	0.0f,  	NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	{"ranger lore",        	NULL, 6, 		0, 0, 	0.0f,  	NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	{"inscription",        	NULL, EXP_NONE, 0, 1, 	5.0f,   POW,      		INTELLIGENCE,  	NO_STAT_VAL},
	{"impact weapons",     	NULL, EXP_NONE, 0, 0, 	1.0f,   STR,      		DEX, 			NO_STAT_VAL},
	{"bow archery",        	NULL, EXP_NONE, 0, 0, 	1.0f,   DEX,      		STR, 			NO_STAT_VAL},
	/* 25 */
	{"throwing",           	NULL, EXP_NONE, 1, 0, 	1.0f,   DEX,      		DEX, 			NO_STAT_VAL},
	{"wizardry spells",		NULL, EXP_NONE, 1, 0, 	0.0f,   POW,      		INTELLIGENCE, 	WIS},
	{"remove traps",       	NULL, EXP_NONE, 0, 1, 	0.5f,   DEX,      		INTELLIGENCE, 	NO_STAT_VAL},
	{"set traps",          	NULL, EXP_NONE, 0, 1, 	0.5f,   INTELLIGENCE,  	DEX, 			NO_STAT_VAL},
	{"magic devices",      	NULL, EXP_NONE, 4, 0, 	1.0f,   POW, 			DEX, 			NO_STAT_VAL},
	/* 30 */
	{"divine prayers", 		NULL, 4, 		0, 0, 	0.0f,   WIS,      		POW,     		INTELLIGENCE},
	{"clawing", 	        NULL, EXP_NONE, 0, 0, 	0.0f,   STR,      		DEX, 			NO_STAT_VAL},
	{"levitation",	        NULL, EXP_NONE, 0, 0, 	0.0f, 	NO_STAT_VAL, 	NO_STAT_VAL, 	NO_STAT_VAL},
	{"disarm traps", 	    NULL, EXP_NONE, 0, 1, 	0.5f,   DEX,      		INTELLIGENCE, 	INTELLIGENCE},
	{"crossbow archery",   	NULL, EXP_NONE, 0, 0, 	1.0f,   DEX,      		STR, 			NO_STAT_VAL},
	/* 35 */
	{"sling archery",      	NULL, EXP_NONE, 0, 0, 	1.0f,   DEX,      		STR, 			NO_STAT_VAL},
	{"identify items",     	NULL, EXP_NONE, 10, 1, 	1.0f, 	INTELLIGENCE, 	DEX, 	  		WIS},
	{"slash weapons",      	NULL, EXP_NONE, 0, 0, 	1.0f,   STR,      		DEX, 			NO_STAT_VAL},
	{"cleave weapons",     	NULL, EXP_NONE, 0, 0, 	1.0f,   STR,      		DEX, 			NO_STAT_VAL},
	{"pierce weapons",     	NULL, EXP_NONE, 0, 0, 	1.0f,   STR,      		DEX, 			NO_STAT_VAL},
	/* 40 */
	{"two-hand mastery",   	NULL, EXP_NONE, 0, 0, 	0.0f,   STR,      		DEX,		 	NO_STAT_VAL},
	{"polearm mastery",    	NULL, EXP_NONE, 0, 0, 	0.0f,   STR,      		DEX, 			NO_STAT_VAL}
};




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

#ifndef LIVING_H
#define LIVING_H

#define STR 0
#define DEX 1
#define CON 2
#define WIS 3
#define CHA 4
#define INTELLIGENCE 5
#define POW 6

#define NUM_STATS 7

/* Changed from NO_STAT to NO_STAT_VAL to fix conlfict on
 * AIX systems */

/* needed by skills code -b.t. */
#define NO_STAT_VAL 99

extern char *attacks[NROFATTACKS];
extern char *spellpathnames[NRSPELLPATHS];

extern float cha_bonus[MAX_STAT + 1];
extern int thaco_bonus[MAX_STAT + 1];
extern int turn_bonus[MAX_STAT + 1];
extern int dam_bonus[MAX_STAT + 1];
extern int savethrow[111];
extern int turn_bonus[MAX_STAT + 1];
extern int learn_prayer_chance[MAX_STAT + 1];
extern int learn_spell[];
extern char *restore_msg[NUM_STATS];
extern char *statname[NUM_STATS];
extern char *short_stat_name[NUM_STATS];
extern char *lose_msg[NUM_STATS];
extern float speed_bonus[MAX_STAT + 1];
extern uint32 weight_limit[MAX_STAT + 1];

#ifdef WIN32
#pragma pack(push,1)
#endif

/* Mostly used by "alive" objects */
typedef struct liv {
	/* Experience. */
  	sint32 exp;

	/* Real Hit Points. */
  	sint32 hp;

	/* max hit points */
  	sint32 maxhp;

	/* Spell points.  Used to cast mage spells. */
  	sint16 sp;

	/* Max spell points. */
  	sint16 maxsp;

	/* Grace.  Used to invoke clerical prayers. */
  	sint16 grace;

	/* Grace.  Used to invoke clerical prayers. */
  	sint16 maxgrace;

	/* How much food in stomach.  0 = starved. */
  	sint16 food;

	/* How much damage this object does when hitting */
  	sint16 dam;

	/* Weapon Class and Armour Class */
  	sint16 wc, ac;

	/* thats the random value range we add to wc value of
	 * attacker: wc+(random()%wc_range). If its higher as
	 * defender AC we have hit our enemy. */
  	uint8 wc_range;

	/* the stats */
  	sint8 Str, Dex, Con, Wis, Cha, Int, Pow;

	/* Affects thaco and ac from time to time */
  	sint8 luck;
} living;

extern float lev_damage[MAXLEVEL + 1];

#ifdef WIN32
#pragma pack(pop)
#endif

#endif

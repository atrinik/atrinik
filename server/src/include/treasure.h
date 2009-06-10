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

/*
 * defines and variables used by the artifact generation routines
 */

#ifndef TREASURE_H
#define TREASURE_H

#define CHANCE_FOR_ARTIFACT	20

#define NUM_COINS 4	/* number of coin types */
extern char *coins[NUM_COINS+1];
extern archetype *coins_arch[NUM_COINS];

/* Flags to generate_treasures(): */

/* i really hate all this value not documented. I wasted some times debugging by
 * see that i have included/copy a wrong flag of this kind somehwere. */
enum {
  	GT_ENVIRONMENT = 0x0001,
  	GT_INVISIBLE = 0x0002,
  	GT_STARTEQUIP = 0x0004,
	/* treasure gets applied when inserted in mob! (food eaten, skill applied...) */
  	GT_APPLY = 0x0008,
  	GT_ONLY_GOOD = 0x0010,
  	GT_UPDATE_INV = 0x0020,
	/* set value of all created treasures to 0 */
  	GT_NO_VALUE = 0x0040
};


/* when a treasure got cloned from archlist, we want perhaps change some default
 * values. All values in this structure will override the default arch.
 * TODO: It is a bad way to implement this with a special structure.
 * Because the real arch list is a at runtime not changed, we can grap for example
 * here a clone of the arch, store it in the treasure list and then run the original
 * arch parser over this clone, using the treasure list as script until an END comes.
 * This will allow ANY changes which is possible and we use ony one parser. */
typedef struct _change_arch {
	/* is != NULL, copy this over the original arch name */
    const char *name;

	/* is != NULL, copy this over the original arch name */
    const char *title;

	/* is != NULL, copy this over the original arch name */
    const char *slaying;

    int item_race;

	/* the real, fixed material value */
    int material;

	/* find a material matching this quality */
    int material_quality;

	/* using material_quality, find quality inside this range */
    int material_range;

	/* quality value. It overwrites the material default value */
	int quality;

	/* used for random range */
	int quality_range;
} _change_arch;


/* treasure is one element in a linked list, which together consist of a
 * complete treasure-list.  Any arch can point to a treasure-list
 * to get generated standard treasure when an archetype of that type
 * is generated (from a generator) */

typedef struct treasurestruct {
	/* Which item this link can be */
  	struct archt *item;

	/* If non null, name of list to use instead */
  	const char *name;

	/* Next treasure-item in a linked list */
  	struct treasurestruct *next;

	/* If this item was generated, use */
	/* this link instead of ->next */
  	struct treasurestruct *next_yes;

	/* If this item was not generated, */
	/* then continue here */
  	struct treasurestruct *next_no;

	/* local t_style (will overrule global one) - used from artifacts */
  	int t_style;

	/* value from 0-1000. chance of item is magic. */
  	int magic_chance;

	/* if this value is != 0, use this as fixed magic value.
	 * if it 0, look at magic to generate perhaps a random magic value */
  	int magic_fix;

	/* default = -1 = ignore this value. 0=NEVER make a artifact for this treasure.
	 * 1-100 = % chance of make a artifact from this treasure. */
  	int artifact_chance;

	/* Max magic bonus to item */
  	int magic;

	/* If the entry is a list transition,
	 * it contains the difficulty
	 * required to go to the new list */
  	int difficulty;

	/* random 1 to nrof items are generated */
	uint16 nrof;

	/* will overrule chance: if set (!=-1) it will create 1/chance_single */
  	sint16 chance_fix;

	/* Percent chance for this item */
  	uint8 chance;

	/* override default arch values if set in treasure list */
  	struct _change_arch change_arch;
} treasure;


typedef struct treasureliststruct {
	/* Usually monster-name/combination */
	const char *name;

	/* global style (used from artifacts file) */
	int t_style;

	int artifact_chance;

	/* if set it will overrule total_chance: */
	sint16 chance_fix;

	/* If non-zero, only 1 item on this
	 * list should be generated.  The
	 * total_chance contains the sum of
	 * the chance for this list. */
	sint16 total_chance;

	/* Next treasure-item in linked list */
	struct treasureliststruct *next;

	/* Items in this list, linked */
	struct treasurestruct *items;
} treasurelist;



#endif

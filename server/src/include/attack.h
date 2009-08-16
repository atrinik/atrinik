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

#ifndef ATTACK_H
#define ATTACK_H

/* Ext. Attack & Defense System */

/* Attacktypes:
 * ATNR_... is the attack number that is indexed into the
 * the resist array in the object structure. */

typedef enum _protections
{
	/* = impact: we use physical intern */
	PROTECT_PHYSICAL,
	PROTECT_SLASH,
	PROTECT_CLEAVE,
	PROTECT_PIERCE,
	PROTECT_WEAPON_MAGIC,

	PROTECT_FIRE,
	PROTECT_COLD,
	PROTECT_ELECTRICITY,
	PROTECT_POISON,
	PROTECT_ACID,

	PROTECT_MAGIC,
	PROTECT_MIND,
	PROTECT_BODY,
	PROTECT_PSIONIC,
	PROTECT_FORCE,

	PROTECT_NETHER,
	PROTECT_CHAOS,
	PROTECT_DEATH,
	PROTECT_SPIRITUAL,
	PROTECT_CORRUPTION,

	/* marker - must be last value */
	NROFPROTECTIONS
}_protections;

#define NROFATTACKS		32

/* Note that the last ATNR_ should be one less than NROFATTACKS above
 * since the ATNR starts counting at zero.
 * For compatible loading, these MUST correspond to the same value
 * as the bitmasks below. */

/* = impact. kind of basic damage */
#define ATNR_PHYSICAL		0
#define ATNR_MAGIC		    1
#define ATNR_FIRE		    2
#define ATNR_ELECTRICITY	3
#define ATNR_COLD		    4
#define ATNR_CONFUSION		5
#define ATNR_ACID		    6
#define ATNR_DRAIN		    7
#define ATNR_WEAPONMAGIC	8
#define ATNR_GHOSTHIT		9
#define ATNR_POISON		    10
#define ATNR_SLOW		    11
#define ATNR_PARALYZE		12
#define ATNR_TIME			13
#define ATNR_FEAR		    14
#define ATNR_CANCELLATION	15
#define ATNR_DEPLETE		16
#define ATNR_DEATH		    17
#define ATNR_CHAOS		    18
#define ATNR_COUNTERSPELL	19
#define ATNR_GODPOWER		20
/* extern name is 'purity' */
#define ATNR_HOLYWORD		21
#define ATNR_BLIND		    22
#define ATNR_INTERNAL		23
#define ATNR_LIFE_STEALING	24
#define ATNR_SLASH      	25
#define ATNR_CLEAVE      	26
#define ATNR_PIERCE      	27

#define ATNR_NETHER      	28
#define ATNR_SONIC      	29
#define ATNR_DEMONIC      	30
#define ATNR_PSIONIC      	31

#define AT_PHYSICAL			0x00000001 /*       1 */
#define AT_MAGIC			0x00000002 /*       2 */
#define AT_FIRE				0x00000004 /*       4 */
#define AT_ELECTRICITY		0x00000008 /*   	8 */
#define AT_COLD				0x00000010 /*      	16 */
#define AT_CONFUSION		0x00000020 /*   	32 The spell will use this one */
#define AT_ACID				0x00000040 /*      	64 Things might corrode when hit */
#define AT_DRAIN			0x00000080 /*     	128 */
#define AT_WEAPONMAGIC		0x00000100 /*   	256 Very special, use with care */
#define AT_GHOSTHIT			0x00000200 /*     	512 Attacker dissolves */
#define AT_POISON			0x00000400 /*    	1024 */
#define AT_SLOW				0x00000800 /*    	2048 */
#define AT_PARALYZE			0x00001000 /*    	4096 */
#define AT_TIME				0x00002000 /*    	8192 */
#define AT_FEAR				0x00004000 /*   	16384 */
#define AT_CANCELLATION 	0x00008000 /*   	32768 ylitalo@student.docs.uu.se */
#define AT_DEPLETE      	0x00010000 /*   	65536 vick@bern.docs.uu.se */
#define AT_DEATH        	0x00020000 /*  		131072 peterm@soda.berkeley.edu */
#define AT_CHAOS        	0x00040000 /*  		262144 peterm@soda.berkeley.edu*/
#define AT_COUNTERSPELL 	0x00080000 /*  		524288 peterm@soda.berkeley.edu*/
#define AT_GODPOWER			0x00100000 /* 		1048576  peterm@soda.berkeley.edu */
#define AT_HOLYWORD			0x00200000 /* 		2097152 race selective attack thomas@astro.psu.edu */
#define AT_BLIND			0x00400000 /* 		4194304 thomas@astro.psu.edu */
#define AT_INTERNAL			0x00800000 /* 		Only used for internal calculations */
#define AT_LIFE_STEALING	0x01000000 /* 		16777216 dnh@hawthorn.csse.monash.edu.au */
#define AT_SLASH			0x02000000  /* 		33554432 new physical attack types */
#define AT_CLEAVE			0x04000000  /* 		67108864 */
#define AT_PIERCE			0x08000000  /* 		134217728 */
#define AT_NETHER			0x10000000  /* 		special (undead) attack from lower spheres - pure damage */
#define AT_SONIC			0x20000000  /* 		pure energy attack - normally mana or grace converted in pure energy */
#define AT_DEMONIC			0x40000000  /* 		special attack for greater demons. */
#define AT_PSIONIC			0x80000000  /* 		well... */

#ifndef INIT_C

extern int resist_table[];
extern char *change_resist_msg[NROFATTACKS];
extern char *resist_plus[NROFATTACKS];
extern char *attacktype_desc[NROFATTACKS];
extern char *resist_save[NROFATTACKS];

extern int  protection_tab[NROFATTACKS];
extern char *protection_save[NROFPROTECTIONS];
extern char *protection_name[NROFPROTECTIONS];
#else

EXTERN char *protection_save[NROFPROTECTIONS] =
{
	"impact", 	"slash", 	"cleave", 		"pierce", 		"weapon_magic",
	"fire", 	"cold", 	"electricity", 	"poison", 		"acid",
	"magic", 	"mind", 	"body", 		"psionic", 		"force",
	"nether", 	"chaos", 	"death", 		"spiritual", 	"corruption"
};

EXTERN char *protection_name[NROFPROTECTIONS] =
{
	"impact", 	"slash", 	"cleave", 		"pierce", 		"weapon magic",
	"fire", 	"cold", 	"electricity", 	"poison", 		"acid",
	"magic", 	"mind", 	"body", 		"psionic", 		"force",
	"nether", 	"chaos", 	"death", 		"spiritual", 	"corruption"
};

/* Beware, names require an _ if there is a space, else they will be read
 * as for example: resist_life stealing 50! */
EXTERN char *resist_save[NROFATTACKS] =
{
	"impact", 		"magic", 		"fire", 		"electricity", 	"cold", 		"confusion",
	"acid",			"drain", 		"weaponmagic", 	"ghosthit", 	"poison", 		"slow",
	"paralyze",		"time", 		"fear", 		"cancellation", "depletion",	"death",
	"chaos",		"counterspell", "godpower", 	"purity", 		"blind" , 		"internal",
	"life_steal",	"slash", 		"cleave", 		"pierce", 		"nether", 		"sonic",
	"demonic", 		"psionic"
};

/* Short description of names of the attacktypes */
EXTERN char *attacktype_desc[NROFATTACKS] =
{
	"impact", 		"magic", 		"fire", 		"electricity", 	"cold", 		"confusion",
	"acid", 		"drain", 		"weapon magic", "ghost hit", 	"poison", 		"slow",
	"paralyze", 	"time", 		"fear", 		"cancellation", "depletion", 	"death",
	"chaos", 		"counterspell", "god power", 	"purity", 		"blind" , 		"internal",
	"life steal", 	"slash", 		"cleave", 		"pierce", 		"nether", 		"sonic",
	"demonic", 		"psionic"
};

/* This is the array that is what the player sees. */
EXTERN char *resist_plus[NROFATTACKS] =
{
	"resist impact", 		"resist magic", 	"resist fire", 		"resist electricity",
	"resist cold",			"resist confusion", "resist acid", 		"resist drain",
	"resist weaponmagic", 	"resist ghosthit", 	"resist poison", 	"resist slow",
	"resist paralyze", 		"resist time", 		"resist fear",		"resist cancellation",
	"resist depletion", 	"resist death", 	"resist chaos",		"resist counterspell",
	"resist god power", 	"resist purity",	"resist blind" ,  	"resist internal",
	"resist life steal",	"resist slash", 	"resist cleave", 	"resist pierce",
	"resist nether", 		"resist sonic", 	"resist demonic", 	"resist psionic"
};

/* These are the descriptions of the resistances displayed when a
 * player puts on/takes off an item. See change_abil() in living.c. */
EXTERN char *change_resist_msg[NROFATTACKS] =
{
	"impact", 		"magic", 		"fire", 		"electricity", 	"cold", 		"confusion",
	"acid",			"drain", 		"weapon magic", "ghosthit", 	"poison", 		"slow",
	"paralyze", 	"time", 		"fear", 		"cancellation", "depletion", 	"death",
	"chaos", 		"counterspell", "god power", 	"purity", 		"blind", 		"internal",
	"life steal", 	"slash", 		"cleave", 		"pierce", 		"nether", 		"sonic",
	"demonic", 		"psionic"
};


/* If you want to weight things so certain resistances show up more often than
 * others, just add more entries in the table for the protections you want to
 * show up. */
EXTERN int resist_table[] =
{
	ATNR_SLASH,			ATNR_CLEAVE,			ATNR_PIERCE,			ATNR_PHYSICAL,
	ATNR_MAGIC,			ATNR_FIRE ,				ATNR_ELECTRICITY,		ATNR_COLD,
	ATNR_CONFUSION,		ATNR_ACID,				ATNR_DRAIN,				ATNR_GHOSTHIT,
	ATNR_POISON,		ATNR_SLOW,				ATNR_PARALYZE,			ATNR_TIME,
	ATNR_FEAR,			ATNR_SLASH,				ATNR_DEPLETE,			ATNR_CLEAVE,
	ATNR_SONIC,			ATNR_PHYSICAL,			ATNR_BLIND,				ATNR_LIFE_STEALING,
	ATNR_PSIONIC,		ATNR_NETHER,			ATNR_PIERCE,			ATNR_SLASH,
	ATNR_CLEAVE,		ATNR_PIERCE,			ATNR_PHYSICAL,			ATNR_MAGIC,
	ATNR_FIRE ,			ATNR_ELECTRICITY,		ATNR_COLD,				ATNR_CONFUSION,
	ATNR_ACID,			ATNR_DRAIN,				ATNR_GHOSTHIT,			ATNR_POISON,
	ATNR_SLOW,			ATNR_PARALYZE,			ATNR_TIME,				ATNR_FEAR,
	ATNR_CANCELLATION,	ATNR_DEPLETE,			ATNR_COUNTERSPELL,		ATNR_SONIC,
	ATNR_HOLYWORD,		ATNR_BLIND,				ATNR_LIFE_STEALING,		ATNR_PSIONIC,
	ATNR_NETHER,		ATNR_DEMONIC,			ATNR_DEATH,
	ATNR_CHAOS,			ATNR_GODPOWER,			ATNR_WEAPONMAGIC
};

#endif

#define num_resist_table 58

/* attacktypes_load is suffixed to resist_ when saving objects.
 * (so the line may be 'resist_fire' 20 for example).  These are never
 * seen by the player.  loader.l uses the same names, but it doesn't look
 * like you can use these values, so in that function they are hard coded. */
#endif

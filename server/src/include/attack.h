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
 * Attack and defense system macros, defines, etc. */

#ifndef ATTACK_H
#define ATTACK_H

/**
 * Attacktypes:
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
} _protections;

/** Total number of attacks */
#define NROFATTACKS		32

/* Note that the last ATNR_ should be one less than NROFATTACKS above
 * since the ATNR starts counting at zero.
 * For compatible loading, these MUST correspond to the same value
 * as the bitmasks below. */

/**
 * @defgroup ATNR_xxx Attack type numbers
 * Attack type numbers.
 *@{*/

/** Impact. Kind of basic damage */
#define ATNR_PHYSICAL		0

/** Magical damage */
#define ATNR_MAGIC		    1

/** Fire attack */
#define ATNR_FIRE		    2

/** Electricity attack */
#define ATNR_ELECTRICITY	3

/** Cold (ice) attack */
#define ATNR_COLD		    4

/**
 * Confusion attack. Confuses the enemy, resulting in random direction
 * movement of the enemy. */
#define ATNR_CONFUSION		5

/** Acid attack. Can corrode armour and weapons. */
#define ATNR_ACID		    6

/** Drain attack. */
#define ATNR_DRAIN		    7

/** Weaponmagic attack */
#define ATNR_WEAPONMAGIC	8

/** Ghost hit attack */
#define ATNR_GHOSTHIT		9

/** Poison attack */
#define ATNR_POISON		    10

/** Slow attack. */
#define ATNR_SLOW		    11

/** Paralyze attack. The enemy is paralyzed and cannot move. */
#define ATNR_PARALYZE		12

/** Time attack */
#define ATNR_TIME			13

/** Fear attack */
#define ATNR_FEAR		    14

/** Cancellation attack. Can cancel magic bonuses on items. */
#define ATNR_CANCELLATION	15

/** Depletion attack */
#define ATNR_DEPLETE		16

/** Death attack */
#define ATNR_DEATH		    17

/** Chaos attack */
#define ATNR_CHAOS		    18

/** Counterspell attack */
#define ATNR_COUNTERSPELL	19

/** Godpower attack */
#define ATNR_GODPOWER		20

/** Holy word attack */
#define ATNR_HOLYWORD		21

/** Blind attack. Blinds the enemy. */
#define ATNR_BLIND		    22

/** Internal type of attack. Only used for internal calculations. */
#define ATNR_INTERNAL		23

/** Life stealing attack */
#define ATNR_LIFE_STEALING	24

/** Slash attack */
#define ATNR_SLASH      	25

/** Cleave attack */
#define ATNR_CLEAVE      	26

/** Pierce attack */
#define ATNR_PIERCE      	27

/** Nether attack */
#define ATNR_NETHER      	28

/** Sonic attack */
#define ATNR_SONIC      	29

/** Demonic attack */
#define ATNR_DEMONIC      	30

/** Psionic attack */
#define ATNR_PSIONIC      	31
/*@}*/

/**
 * @defgroup AT_xxx Attack types
 * Attack types. Used to set attacktype of an object.
 * @see ATNR_xxx
 *@{*/
/** 1 */
#define AT_PHYSICAL			0x00000001
/** 2 */
#define AT_MAGIC			0x00000002
/** 4 */
#define AT_FIRE				0x00000004
/** 8 */
#define AT_ELECTRICITY		0x00000008
/** 16 */
#define AT_COLD				0x00000010
/** 32 */
#define AT_CONFUSION		0x00000020
/** 64 */
#define AT_ACID				0x00000040
/** 128 */
#define AT_DRAIN			0x00000080
/** 256 */
#define AT_WEAPONMAGIC		0x00000100
/** 512 */
#define AT_GHOSTHIT			0x00000200
/** 1024 */
#define AT_POISON			0x00000400
/** 2048 */
#define AT_SLOW				0x00000800
/** 4096 */
#define AT_PARALYZE			0x00001000
/** 8192 */
#define AT_TIME				0x00002000
/** 16384 */
#define AT_FEAR				0x00004000
/** 32768 */
#define AT_CANCELLATION 	0x00008000
/** 65536 */
#define AT_DEPLETE      	0x00010000
/** 131072 */
#define AT_DEATH        	0x00020000
/** 262144 */
#define AT_CHAOS        	0x00040000
/** 524288 */
#define AT_COUNTERSPELL 	0x00080000
/** 1048576 */
#define AT_GODPOWER			0x00100000
/** 2097152 */
#define AT_HOLYWORD			0x00200000
/** 4194304 */
#define AT_BLIND			0x00400000
/** 8388608 */
#define AT_INTERNAL			0x00800000
/** 16777216 */
#define AT_LIFE_STEALING	0x01000000
/** 33554432 */
#define AT_SLASH			0x02000000
/** 67108864 */
#define AT_CLEAVE			0x04000000
/** 134217728 */
#define AT_PIERCE			0x08000000
/** 268435456 */
#define AT_NETHER			0x10000000
/** 536870912 */
#define AT_SONIC			0x20000000
/** 1073741824 */
#define AT_DEMONIC			0x40000000
/** 2147483648 */
#define AT_PSIONIC			0x80000000
/*@}*/

#ifndef INIT_C

extern int resist_table[];
extern char *change_resist_msg[NROFATTACKS];
extern char *resist_plus[NROFATTACKS];
extern char *attacktype_desc[NROFATTACKS];
extern char *resist_save[NROFATTACKS];

extern char *protection_save[NROFPROTECTIONS];
extern char *protection_name[NROFPROTECTIONS];

#else

/**
 * Names of protections to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead. */
EXTERN char *protection_save[NROFPROTECTIONS] =
{
	"impact", "slash", "cleave",      "pierce",    "weapon_magic",
	"fire",   "cold",  "electricity", "poison",    "acid",
	"magic",  "mind",  "body",        "psionic",   "force",
	"nether", "chaos", "death",       "spiritual", "corruption"
};

/** Actual names of the protections */
EXTERN char *protection_name[NROFPROTECTIONS] =
{
	"impact", "slash", "cleave",      "pierce",    "weapon magic",
	"fire",   "cold",  "electricity", "poison",    "acid",
	"magic",  "mind",  "body",        "psionic",   "force",
	"nether", "chaos", "death",       "spiritual", "corruption"
};

/**
 * Names of resists to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead. */
EXTERN char *resist_save[NROFATTACKS] =
{
	"impact",     "magic",        "fire",        "electricity",  "cold",      "confusion",
	"acid",       "drain",        "weaponmagic", "ghosthit",     "poison",    "slow",
	"paralyze",   "time",         "fear",        "cancellation", "depletion", "death",
	"chaos",      "counterspell", "godpower",    "purity",       "blind",     "internal",
	"life_steal", "slash",        "cleave",      "pierce",       "nether",    "sonic",
	"demonic",    "psionic"
};

/** Short description of names of the attacktypes */
EXTERN char *attacktype_desc[NROFATTACKS] =
{
	"impact",     "magic",        "fire",         "electricity",  "cold",      "confusion",
	"acid",       "drain",        "weapon magic", "ghost hit",    "poison",    "slow",
	"paralyze",   "time",         "fear",         "cancellation", "depletion", "death",
	"chaos",      "counterspell", "god power",    "purity",       "blind",     "internal",
	"life steal", "slash",        "cleave",       "pierce",       "nether",    "sonic",
	"demonic",    "psionic"
};

/** This is the array that is what the player sees. */
EXTERN char *resist_plus[NROFATTACKS] =
{
	"resist impact",      "resist magic",     "resist fire",    "resist electricity",
	"resist cold",        "resist confusion", "resist acid",    "resist drain",
	"resist weaponmagic", "resist ghosthit",  "resist poison",  "resist slow",
	"resist paralyze",    "resist time",      "resist fear",    "resist cancellation",
	"resist depletion",   "resist death",     "resist chaos",   "resist counterspell",
	"resist god power",   "resist purity",    "resist blind",   "resist internal",
	"resist life steal",  "resist slash",     "resist cleave",  "resist pierce",
	"resist nether",      "resist sonic",     "resist demonic", "resist psionic"
};

/**
 * These are the descriptions of the resistances displayed when a
 * player puts on/takes off an item.
 * @see change_abil() */
EXTERN char *change_resist_msg[NROFATTACKS] =
{
	"impact",     "magic",        "fire",         "electricity",  "cold",      "confusion",
	"acid",       "drain",        "weapon magic", "ghosthit",     "poison",    "slow",
	"paralyze",   "time",         "fear",         "cancellation", "depletion", "death",
	"chaos",      "counterspell", "god power",    "purity",       "blind",     "internal",
	"life steal", "slash",        "cleave",       "pierce",       "nether",    "sonic",
	"demonic",    "psionic"
};

/**
 * If you want to weight things so certain resistances show up more often than
 * others, just add more entries in the table for the protections you want to
 * show up. */
EXTERN int resist_table[] =
{
	ATNR_SLASH,        ATNR_CLEAVE,      ATNR_PIERCE,        ATNR_PHYSICAL,
	ATNR_MAGIC,        ATNR_FIRE,        ATNR_ELECTRICITY,   ATNR_COLD,
	ATNR_CONFUSION,    ATNR_ACID,        ATNR_DRAIN,         ATNR_GHOSTHIT,
	ATNR_POISON,       ATNR_SLOW,        ATNR_PARALYZE,      ATNR_TIME,
	ATNR_FEAR,         ATNR_SLASH,       ATNR_DEPLETE,       ATNR_CLEAVE,
	ATNR_SONIC,        ATNR_PHYSICAL,    ATNR_BLIND,         ATNR_LIFE_STEALING,
	ATNR_PSIONIC,      ATNR_NETHER,      ATNR_PIERCE,        ATNR_SLASH,
	ATNR_CLEAVE,       ATNR_PIERCE,      ATNR_PHYSICAL,      ATNR_MAGIC,
	ATNR_FIRE,         ATNR_ELECTRICITY, ATNR_COLD,          ATNR_CONFUSION,
	ATNR_ACID,         ATNR_DRAIN,       ATNR_GHOSTHIT,      ATNR_POISON,
	ATNR_SLOW,         ATNR_PARALYZE,    ATNR_TIME,          ATNR_FEAR,
	ATNR_CANCELLATION, ATNR_DEPLETE,     ATNR_COUNTERSPELL,  ATNR_SONIC,
	ATNR_HOLYWORD,     ATNR_BLIND,       ATNR_LIFE_STEALING, ATNR_PSIONIC,
	ATNR_NETHER,       ATNR_DEMONIC,     ATNR_DEATH,
	ATNR_CHAOS,        ATNR_GODPOWER,    ATNR_WEAPONMAGIC
};

#endif

/** Number of entries in the resist table. */
#define num_resist_table 58

#endif

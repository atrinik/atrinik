/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * The attack IDs. */
typedef enum _attacks
{
	ATNR_IMPACT,
	ATNR_SLASH,
	ATNR_CLEAVE,
	ATNR_PIERCE,
	ATNR_WEAPON_MAGIC,

	ATNR_FIRE,
	ATNR_COLD,
	ATNR_ELECTRICITY,
	ATNR_POISON,
	ATNR_ACID,

	ATNR_MAGIC,
	ATNR_MIND,
	ATNR_BLIND,
	ATNR_PARALYZE,
	ATNR_FORCE,

	ATNR_GODPOWER,
	ATNR_CHAOS,
	ATNR_DRAIN,
	ATNR_SLOW,
	ATNR_CONFUSION,

	ATNR_INTERNAL,

	NROFATTACKS
} _attacks;

/**
 * Last valid protection, used for treasure generation. */
#define LAST_PROTECTION (ATNR_CONFUSION + 1)

/**
 * @defgroup AT_xxx Attack types
 * Attack types. Used to set attacktype of an object.
 * @deprecated In the process of being phased out, do not use.
 *@{*/
/** 1 */
#define AT_PHYSICAL			0x00000001
/** 2 */
#define AT_MAGIC			0x00000002
/** 128 */
#define AT_DRAIN			0x00000080
/** 1024 */
#define AT_POISON			0x00000400
/** 8388608 */
#define AT_INTERNAL			0x00800000
/*@}*/

#ifndef INIT_C

extern char *attack_name[NROFATTACKS];
extern char *attack_save[NROFATTACKS];

#else

/**
 * Names of attack types to use when saving them to file.
 * @warning Cannot contain spaces. Use underscores instead. */
EXTERN char *attack_save[NROFATTACKS] =
{
	"impact",   "slash", "cleave",      "pierce",    "weaponmagic",
	"fire",     "cold",  "electricity", "poison",    "acid",
	"magic",    "mind",  "blind",       "paralyze",  "force",
	"godpower", "chaos", "drain",       "slow",      "confusion",
	"internal"
};

/** Short description of names of the attack types. */
EXTERN char *attack_name[NROFATTACKS] =
{
	"impact",   "slash", "cleave",      "pierce",    "weapon magic",
	"fire",     "cold",  "electricity", "poison",    "acid",
	"magic",    "mind",  "blind",       "paralyze",  "force",
	"godpower", "chaos", "drain",       "slow",      "confusion",
	"internal"
};

#endif

#endif

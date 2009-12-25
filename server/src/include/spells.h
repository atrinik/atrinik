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
 * Spells header. */

#ifndef SPELLS_H
#define SPELLS_H

extern int turn_bonus[];
extern int fear_bonus[];
extern int cleric_chance[];

/**
 * @defgroup spell_path_defines Spell paths
 * Spell path defines.
 *@{*/

#define PATH_NULL       0x00000000
#define PATH_PROT       0x00000001
#define PATH_FIRE       0x00000002
#define PATH_FROST      0x00000004
#define PATH_ELEC       0x00000008
#define PATH_MISSILE    0x00000010
#define PATH_SELF       0x00000020
#define PATH_SUMMON     0x00000040
#define PATH_ABJURE     0x00000080
#define PATH_RESTORE    0x00000100
#define PATH_DETONATE   0x00000200
#define PATH_MIND       0x00000400
#define PATH_CREATE     0x00000800
#define PATH_TELE       0x00001000
#define PATH_INFO       0x00002000
#define PATH_TRANSMUTE  0x00004000
#define PATH_TRANSFER   0x00008000
#define PATH_TURNING    0x00010000
#define PATH_WOUNDING   0x00020000
#define PATH_DEATH      0x00040000
#define PATH_LIGHT      0x00080000
/*@}*/

/**
 * @defgroup SPELL_USE_xxx Spell use flags
 * Spell use flags.
 *@{*/

/** Special spells - don't list them as available spell */
#define SPELL_USE_INTERN 0x00
/** Spell can be cast */
#define SPELL_USE_CAST   0x01
#define SPELL_USE_BALM   0x02
#define SPELL_USE_DUST   0x04
#define SPELL_USE_SCROLL 0x08
#define SPELL_USE_HORN   0x10
#define SPELL_USE_WAND   0x20
#define SPELL_USE_ROD    0x40
#define SPELL_USE_POTION 0x80
/**
 * Normally we don't use spellbooks as random stuff except some special
 * "quest" spells for quest monster. */
#define SPELL_USE_BOOK   0x100
/*@}*/

/**
 * @defgroup SPELL_TYPE_xxx Spell types
 * Spell types.
 *@{*/

/**
 * Special case: this is use like a spell but natural ability - effect
 * is non magical */
#define SPELL_TYPE_NATURAL 0
/** Base mage spell: using mana */
#define SPELL_TYPE_WIZARD  1
/** Base cleric spell: using grace */
#define SPELL_TYPE_PRIEST  2
/**
 * Number of different spell types, should be have value of the highest
 * spell type. */
#define SPELL_TYPE_NROF	   2
/*@}*/

/**
 * @defgroup SPELL_DESC_xxx Spell flags
 * Spell flags.
 *@{*/

/** Spell is safe to cast in as TOWN marked maps */
#define SPELL_DESC_TOWN         0x01
/** Spell is fired in a direction (bullet, bolt, ... ) */
#define SPELL_DESC_DIRECTION    0x02
/** Spell can be cast on self (with target system) */
#define SPELL_DESC_SELF         0x04
/** Spell can be cast on friendly creature (with target system) */
#define SPELL_DESC_FRIENDLY     0x08
/** Spell can be cast on enemy creature (with target system) */
#define SPELL_DESC_ENEMY        0x10
/** Spell can be cast on party members  */
#define SPELL_DESC_PARTY        0x20
/** Spell summons something */
#define SPELL_DESC_SUMMON       0x40
/**
 * Spell can be cast even when paralyzed
 * @todo Implement. */
#define SPELL_DESC_PARALYZED    0x80
/**
 * If set, this is a prayer using wisdom stat, instead of intelligence
 * stat of an object. */
#define SPELL_DESC_WIS          0x100
/*@}*/

/** Spell structure. */
typedef struct spell_struct
{
	/** Name of this spell */
	char name[BIG_NAME];

	/** Type of spell: wizard, priest, ... */
	int type;

	/** Level required to cast this spell */
	int level;

	/** Spellpoint cost to cast it */
	int sp;

	/** How many ticks it takes to cast the spell */
	int time;

	/** From 1 to this value we will generate for potions/scrolls... */
	int scrolls;

	/** If it can be used in wands, max # of charges */
	int charges;

	/** If target spell, this is max range to target */
	int range;

	/**
	 * Used when we have an item of this kind of spell.
	 * A magic potion has value x.
	 *
	 * We do: (x * value_mul) * level to increase the value.
	 * @see object::value */
	float value_mul;

	/** Base damage or hp of spell or summoned monster */
	int bdam;

	/** Base duration of spell or base range */
	int bdur;

	/** Levels you need over the min for the spell to gain one damage */
	int ldam;

	/** Levels you need over the min for the spell to gain one duration */
	int ldur;

	/**
	 * Number of levels beyond minimum for spell point cost to increase
	 * by amount equal to base cost.
	 *
	 * I.e. if base cost is 10 at level 2 and spl is 5, cost will
	 * increase by 2 per level.
	 *
	 * If base cost is 5 and spl is 10, cost increases by 1 every 2
	 * levels. */
	int spl;

	/** Number of sound ID for this sound */
	int sound;

	/** Define to what items this spell can be bound (potion, rod,,, ) */
	int spell_use;

	/** Used for SPELL_DESC_xx flags */
	uint32 flags;

	/** Path this spell belongs to */
	uint32 path;

	/** Pointer to archetype used by spell */
	char *archname;
} spell;

#define SP_NO_SPELL -1

/**
 * The spell numbers.
 *
 * When adding new spells, don't insert into the middle of the list -
 * add to the end of the list.
 *
 * Some archetypes and treasures require the spell numbers to be as they
 * are.
 *
 * @anchor spell_numbers */
enum spellnrs
{
	SP_FIRESTORM,
	SP_ICESTORM,
	SP_MINOR_HEAL,
	SP_CURE_POISON,
	SP_CURE_DISEASE,
	SP_STRENGTH,
	SP_IDENTIFY,
	SP_DETECT_MAGIC,
	SP_DETECT_CURSE,
	SP_REMOVE_CURSE,
	SP_REMOVE_DAMNATION,
	SP_CAUSE_LIGHT,
	SP_CONFUSION,
	SP_BULLET,
	SP_GOLEM,
	SP_REMOVE_DEPLETION,
	SP_PROBE,
	SP_TOWN_PORTAL,
	SP_CREATE_FOOD,
	SP_WOR,
	SP_CHARGING,
	SP_GREATER_HEAL,
	SP_RESTORATION,
	SP_PROT_COLD,
	SP_PROT_FIRE,
	SP_PROT_ELEC,
	SP_PROT_POISON,
	SP_CONSECRATE,
	SP_FINGER_DEATH,
	SP_CAUSE_COLD,
	SP_CAUSE_FLU,
	SP_CAUSE_LEPROSY,
	SP_CAUSE_SMALLPOX,
	SP_CAUSE_PNEUMONIC_PLAGUE,
	SP_METEOR,
	SP_METEOR_SWARM,
	SP_POISON_FOG,
	SP_BULLET_SWARM,
	SP_BULLET_STORM,
	SP_DESTRUCTION,
	SP_BOMB,
	SP_CURE_CONFUSION
};

extern spell spells[NROFREALSPELLS];

/** Multiplier for spell points / grace based on the attenuation. */
#define PATH_SP_MULT(op, spell) (((op->path_attuned & s->path) ? 0.8 : 1) * ((op->path_repelled & s->path) ? 1.25 : 1))

extern char *spellpathnames[NRSPELLPATHS];
extern archetype *spellarch[NROFREALSPELLS];

/** How is the spell being cast. */
typedef enum SpellTypeFrom
{
	spellNormal,
	spellWand,
	spellRod,
	spellHorn,
	spellScroll,
	spellPotion,
	spellNPC
} SpellTypeFrom;

#endif

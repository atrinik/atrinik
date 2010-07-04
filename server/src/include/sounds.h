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
 * Sounds header file */

#ifndef SOUNDS_H
#define SOUNDS_H

/**
 * @defgroup CMD_SOUND_xxx Sound command types
 * The sound command types.
 *@{*/
/** A sound effect, like poison, melee/range hit, spell sound, etc. */
#define CMD_SOUND_EFFECT 1
/** Background music. */
#define CMD_SOUND_BACKGROUND 2
/*@}*/

/**
 * Sound numbers.
 * @note The order here must match the order used in the client. */
typedef enum sound_ids_enum
{
	SOUND_LEVEL_UP,
	SOUND_FIRE_ARROW,
	SOUND_LEARN_SPELL,
	SOUND_FUMBLE_SPELL,
	SOUND_WAND_POOF,
	SOUND_OPEN_DOOR,
	SOUND_PUSH_PLAYER,

	SOUND_HIT_IMPACT,
	SOUND_HIT_CLEAVE,
	SOUND_HIT_SLASH,
	SOUND_HIT_PIERCE,
	SOUND_MISS_BLOCK,
	SOUND_MISS_HAND,
	SOUND_MISS_MOB,
	SOUND_MISS_PLAYER,

	SOUND_PET_IS_KILLED,
	SOUND_PLAYER_DIES,
	SOUND_OB_EVAPORATE,
	SOUND_OB_EXPLODE,
	SOUND_PLAYER_KILLS,
	SOUND_TURN_HANDLE,
	SOUND_FALL_HOLE,
	SOUND_DRINK_POISON,

	SOUND_DROP_THROW,
	SOUND_LOSE_SOME,
	SOUND_THROW,
	SOUND_GATE_OPEN,
	SOUND_GATE_CLOSE,
	SOUND_OPEN_CONTAINER,
	SOUND_GROWL,
	SOUND_ARROW_HIT,
	SOUND_DOOR_CLOSE,
	SOUND_TELEPORT,
	SOUND_CLICK,

	SOUND_MAGIC_DEFAULT,
	SOUND_MAGIC_ACID,
	SOUND_MAGIC_ANIMATE,
	SOUND_MAGIC_AVATAR,
	SOUND_MAGIC_BOMB,
	SOUND_MAGIC_BULLET1,
	SOUND_MAGIC_BULLET2,
	SOUND_MAGIC_CANCEL,
	SOUND_MAGIC_COMET,
	SOUND_MAGIC_CONFUSION,
	SOUND_MAGIC_CREATE,
	SOUND_MAGIC_DARK,
	SOUND_MAGIC_DEATH,
	SOUND_MAGIC_DESTRUCTION,
	SOUND_MAGIC_ELEC,
	SOUND_MAGIC_FEAR,
	SOUND_MAGIC_FIRE,
	SOUND_MAGIC_FIREBALL1,
	SOUND_MAGIC_FIREBALL2,
	SOUND_MAGIC_HWORD,
	SOUND_MAGIC_ICE,
	SOUND_MAGIC_INVISIBLE,
	SOUND_MAGIC_INVOKE,
	SOUND_MAGIC_INVOKE2,
	SOUND_MAGIC_MAGIC,
	SOUND_MAGIC_MANABALL,
	SOUND_MAGIC_MISSILE,
	SOUND_MAGIC_MMAP,
	SOUND_MAGIC_ORB,
	SOUND_MAGIC_PARALYZE,
	SOUND_MAGIC_POISON,
	SOUND_MAGIC_PROTECTION,
	SOUND_MAGIC_RSTRIKE,
	SOUND_MAGIC_RUNE,
	SOUND_MAGIC_SBALL,
	SOUND_MAGIC_SLOW,
	SOUND_MAGIC_SNOWSTORM,
	SOUND_MAGIC_STAT,
	SOUND_MAGIC_STEAMBOLT,
	SOUND_MAGIC_SUMMON1,
	SOUND_MAGIC_SUMMON2,
	SOUND_MAGIC_SUMMON3,
	SOUND_MAGIC_TELEPORT,
	SOUND_MAGIC_TURN,
	SOUND_MAGIC_WALL,
	SOUND_MAGIC_WALL2,
	SOUND_MAGIC_WOUND,

	SOUND_STEP1,
	SOUND_STEP2,
	SOUND_PRAY,
	SOUND_CONSOLE,
	SOUND_CLICKFAIL,
	SOUND_CHANGE1,
	SOUND_WARN_FOOD,
	SOUND_WARN_DRAIN,
	SOUND_WARN_STATUP,
	SOUND_WARN_STATDOWN,
	SOUND_WARN_HP,
	SOUND_WARN_HP2,
	SOUND_WEAPON_ATTACK,
	SOUND_WEAPON_HOLD,
	SOUND_GET,
	SOUND_BOOK,
	SOUND_PAGE,

	SPELL_SOUND_MAX
} sound_ids_enum;

#endif

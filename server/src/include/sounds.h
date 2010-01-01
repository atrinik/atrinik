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
 * @defgroup sound_types Sounds types
 * If we send a sound to the client, this defines what kind of sound we
 * have and how the client should use the sound number.
 *@{*/

/** Normal sound */
#define SOUND_NORMAL    0

/** Spell type sound */
#define SOUND_SPELL     1
/*@}*/

/**
 * @defgroup sound_numbers_normal Sound numbers for normal sounds
 * These are sounds for SOUND_NORMAL type sounds.
 * @note The order here must match the order used in the client.
 *@{*/
#define SOUND_LEVEL_UP			0
#define SOUND_FIRE_ARROW		1
#define SOUND_LEARN_SPELL		2
#define SOUND_FUMBLE_SPELL		3
#define SOUND_WAND_POOF			4
#define SOUND_OPEN_DOOR			5
#define SOUND_PUSH_PLAYER		6

#define SOUND_HIT_IMPACT    	7
#define SOUND_HIT_CLEAVE    	8
#define SOUND_HIT_SLASH     	9
#define SOUND_HIT_PIERCE    	10
#define SOUND_MISS_BLOCK    	11
#define SOUND_MISS_HAND     	12
#define SOUND_MISS_MOB      	13
#define SOUND_MISS_PLAYER   	14

#define SOUND_PET_IS_KILLED		15
#define SOUND_PLAYER_DIES		16
#define SOUND_OB_EVAPORATE		17
#define SOUND_OB_EXPLODE		18
#define SOUND_PLAYER_KILLS		19
#define SOUND_TURN_HANDLE		20
#define SOUND_FALL_HOLE			21
#define SOUND_DRINK_POISON  	22

#define SOUND_DROP_THROW    	23
#define SOUND_LOSE_SOME     	24
#define SOUND_THROW		    	25
#define SOUND_GATE_OPEN			26
#define SOUND_GATE_CLOSE		27
#define SOUND_OPEN_CONTAINER	28
#define SOUND_GROWL				29
#define SOUND_ARROW_HIT			30
#define SOUND_DOOR_CLOSE		31
#define SOUND_TELEPORT			32
#define SOUND_CLICK				33
/*@}*/

/**
 * @defgroup sound_numbers_spell Sound numbers for spell sounds
 * These are general spell sounds.
 *
 * Different spells can, of course, use the same sound.
 * @note The order here must match the order used in the client.
 *@{*/
typedef enum _spell_sound_id
{
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

	SPELL_SOUND_MAX
} _sound_id;
/*@}*/

#endif

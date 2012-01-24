/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Player related header file. */

#ifndef PLAYER_H
#define PLAYER_H

/**
 * @defgroup GENDER_xxx Gender IDs.
 * IDs of the various genders.
 *@{*/
/** Neuter: no gender. */
#define GENDER_NEUTER 0
/** Male. */
#define GENDER_MALE 1
/** Female. */
#define GENDER_FEMALE 2
/** Hermaphrodite: both genders. */
#define GENDER_HERMAPHRODITE 3
/** Total number of genders. */
#define GENDER_MAX 4
/*@}*/

/** IDs of the player doll items. */
typedef enum _player_doll_enum
{
	PDOLL_ARMOUR,
	PDOLL_HELM,
	PDOLL_GIRDLE,
	PDOLL_BOOT,
	PDOLL_RHAND,
	PDOLL_LHAND,
	PDOLL_RRING,
	PDOLL_LRING,
	PDOLL_BRACER,
	PDOLL_AMULET,
	PDOLL_SKILL_ITEM,
	PDOLL_WAND,
	PDOLL_BOW,
	PDOLL_GAUNTLET,
	PDOLL_ROBE,
	PDOLL_LIGHT,

	/* Must be last element */
	PDOLL_INIT
} _player_doll_enum;

/** Player doll item position structure. */
typedef struct _player_doll_pos
{
	/** X position. */
	int xpos;

	/** Y position. */
	int ypos;
} _player_doll_pos;

/**
 * Enumerations of the READY_OBJ_xxx constants used by
 * @ref BINARY_CMD_READY.
 * @anchor READY_OBJ_xxx */
enum
{
	READY_OBJ_ARROW,
	READY_OBJ_THROW,
	READY_OBJ_MAX
};

#endif

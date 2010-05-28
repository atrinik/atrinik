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
 * Party code header file. */

#ifndef PARTY_H
#define PARTY_H

/**
 * @defgroup PARTY_MESSAGE_xxx Party message types
 * Party message types.
 *@{*/
/**
 * Status is used for party messages like password change, join/leave,
 * etc. */
#define PARTY_MESSAGE_STATUS 1
/** Chat is used for party chat messages from party members. */
#define PARTY_MESSAGE_CHAT 2
/*@}*/

/**
 * Party structure. */
typedef struct party_struct
{
	/** Name of the party leader. */
	shstr *leader;

	/** Password this party requires. */
	char passwd[9];

	/** Name of the party. */
	shstr *name;

	/** Party members. */
	objectlink *members;

	/** Next party in the list. */
	struct party_struct *next;
} party_struct;

party_struct *first_party;

/**
 * @defgroup CMD_PARTY_xxx Party socket command types
 * Various types of the BINARY_CMD_PARTY socket command.
 *@{*/
/** Show a list of all parties in the game. */
#define CMD_PARTY_LIST 1
/** Show current members of your party. */
#define CMD_PARTY_WHO 2
/** Successfully joined a party. */
#define CMD_PARTY_JOIN 3
/** Joining a party requires a password. */
#define CMD_PARTY_PASSWORD 4
/** We're leaving a party. */
#define CMD_PARTY_LEAVE 5
/*@}*/

#endif

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
 * Party header file. */

#ifndef PARTY_H
#define PARTY_H

/**
 * @defgroup CMD_PARTY_xxx Party socket command types
 * Various types of the BINARY_CMD_PARTY socket command.
 *@{*/
/**
 * Show a list of all parties in the game. */
#define CMD_PARTY_LIST 1
/**
 * Show current members of your party. */
#define CMD_PARTY_WHO 2
/**
 * Successfully joined a party. */
#define CMD_PARTY_JOIN 3
/**
 * Joining a party requires a password. */
#define CMD_PARTY_PASSWORD 4
/**
 * We're leaving a party. */
#define CMD_PARTY_LEAVE 5
/**
 * Update party's who list. */
#define CMD_PARTY_UPDATE 6
/**
 * Remove memebr from party's who list. */
#define CMD_PARTY_REMOVE_MEMBER 7
/*@}*/

#endif

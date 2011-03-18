/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
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
 * Log levels. */

#ifndef LOGGER_H
#define LOGGER_H

/**
 * Log levels for the LOG() function. */
typedef enum LogLevel
{
	/**
	 * Set GLOBAL_LOG_LEVEL to this, and no messages will be printed
	 * out. */
	llevNoLog = -1,

	/**
	 * Used for system-type messages. */
	llevSystem = 0,

	/** An irrecoverable fatal error; the server will shut down. */
	llevError,

	/**
	 * A bug; after too many of these in a single tick the server will
	 * shut down. */
	llevBug,

	/** Just tell the log stuff we think it's useful to know. */
	llevInfo,

	/** Give out maximal information for debug and bug control. */
	llevDebug,

	/**
	 * Give out full monster information and debug messages. */
	llevMonster
} LogLevel;

/* If not set from outside, we force a useful setting here */
#ifndef GLOBAL_LOG_LEVEL
#	ifdef DEBUG
#		define GLOBAL_LOG_LEVEL llevMonster
#	else
#		define GLOBAL_LOG_LEVEL llevInfo
#	endif
#endif

#endif

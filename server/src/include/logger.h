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
 * Log levels. */

#ifndef LOGGER_H
#define LOGGER_H

/**
 * The log level system have this meaning:
 *
 * In GLOBAL_LOG_LEVEL is the default log level stored - at the bottom of
 * this file.
 *
 * This can changed at runtime by using a debug command.
 *
 * When DEBUG is set (or GLOBAL_LOG_LEVEL set to llevDebug) then the
 * system drops maximum log messages.
 *
 * If llevInfo is set, is still drops a lot useful messages.
 *
 * If llevBug is set, only really bugs and errors are logged.
 *
 * Set levNoLog for no output. */
typedef enum LogLevel
{
	/**
	 * Set GLOBAL_LOG_LEVEL to this, and no messages will be printed
	 * out. */
	llevNoLog = -1,

	/**
	 * Internal: used for llevError msg and llevBug message - don't set
	 * this! */
	llevSystem = 0,

	/** For fatal errors - server stability is unsafe after this */
	llevError,

	/** A bug - but we have it under control (we hope so) */
	llevBug,

	/** Just tell the log stuff we think it's useful to know */
	llevInfo,

	/** Give out maximal information for debug and bug control */
	llevDebug,

	/**
	 * SPECIAL DEBUG: give out full monster information and debug
	 * messages */
	llevMonster
} LogLevel;

/* if not set from outside, we force a useful setting here */
#ifndef GLOBAL_LOG_LEVEL
#ifdef DEBUG
#define GLOBAL_LOG_LEVEL llevMonster
#else
#define GLOBAL_LOG_LEVEL llevInfo
#endif
#endif

#endif

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
* the Free Software Foundation; either version 3 of the License, or     *
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

#ifndef LOGGER_H
#define LOGGER_H


/* The LOG system is more as a logger - it works also as a error & bug counter system.
 * Every llevError LOG will increase the error counter of the server - if to many errors
 * happens, the server will start a emergency shutdown. This will avoid bug loops or
 * every round LOGs, which will fill the log file fast.
 * llevError is always fatal - if this happens, ther server is not stabile anymore.
 * For the real bad parts, we go down directly - for some other we go on a bit - most
 * times to see what else is wrong.
 * llevBug is also a bug/error which should not happens BUT but we have catched the
 * problem and the server itself is not in danger. This can be for example wrong
 * settings of a object - we catch it and don't generate it for example. */

/* the log level system have this meaning:
 * in GLOBAL_LOG_LEVEL is the default log level stored - at the bottom of this header.
 * this can changed at runtime by using a debug cmd.
 * When DEBUG is set (or GLOBAL_LOG_LEVEL set to llevDebug) then
 * the system drops maximum log messages.
 * If llevInfo is set, is still drops alot useful messages.
 * If llevBug is set, only really bugs and errors are loged.
 * Set levNoLog for no output.
 * ingore llevSystem - its used for additional infos used by llevError and llevBug */

typedef enum LogLevel {
	/* set GLOBAL_LOG_LEVEL to this, and no message will be printed out */
  	llevNoLog = -1,

	/* internal: used for llevError msg and llevBug message - don't set this! */
  	llevSystem = 0,

	/* thats fatal errors - server stability is unsafe after this */
  	llevError,

	/* thats a bug - but we have it under control (we hope so) */
  	llevBug,

	/* just tell the log stuff we think its useful to know */
  	llevInfo,

	/* give out maximal information for debug and bug control */
  	llevDebug,

	/* SPECIAL DEBUG: give out full monster infos & debugs msg */
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

#endif /* LOGGER_H */

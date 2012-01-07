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
 * Logger API header file.
 *
 * @author Alex Tokar */

#ifndef LOGGER_H
#define LOGGER_H

typedef void (*logger_print_func)(const char *str);

#define LOG(_level) LOG_##_level, __FUNCTION__, __LINE__

#define LOG_INFO "INFO"
#define LOG_WARNING "WARNING"
#define LOG_DEBUG "DEBUG"
#define LOG_BUG "BUG"
#define LOG_ERROR "ERROR"
#define LOG_SYSTEM "SYSTEM"
#define LOG_CHAT "CHAT"

#ifdef WIN32
#	define LOGGER_ESC_SEQ_BOLD ""
#	define LOGGER_ESC_SEQ_BLACK ""
#	define LOGGER_ESC_SEQ_RED ""
#	define LOGGER_ESC_SEQ_GREEN ""
#	define LOGGER_ESC_SEQ_YELLOW ""
#	define LOGGER_ESC_SEQ_BLUE ""
#	define LOGGER_ESC_SEQ_MAGENTA ""
#	define LOGGER_ESC_SEQ_CYAN ""
#	define LOGGER_ESC_SEQ_WHITE ""
#	define LOGGER_ESC_SEQ_END ""
#else
#	define LOGGER_ESC_SEQ_BOLD "\033[1m"
#	define LOGGER_ESC_SEQ_BLACK "\033[30m"
#	define LOGGER_ESC_SEQ_RED "\033[31m"
#	define LOGGER_ESC_SEQ_GREEN "\033[32m"
#	define LOGGER_ESC_SEQ_YELLOW "\033[33m"
#	define LOGGER_ESC_SEQ_BLUE "\033[34m"
#	define LOGGER_ESC_SEQ_MAGENTA "\033[35m"
#	define LOGGER_ESC_SEQ_CYAN "\033[36m"
#	define LOGGER_ESC_SEQ_WHITE "\033[37m"
#	define LOGGER_ESC_SEQ_END "\033[0m"
#endif

#endif

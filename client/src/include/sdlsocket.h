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
 * Socket header file. */

#ifndef SDLSOCKET_H
#define SDLSOCKET_H

/**
 * Timeout when attempting a connection in milliseconds. */
#define SOCKET_TIMEOUT_MS 4000

#define SOCKET_NO -1

/**
 * One command buffer. */
typedef struct _command_buffer
{
	/** Next command in queue. */
	struct _command_buffer *next;

	/** Previous command in queue. */
	struct _command_buffer *prev;

	/** Length of the data. */
	int len;

	/** The data. */
	uint8 data[1];
} command_buffer;

#endif

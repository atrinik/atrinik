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
 * Packet API header file.
 *
 * @author Alex Tokar */

#ifndef PACKET_H
#define PACKET_H

/**
 * A single data packet. */
typedef struct packet_struct
{
	/**
	 * Next packet to send. */
	struct packet_struct *next;

	/**
	 * Previous packet. */
	struct packet_struct *prev;

	/**
	 * The data. */
	uint8 *data;

	/**
	 * Length of 'data'. */
	size_t len;

	/**
	 * Current size of 'data'. */
	size_t size;

	/**
	 * Expand size. */
	size_t expand;

	/**
	 * Position in 'data'. */
	size_t pos;

	/**
	 * Whether to enable NDELAY on this packet. */
	uint8 ndelay;

	/**
	 * The packet's command type. */
    uint8 type;
} packet_struct;

/**
 * How many packet structures to allocate when expanding the packets
 * memory pool. */
#define PACKET_EXPAND 10

#endif

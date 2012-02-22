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
 * Handles code related to @ref BOOK "books". */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	packet_struct *packet;

	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_OK;
	}

	if (QUERY_FLAG(applier, FLAG_BLIND))
	{
		draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
		return OBJECT_METHOD_OK;
	}

	if (op->msg == NULL)
	{
		draw_info_format(COLOR_WHITE, applier, "You open the %s and find it empty.", op->name);
		return OBJECT_METHOD_OK;
	}

	draw_info_format(COLOR_WHITE, applier, "You open the %s and start reading.", op->name);
	CONTR(applier)->stat_books_read++;

	packet = packet_new(CLIENT_CMD_BOOK, 512, 512);
	packet_append_string(packet, "<book>");
	packet_append_string(packet, query_base_name(op, applier));
	packet_append_string(packet, "</book>");
	packet_append_string_terminated(packet, op->msg);
	socket_send_packet(&CONTR(applier)->socket, packet);

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the book type object methods. */
void object_type_init_book(void)
{
	object_type_methods[BOOK].apply_func = apply_func;
}

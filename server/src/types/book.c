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
 * Handles code related to @ref BOOK "books". */

#include <global.h>

/**
 * Apply a book.
 *
 * Sends the book contents to the player object using
 * Send_With_Handling(), but only if the book does not have an APPLY
 * plugin trigger.
 *
 * A book can only be read if you're high enough level compared to the
 * book's level. You will also get experience for reading books, but only
 * if the book has not been read before.
 * @param op The player object applying the book.
 * @param tmp The book. */
void apply_book(object *op, object *tmp)
{
	int lev_diff;

	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op,FLAG_WIZ))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to read while blind.");
		return;
	}

	if (tmp->msg == NULL)
	{
		new_draw_info_format(NDI_UNIQUE, op, "You open the %s and find it empty.", tmp->name);
		return;
	}

	/* need a literacy skill to read stuff! */
	if (!change_skill(op, SK_LITERACY))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to decipher the strange symbols.");
		return;
	}

	lev_diff = tmp->level - (SK_level(op) + 5);

	if (!QUERY_FLAG(op, FLAG_WIZ) && lev_diff > 0)
	{
		if (lev_diff < 2)
		{
			new_draw_info(NDI_UNIQUE, op, "This book is just barely beyond your comprehension.");
		}
		else if (lev_diff < 3)
		{
			new_draw_info(NDI_UNIQUE, op, "This book is slightly beyond your comprehension.");
		}
		else if (lev_diff < 5)
		{
			new_draw_info(NDI_UNIQUE, op, "This book is beyond your comprehension.");
		}
		else if (lev_diff < 8)
		{
			new_draw_info(NDI_UNIQUE, op, "This book is quite a bit beyond your comprehension.");
		}
		else if (lev_diff < 15)
		{
			new_draw_info(NDI_UNIQUE, op, "This book is way beyond your comprehension.");
		}
		else
		{
			new_draw_info(NDI_UNIQUE, op, "This book is totally beyond your comprehension.");
		}

		return;
	}

	new_draw_info_format(NDI_UNIQUE, op, "You open the %s and start reading.", tmp->name);

	if (HAS_EVENT(tmp, EVENT_APPLY))
	{
		/* Trigger the APPLY event */
		trigger_event(EVENT_APPLY, op, tmp, NULL, NULL, 0, 0, 0, SCRIPT_FIX_ALL);
	}
	else
	{
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];

		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_BOOK);
		SockList_AddInt(&sl, tmp->weight_limit);
		strcpy((char *) sl.buf + sl.len, tmp->msg);
		sl.len += strlen(tmp->msg) + 1;
		Send_With_Handling(&CONTR(op)->socket, &sl);
	}

	/* gain xp from reading but only if not read before */
	if (!QUERY_FLAG(tmp, FLAG_NO_SKILL_IDENT))
	{
		sint64 exp_gain = calc_skill_exp(op, tmp, -1);

		if (!QUERY_FLAG(tmp, FLAG_IDENTIFIED))
		{
			/* Because they just identified it too */
			exp_gain *= 2;
			SET_FLAG(tmp, FLAG_IDENTIFIED);

			/* If in a container, update how it looks */
			if (tmp->env)
			{
				esrv_update_item(UPD_FLAGS | UPD_NAME, op, tmp);
			}
			else
			{
				CONTR(op)->socket.update_tile = 0;
			}
		}

		add_exp(op, exp_gain, op->chosen_skill->stats.sp, 0);

		/* so no more xp gained from this book */
		SET_FLAG(tmp, FLAG_NO_SKILL_IDENT);
	}
}

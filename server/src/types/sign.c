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
 * Handles code related to @ref SIGN "signs". */

#include <global.h>

/**
 * Apply a sign or trigger a magic mouth.
 *
 * Signs and magic mouths can have a "counter" value, which will make it
 * possible to read/trigger it only so many times.
 * @param op Object applying the sign
 * @param sign The sign or magic mouth object. */
void apply_sign(object *op, object *sign)
{
	if (sign->msg == NULL)
	{
		new_draw_info(NDI_UNIQUE, op, "Nothing is written on it.");
		return;
	}

	if (sign->stats.food)
	{
		if (sign->last_eat >= sign->stats.food)
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
			{
				new_draw_info(NDI_UNIQUE, op, "You cannot read it anymore.");
			}

			return;
		}

		sign->last_eat++;
	}

	/* Sign or magic mouth?  Do we need to see it, or does it talk to us?
	 * No way to know for sure.
	 *
	 * This check fails for signs with FLAG_WALK_ON/FLAG_FLY_ON.  Checking
	 * for FLAG_INVISIBLE instead of FLAG_WALK_ON/FLAG_FLY_ON would fail
	 * for magic mouths that have been made visible. */
	if (QUERY_FLAG(op, FLAG_BLIND) && !QUERY_FLAG(op, FLAG_WIZ) && !QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
	{
		new_draw_info(NDI_UNIQUE, op, "You are unable to read while blind.");
		return;
	}

	if (sign->slaying || sign->stats.hp || sign->race)
	{
		object *match = check_inv_recursive(op, sign);

		if ((match && sign->last_sp) || (!match && !sign->last_sp))
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
			{
				new_draw_info(NDI_UNIQUE, op, "You are unable to decipher the strange symbols.");
			}

			return;
		}
	}

	if (sign->direction && QUERY_FLAG(sign, FLAG_SYS_OBJECT))
	{
		if (op->direction != absdir(sign->direction + 4) && !(QUERY_FLAG(sign, FLAG_SPLITTING) && (op->direction == absdir(sign->direction - 5) || op->direction == absdir(sign->direction + 5))))
		{
			return;
		}
	}

	/* Use book GUI? */
	if (QUERY_FLAG(sign, FLAG_XRAYS))
	{
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];

		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_BOOK);
		SockList_AddInt(&sl, 0);
		strcpy((char *) sl.buf + sl.len, sign->msg);
		sl.len += strlen(sign->msg) + 1;
		Send_With_Handling(&CONTR(op)->socket, &sl);

		/* Ensure player is not running, mostly for walk/fly on signs. */
		CONTR(op)->run_on = CONTR(op)->fire_on = 0;
	}
	else
	{
		new_draw_info(NDI_UNIQUE | NDI_NAVY, op, sign->msg);
	}
}

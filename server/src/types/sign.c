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
	shstr *notification_msg;

	notification_msg = object_get_value(sign, "notification_message");

	if (!sign->msg && !sign->title && !notification_msg)
	{
		draw_info(COLOR_WHITE, op, "Nothing is written on it.");
		return;
	}

	if (sign->stats.food)
	{
		if (sign->last_eat >= sign->stats.food)
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
			{
				draw_info(COLOR_WHITE, op, "You cannot read it anymore.");
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
		draw_info(COLOR_WHITE, op, "You are unable to read while blind.");
		return;
	}

	if (sign->slaying || sign->stats.hp || sign->race)
	{
		object *match = check_inv_recursive(op, sign);

		if ((match && sign->last_sp) || (!match && !sign->last_sp))
		{
			if (!QUERY_FLAG(sign, FLAG_WALK_ON) && !QUERY_FLAG(sign, FLAG_FLY_ON))
			{
				draw_info(COLOR_WHITE, op, "You are unable to decipher the strange symbols.");
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

	if (sign->title)
	{
		play_sound_player_only(CONTR(op), CMD_SOUND_EFFECT, sign->title, 0, 0, 0, 0);
	}

	if (sign->msg)
	{
		/* Use book GUI? */
		if (QUERY_FLAG(sign, FLAG_XRAYS))
		{
			SockList sl;
			unsigned char sock_buf[MAXSOCKBUF];

			sl.buf = sock_buf;
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_BOOK);
			strcpy((char *) sl.buf + sl.len, sign->msg);
			sl.len += strlen(sign->msg) + 1;
			Send_With_Handling(&CONTR(op)->socket, &sl);

			/* Ensure player is not running, mostly for walk/fly on signs. */
			CONTR(op)->run_on = CONTR(op)->fire_on = 0;
		}
		else
		{
			draw_info(COLOR_NAVY, op, sign->msg);
		}
	}

	/* Add notification message, if any. */
	if (notification_msg)
	{
		shstr *notification_action, *notification_shortcut, *notification_delay;
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];

		notification_action = object_get_value(sign, "notification_action");
		notification_shortcut = object_get_value(sign, "notification_shortcut");
		notification_delay = object_get_value(sign, "notification_delay");

		sl.buf = sock_buf;
		SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_NOTIFICATION);

		SockList_AddChar(&sl, CMD_NOTIFICATION_TEXT);
		SockList_AddString(&sl, notification_msg);

		if (notification_action)
		{
			SockList_AddChar(&sl, CMD_NOTIFICATION_ACTION);
			SockList_AddString(&sl, notification_action);
		}

		if (notification_shortcut)
		{
			SockList_AddChar(&sl, CMD_NOTIFICATION_SHORTCUT);
			SockList_AddString(&sl, notification_shortcut);
		}

		if (notification_delay)
		{
			SockList_AddChar(&sl, CMD_NOTIFICATION_DELAY);
			SockList_AddInt(&sl, atoi(notification_delay));
		}

		Send_With_Handling(&CONTR(op)->socket, &sl);
	}
}

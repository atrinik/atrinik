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
 * Handles code related to @ref SIGN "signs".
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc object_methods::apply_func */
static int apply_func(object *op, object *applier, int aflags)
{
	shstr *notification_msg, *midi_data;

	(void) aflags;

	/* No point in non-players applying signs. */
	if (applier->type != PLAYER)
	{
		return OBJECT_METHOD_OK;
	}

	notification_msg = object_get_value(op, "notification_message");
	midi_data = object_get_value(op, "midi_data");

	if (!op->msg && !op->title && !notification_msg && !midi_data)
	{
		draw_info(COLOR_WHITE, applier, "Nothing is written on it.");
		return OBJECT_METHOD_OK;
	}

	if (op->stats.food)
	{
		if (op->last_eat >= op->stats.food)
		{
			if (!QUERY_FLAG(op, FLAG_SYS_OBJECT))
			{
				draw_info(COLOR_WHITE, applier, "You cannot read it anymore.");
			}

			return OBJECT_METHOD_OK;
		}

		op->last_eat++;
	}

	if (QUERY_FLAG(applier, FLAG_BLIND) && !QUERY_FLAG(applier, FLAG_WIZ) && !QUERY_FLAG(op, FLAG_SYS_OBJECT))
	{
		draw_info(COLOR_WHITE, applier, "You are unable to read while blind.");
		return OBJECT_METHOD_OK;
	}

	if (op->slaying || op->stats.hp || op->race)
	{
		object *match;

		match = check_inv(op, applier);

		if ((match && op->last_sp) || (!match && !op->last_sp))
		{
			if (!QUERY_FLAG(op, FLAG_SYS_OBJECT))
			{
				draw_info(COLOR_WHITE, applier, "You are unable to decipher the strange symbols.");
			}

			return OBJECT_METHOD_OK;
		}
	}

	if (op->direction && QUERY_FLAG(op, FLAG_SYS_OBJECT))
	{
		if (applier->direction != absdir(op->direction + 4) && !(QUERY_FLAG(op, FLAG_SPLITTING) && (applier->direction == absdir(op->direction - 5) || applier->direction == absdir(op->direction + 5))))
		{
			return OBJECT_METHOD_OK;
		}
	}

	if (op->title)
	{
		play_sound_player_only(CONTR(applier), CMD_SOUND_EFFECT, op->title, 0, 0, 0, 0);
	}

	if (midi_data)
	{
		play_sound_player_only(CONTR(applier), CMD_SOUND_MIDI_NOTE, midi_data, 0, 0, 0, 0);
	}

	if (op->msg)
	{
		draw_info(COLOR_NAVY, applier, op->msg);
	}

	/* Add notification message, if any. */
	if (notification_msg)
	{
		shstr *notification_action, *notification_shortcut, *notification_delay;
		SockList sl;
		unsigned char sock_buf[MAXSOCKBUF];

		notification_action = object_get_value(op, "notification_action");
		notification_shortcut = object_get_value(op, "notification_shortcut");
		notification_delay = object_get_value(op, "notification_delay");

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

		Send_With_Handling(&CONTR(applier)->socket, &sl);
	}

	return OBJECT_METHOD_OK;
}

/** @copydoc object_methods::trigger_func */
static int trigger_func(object *op, object *cause, int state)
{
	shstr *midi_data;

	(void) cause;
	(void) state;

	if (op->stats.food)
	{
		if (op->last_eat >= op->stats.food)
		{
			return OBJECT_METHOD_OK;
		}

		op->last_eat++;
	}

	midi_data = object_get_value(op, "midi_data");

	if (op->title)
	{
		play_sound_map(op->map, CMD_SOUND_EFFECT, op->title, op->x, op->y, 0, 0);
	}

	if (midi_data)
	{
		play_sound_map(op->map, CMD_SOUND_MIDI_NOTE, midi_data, op->x, op->y, 0, 0);
	}

	if (op->msg)
	{
		draw_info_map(0, COLOR_NAVY, op->map, op->x, op->y, MAP_INFO_NORMAL, NULL, NULL, op->msg);
	}

	return OBJECT_METHOD_OK;
}

/**
 * Initialize the sign type object methods. */
void object_type_init_sign(void)
{
	object_type_methods[SIGN].apply_func = apply_func;
	object_type_methods[SIGN].trigger_func = trigger_func;
}

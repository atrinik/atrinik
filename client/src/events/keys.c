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
 *  */

#include <global.h>

key_struct keys[SDLK_LAST];

/**
 * Initialize keys. */
void init_keys(void)
{
	memset(keys, 0, sizeof(*keys) * arraysize(keys));
}

/**
 * Handle a keyboard event.
 * @param event The event to handle. */
void key_handle_event(SDL_KeyboardEvent *event)
{
	/* Handle text input. */
	if (text_input_string_flag)
	{
		text_input_handle(event);
		return;
	}
	else if (text_input_string_end_flag)
	{
		return;
	}

	if (event->type == SDL_KEYDOWN)
	{
		if (GameStatus == GAME_STATUS_PLAY && event->keysym.sym == SDLK_ESCAPE)
		{
			settings_open();
			return;
		}
	}

	keybind_process_event(event);
}

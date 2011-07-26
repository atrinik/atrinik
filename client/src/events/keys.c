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

static int menuRepeatKey = -1;
static Uint32 menuRepeatTicks = 0, menuRepeatTime = KEY_REPEAT_TIME_INIT;
key_struct keys[SDLK_LAST];

/**
 * Screen definitions used when changing between resolutions in the
 * client. */
static const int screen_definitions[16][2] =
{
	{800, 600},
	{960, 600},
	{1024, 768},
	{1100, 700},
	{1280, 720},
	{1280, 800},
	{1280, 960},
	{1280, 1024},
	{1440, 900},
	{1400, 1050},
	{1600, 1200},
	{1680, 1050},
	{1920, 1080},
	{1920, 1200},
	{2048, 1536},
	{2560, 1600},
};

/**
 * Initialize keys and movement queue. */
void init_keys()
{
	size_t i;

	for (i = 0; i < arraysize(keys); i++)
	{
		keys[i].time = 0;
	}

	reset_keys();
}

void reset_keys()
{
	size_t i;

	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);

	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;

	for (i = 0; i < arraysize(keys); i++)
	{
		keys[i].pressed = 0;
	}
}

/* We have a key event */
void event_poll_key(SDL_KeyboardEvent *event)
{
	if (event->type == SDL_KEYUP)
	{
		/* End of key repeat. */
		menuRepeatKey = -1;
		menuRepeatTime = KEY_REPEAT_TIME_INIT;
		keys[event->keysym.sym].pressed = 0;
	}
	else if (event->type == SDL_KEYDOWN)
	{
		keys[event->keysym.sym].pressed = 1;
		keys[event->keysym.sym].time = SDL_GetTicks() + KEY_REPEAT_TIME_INIT;
	}

	/* Handle lists. */
	if (lists_handle_keyboard(event))
	{
		return;
	}

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
		if (cpl.menustatus != MENU_NO)
		{
			check_menu_keys(cpl.menustatus, event->keysym.sym);
			return;
		}

		if (GameStatus == GAME_STATUS_PLAY && event->keysym.sym == SDLK_ESCAPE)
		{
			settings_open();
			return;
		}
	}

	keybind_process_event(event);
}

void cursor_keys(int num)
{
	const int below_inv_adjust[4] = {-INVITEMBELOWXLEN, INVITEMBELOWXLEN, -1, 1};
	const int inv_adjust[4] = {-INVITEMXLEN, INVITEMXLEN, -1, 1};

	if (num < 0 || num >= 4)
	{
		return;
	}

	if (cpl.inventory_win == IWIN_BELOW)
	{
		cpl.win_below_slot += below_inv_adjust[num];
		cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
	}
	else
	{
		cpl.win_inv_slot += inv_adjust[num];
		cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
	}
}

/* Handle key repeating. */
void key_repeat()
{
	if (cpl.menustatus == MENU_NO)
	{
		keybind_repeat();
	}
	/* check menu keys for repeat */
	else
	{
		if (SDL_GetTicks() - menuRepeatTicks > menuRepeatTime || !menuRepeatTicks || menuRepeatKey < 0)
		{
			menuRepeatTicks = SDL_GetTicks();

			if (menuRepeatKey >= 0)
			{
				check_menu_keys(cpl.menustatus, menuRepeatKey);
				menuRepeatTime = KEY_REPEAT_TIME;
			}
		}
	}
}

/* Handle keystrokes in menu dialog. */
void check_menu_keys(int menu, int key)
{
	if (cpl.menustatus == MENU_NO)
		return;

	/* close menu */
	if (key == SDLK_ESCAPE)
	{
		cpl.menustatus = MENU_NO;
		map_udate_flag = 2;
		reset_keys();
		return;
	}

	switch (menu)
	{
		case MENU_BOOK:
			book_handle_key(key);
			menuRepeatKey = key;
			break;

		case MENU_REGION_MAP:
			region_map_handle_key(key);
			menuRepeatKey = key;
			break;
	}
}

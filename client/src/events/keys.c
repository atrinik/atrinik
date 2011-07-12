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
_keys keys[MAX_KEYS];

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
	int i;

	for (i = 0; i < MAX_KEYS; i++)
	{
		keys[i].time = 0;
	}

	reset_keys();
}

void reset_keys()
{
	int i;

	SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);

	text_input_string_flag = 0;
	text_input_string_end_flag = 0;
	text_input_string_esc_flag = 0;

	for (i = 0; i < MAX_KEYS; i++)
	{
		keys[i].pressed = 0;
	}
}

int key_event(SDL_KeyboardEvent *key)
{
	if (key->type == SDL_KEYUP)
	{
		keys[key->keysym.sym].pressed = 0;

		if (cpl.menustatus == MENU_NO)
		{
			keybind_process_event(key);
		}
	}
	else if (key->type == SDL_KEYDOWN)
	{
		if (cpl.menustatus != MENU_NO)
		{
			keys[key->keysym.sym].pressed = 1;
			keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;
			check_menu_keys(cpl.menustatus, key->keysym.sym);
		}
		/* no menu */
		else
		{
			keys[key->keysym.sym].pressed = 1;
			keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;

			if (keybind_command_matches_event("?COPY", key))
			{
				textwin_handle_copy();
			}
			else if (key->keysym.sym == SDLK_ESCAPE)
			{
				settings_open();
			}

			keybind_process_event(key);
		}
	}

	return 0;
}

/* We have a key event */
int event_poll_key(SDL_Event *event)
{
	if (event->type == SDL_KEYUP)
	{
		/* End of key repeat. */
		menuRepeatKey = -1;
		menuRepeatTime = KEY_REPEAT_TIME_INIT;
	}

	if (lists_handle_keyboard(&event->key))
	{
		return 0;
	}

	if (text_input_string_flag)
	{
		if (cpl.input_mode != INPUT_MODE_NUMBER)
			cpl.inventory_win = IWIN_BELOW;

		text_input_handle(&event->key);
	}
	else if (!text_input_string_end_flag)
	{
		if (GameStatus == GAME_STATUS_PLAY)
			return key_event(&event->key);
	}

	return 0;
}

void cursor_keys(int num)
{
	switch (num)
	{
		case 0:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				if (cpl.win_below_slot - INVITEMBELOWXLEN >= 0)
					cpl.win_below_slot -= INVITEMBELOWXLEN;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				if (cpl.win_inv_slot - INVITEMXLEN >= 0)
					cpl.win_inv_slot -= INVITEMXLEN;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 1:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				if (cpl.win_below_slot + INVITEMXLEN < cpl.win_below_count)
					cpl.win_below_slot += INVITEMXLEN;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				if (cpl.win_inv_slot + INVITEMXLEN < cpl.win_inv_count)
					cpl.win_inv_slot += INVITEMXLEN;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 2:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				cpl.win_below_slot--;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				cpl.win_inv_slot--;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;

		case 3:
			if (cpl.inventory_win == IWIN_BELOW)
			{
				cpl.win_below_slot++;

				cpl.win_below_tag = get_inventory_data(cpl.below, &cpl.win_below_ctag, &cpl.win_below_slot, &cpl.win_below_start, &cpl.win_below_count, INVITEMBELOWXLEN, INVITEMBELOWYLEN);
			}
			else
			{
				cpl.win_inv_slot++;

				cpl.win_inv_tag = get_inventory_data(cpl.ob, &cpl.win_inv_ctag, &cpl.win_inv_slot, &cpl.win_inv_start, &cpl.win_inv_count, INVITEMXLEN, INVITEMYLEN);
			}
			break;
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

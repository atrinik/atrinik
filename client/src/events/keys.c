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

#define KEY_REPEAT_TIME 35
#define KEY_REPEAT_TIME_INIT 175

static int menuRepeatKey = -1;
static Uint32 menuRepeatTicks = 0, menuRepeatTime = KEY_REPEAT_TIME_INIT;

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

int key_event(SDL_KeyboardEvent *key)
{
	if (key->type == SDL_KEYUP)
	{
		if (cpl.menustatus != MENU_NO)
		{
			keys[key->keysym.sym].pressed = 0;
		}
		else
		{
			keys[key->keysym.sym].pressed = 0;

			switch (key->keysym.sym)
			{
				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
					cpl.inventory_win = IWIN_BELOW;
					break;

				case SDLK_LALT:
				case SDLK_RALT:
					send_command("/run_stop");
					cpl.run_on = 0;
					break;

				case SDLK_RCTRL:
				case SDLK_LCTRL:
					cpl.fire_on = 0;
					break;

				default:
					break;
			}
		}
	}
	else if (key->type == SDL_KEYDOWN)
	{
		if (cpl.menustatus != MENU_NO)
		{
			keys[key->keysym.sym].pressed = 1;
			keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;

			if (keybind_status == KEYBIND_STATUS_NO)
			{
				check_menu_keys(cpl.menustatus, key->keysym.sym);
			}
		}
		/* no menu */
		else
		{
			keys[key->keysym.sym].pressed = 1;
			keys[key->keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;
			check_keys(key->keysym.sym);

			switch ((int)key->keysym.sym)
			{
				case SDLK_F1:
					quickslots_handle_key(key, 0);
					break;

				case SDLK_F2:
					quickslots_handle_key(key, 1);
					break;

				case SDLK_F3:
					quickslots_handle_key(key, 2);
					break;

				case SDLK_F4:
					quickslots_handle_key(key, 3);
					break;

				case SDLK_F5:
					quickslots_handle_key(key, 4);
					break;

				case SDLK_F6:
					quickslots_handle_key(key, 5);
					break;

				case SDLK_F7:
					quickslots_handle_key(key, 6);
					break;

				case SDLK_F8:
					quickslots_handle_key(key, 7);
					break;

				case SDLK_END:
					quickslot_group++;

					if (quickslot_group > MAX_QUICKSLOT_GROUPS)
						quickslot_group = MAX_QUICKSLOT_GROUPS;

					break;

				case SDLK_HOME:
					quickslot_group--;

					if (quickslot_group < 1)
						quickslot_group = 1;

					break;

				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
					SetPriorityWidget(cur_widget[MAIN_INV_ID]);

					if (!setting_get_int(OPT_CAT_GENERAL, OPT_PLAYERDOLL))
					{
						SetPriorityWidget(cur_widget[PDOLL_ID]);
					}

					cpl.inventory_win = IWIN_INV;
					break;

				case SDLK_RALT:
				case SDLK_LALT:
					cpl.run_on = 1;
					break;

				case SDLK_RCTRL:
				case SDLK_LCTRL:
					cpl.fire_on = 1;
					break;

				case SDLK_ESCAPE:
					settings_open();
					break;
			}
		}
	}

	return 0;
}

/* We have a key event */
int event_poll_key(SDL_Event *event)
{
	if (event->key.type == SDL_KEYUP)
	{
		/* End of key repeat. */
		menuRepeatKey = -1;
		menuRepeatTime = KEY_REPEAT_TIME_INIT;
	}

	if (cpl.menustatus == MENU_NO && !text_input_string_flag)
	{
		if (event->key.keysym.mod & KMOD_SHIFT)
		{
			cpl.inventory_win = IWIN_INV;

			if (GameStatus == GAME_STATUS_PLAY && event->key.type != SDL_KEYUP)
			{
				if (event->key.keysym.sym == SDLK_1)
				{
					inventory_filter_set(INVENTORY_FILTER_ALL);
					return 0;
				}
				else if (event->key.keysym.sym == SDLK_2)
				{
					inventory_filter_set(INVENTORY_FILTER_APPLIED);
					return 0;
				}
				else if (event->key.keysym.sym == SDLK_3)
				{
					inventory_filter_set(INVENTORY_FILTER_LOCKED);
					return 0;
				}
				else if (event->key.keysym.sym == SDLK_4)
				{
					inventory_filter_set(INVENTORY_FILTER_UNIDENTIFIED);
					return 0;
				}
			}
		}
		else
		{
			cpl.inventory_win = IWIN_BELOW;
		}

		if (event->key.keysym.mod & KMOD_RCTRL || event->key.keysym.mod & KMOD_LCTRL || event->key.keysym.mod & KMOD_CTRL)
		{
			if (event->key.keysym.sym == SDLK_c && event->key.type != SDL_KEYUP)
			{
				textwin_handle_copy();
				return 0;
			}

			cpl.fire_on = 1;
		}
		else
		{
			cpl.fire_on = 0;
		}
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
	int i, j;

	if (cpl.menustatus == MENU_NO)
	{
		/* Groups */
		for (j = 0; j < BINDKEY_LIST_MAX; j++)
		{
			for (i = 0; i < OPTWIN_MAX_OPT; i++)
			{
				/* Key for this keymap is pressed */
				if (keys[bindkey_list[j].entry[i].key].pressed && bindkey_list[j].entry[i].repeatflag)
				{
					/* If time to repeat */
					if (keys[bindkey_list[j].entry[i].key].time + KEY_REPEAT_TIME - 5 < LastTick)
					{
						/* Repeat x times*/
						while ((keys[bindkey_list[j].entry[i].key].time += KEY_REPEAT_TIME - 5) < LastTick)
						{
							process_macro(bindkey_list[j].entry[i]);
						}
					}
				}
			}
		}
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
		if (cpl.menustatus == MENU_KEYBIND)
			save_keybind_file(KEYBIND_FILE);

		cpl.menustatus = MENU_NO;
		map_udate_flag = 2;
		reset_keys();
		return;
	}

	if (check_keys_menu_status(key))
		return;

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

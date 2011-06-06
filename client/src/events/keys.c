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

#include <include.h>

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
#if 0
					draw_info("run_stop", COLOR_DGOLD);
#endif
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
			/* We catch here the keybind key, when we insert a new macro there */
			if (cpl.menustatus == MENU_KEYBIND)
			{
				if (keybind_status == KEYBIND_STATUS_EDITKEY)
				{
					keybind_status = KEYBIND_STATUS_NO;

					if (key->keysym.sym != SDLK_ESCAPE)
					{
						int i, j, already_bound = 0;

						for (i = 0; i < BINDKEY_LIST_MAX; i++)
						{
							for (j = 0; j < OPTWIN_MAX_OPT; j++)
							{
								if (i == bindkey_list_set.group_nr && j == bindkey_list_set.entry_nr)
								{
									continue;
								}

								if (bindkey_list[i].entry[j].key == (int) key->keysym.sym)
								{
									already_bound = 1;
									draw_info_format(COLOR_RED, "The key %s is already bound!", bindkey_list[i].entry[j].keyname);
									break;
								}
							}
						}

						/* If the key is already bound, just continue trying to get a different key. */
						if (already_bound)
						{
							keybind_status = KEYBIND_STATUS_EDITKEY;
						}
						else
						{
							sound_play_effect("scroll.ogg", 100);
							strcpy(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname, SDL_GetKeyName(key->keysym.sym));
							bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key = key->keysym.sym;
						}
					}

					return 0;
				}
			}

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
					if (!options.playerdoll)
						SetPriorityWidget(cur_widget[PDOLL_ID]);
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
			cpl.fire_on = 1;
		else
			cpl.fire_on = 0;
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
	int shiftPressed = SDL_GetModState() & KMOD_SHIFT;

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

		case MENU_OPTION:
			switch (key)
			{
				case SDLK_LEFT:
					option_list_set.key_change =-1;
					menuRepeatKey = SDLK_LEFT;
					break;

				case SDLK_RIGHT:
					option_list_set.key_change = 1;
					menuRepeatKey = SDLK_RIGHT;
					break;

				case SDLK_UP:
					if (!shiftPressed)
					{
						if (option_list_set.entry_nr > 0)
							option_list_set.entry_nr--;
						else
							sound_play_effect("click_fail.ogg", MENU_SOUND_VOL);
					}
					else
					{
						if (option_list_set.group_nr > 0)
						{
							option_list_set.group_nr--;
							option_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_UP;
					break;

				case SDLK_DOWN:
					if (!shiftPressed)
					{
						option_list_set.entry_nr++;
					}
					else
					{
						if (opt_tab[option_list_set.group_nr + 1])
						{
							option_list_set.group_nr++;
							option_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_DOWN;
					break;

				case SDLK_d:
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					map_udate_flag = 2;

					if (cpl.menustatus == MENU_KEYBIND)
						save_keybind_file(KEYBIND_FILE);

					if (cpl.menustatus == MENU_OPTION)
					{
						save_options_dat();

						if (options.playerdoll)
						{
							cur_widget[PDOLL_ID]->show = 1;
						}

						change_textwin_font(options.chat_font_size);
						sound_update_volume();

						if (options.resolution && (screen_definitions[options.resolution - 1][0] != Screensize->x || screen_definitions[options.resolution - 1][1] != Screensize->y))
						{
							resize_window(screen_definitions[options.resolution - 1][0], screen_definitions[options.resolution - 1][1]);
							video_set_size();
						}
					}

					cpl.menustatus = MENU_NO;
					reset_keys();
					break;
			}
			break;

			/* The party GUI menu */
		case MENU_PARTY:
			if (!gui_interface_party)
				return;

			switch (key)
			{
					/* Arrow up */
				case SDLK_UP:
					/* If shift is pressed too */
					if (shiftPressed)
					{
						/* If the current tab is not 0, go to the above tab and switch tab. */
						if (gui_interface_party->tab > 0)
						{
							gui_interface_party->tab--;
							gui_interface_party->selected = 0;
							switch_tabs();
						}
					}
					/* Otherwise, we're scrolling in the GUI, and adjusting the selected row too */
					else
					{
						gui_interface_party->selected--;

						if (gui_interface_party->yoff > gui_interface_party->selected)
							gui_interface_party->yoff--;
					}

					menuRepeatKey = SDLK_UP;
					break;

					/* Arrow down */
				case SDLK_DOWN:
					/* If shift is pressed too */
					if (shiftPressed)
					{
						/* If the next time won't be over maximum, go to the below tab and switch tab. */
						if (gui_interface_party->tab + 1 < (cpl.partyname[0] == '\0' ? PARTY_TAB_LIST + 1 : PARTY_TABS))
						{
							gui_interface_party->tab++;
							gui_interface_party->selected = 0;
							switch_tabs();
						}
					}
					/* Otherwise, we're scrolling in the GUI, and adjusting the selected row too */
					else
					{
						gui_interface_party->selected++;

						if (gui_interface_party->selected >= DIALOG_LIST_ENTRY + gui_interface_party->yoff)
							gui_interface_party->yoff++;
					}

					menuRepeatKey = SDLK_DOWN;
					break;

					/* Enter or return key */
				case SDLK_KP_ENTER:
				case SDLK_RETURN:
				{
					/* Was pressed when we were in the parties list */
					if (strcmp(gui_interface_party->command, "list") == 0)
					{
						_gui_party_line *party_line = gui_interface_party->start;
						int i = 0;
						char partyname[HUGE_BUF], buf[HUGE_BUF];

						partyname[0] = '\0';

						/* Go through the lines, looking for our selected party */
						while (party_line)
						{
							/* Got it! */
							if (i == gui_interface_party->selected)
							{
								sscanf(party_line->line, "Name: %32[^\t]", partyname);
								break;
							}

							i++;
							party_line = party_line->next;
						}

						/* If we found it... */
						if (partyname[0] != '\0')
						{
							/* ... and it's not party we're member of, send command to server and close the GUI. */
							if (strcmp(partyname, cpl.partyname))
							{
								snprintf(buf, sizeof(buf), "/party join %s", partyname);
								send_command(buf);

								map_udate_flag = 2;
								cpl.menustatus = MENU_NO;
								reset_keys();
							}
						}
					}
				}

				/* y key */
				case SDLK_y:
					/* Coming from the GUI that asks you if you're sure you want to leave... */
					if (strcmp(gui_interface_party->command, "askleave") == 0)
					{
						/* Check the command */
						if (!client_command_check("/party leave"))
							send_command("/party leave");

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}
					/* Otherwise it's GUI that asks us if we really want to change the password. :-) */
					else if (strcmp(gui_interface_party->command, "askpassword") == 0)
					{
						/* Load the party interface - mostly we need it to set the command. */
						gui_interface_party = load_party_interface("passwordset", -1);
						cpl.input_mode = INPUT_MODE_CONSOLE;

						/* Open the console, with a maximum of 8 chars. */
						text_input_open(8);

						/* Output some info as to why the console magically opened. */
						draw_info("Type the new party password, or press ESC to cancel.", 10);

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}

					break;

					/* n key*/
				case SDLK_n:
					/* If used from the GUI to change party password, or leave party, just close the menu */
					if (strcmp(gui_interface_party->command, "askleave") == 0 || strcmp(gui_interface_party->command, "askpassword") == 0)
						cpl.menustatus = MENU_NO;

					break;

					/* f key */
				case SDLK_f:
					/* From the list... */
					if (strcmp(gui_interface_party->command, "list") == 0)
					{
						/* Load the party interface */
						gui_interface_party = load_party_interface("form", -1);

						cpl.input_mode = INPUT_MODE_CONSOLE;

						/* Open the console with a 60 chars limit */
						text_input_open(60);

						/* Output some info as to why the console magically opened */
						draw_info("Type the party name to form, or press ESC to cancel.", 10);

						/* Close the menu */
						cpl.menustatus = MENU_NO;
					}

					break;
			}

			/* Some sanity checks to make sure yoff doesn't go off-limits. */
			if (gui_interface_party->yoff < 0 || gui_interface_party->lines < DIALOG_LIST_ENTRY)
				gui_interface_party->yoff = 0;
			else if (gui_interface_party->yoff >= gui_interface_party->lines - DIALOG_LIST_ENTRY)
				gui_interface_party->yoff = gui_interface_party->lines - DIALOG_LIST_ENTRY;

			/* Checks for the selected row too. */
			if (gui_interface_party->selected < 0 || gui_interface_party->lines == 0)
				gui_interface_party->selected = 0;
			else if (gui_interface_party->selected >= gui_interface_party->lines)
				gui_interface_party->selected = gui_interface_party->lines - 1;

			break;

		case MENU_KEYBIND:
			switch (key)
			{
				case SDLK_UP:
					if (!shiftPressed)
					{
						if (bindkey_list_set.entry_nr > 0)
							bindkey_list_set.entry_nr--;
					}
					else
					{
						if (bindkey_list_set.group_nr > 0)
						{
							bindkey_list_set.group_nr--;
							bindkey_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_UP;
					break;

				case SDLK_DOWN:
					if (!shiftPressed)
					{
						if (bindkey_list_set.entry_nr < OPTWIN_MAX_OPT - 1)
							bindkey_list_set.entry_nr++;
					}
					else
					{
						if (bindkey_list_set.group_nr < BINDKEY_LIST_MAX - 1 && bindkey_list[bindkey_list_set.group_nr+1].name[0])
						{
							bindkey_list_set.group_nr++;
							bindkey_list_set.entry_nr = 0;
						}
					}

					menuRepeatKey = SDLK_DOWN;
					break;

				case SDLK_d:
					save_keybind_file(KEYBIND_FILE);
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					map_udate_flag = 2;
					cpl.menustatus = MENU_NO;
					reset_keys();
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					break;

				case SDLK_RETURN:
				case SDLK_KP_ENTER:
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					keybind_status = KEYBIND_STATUS_EDIT;
					reset_keys();
					text_input_open(240);
					text_input_add_string(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text);
					cpl.input_mode = INPUT_MODE_GETKEY;
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					break;

				case SDLK_r:
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].repeatflag = bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].repeatflag ? 0 : 1;
					sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
					break;
			}

			break;
	}
}

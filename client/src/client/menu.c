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
 * Menu related functions. */

#include <include.h>

/** Keybind menu */
int keybind_status;

/**
 * Analyze /cmd type commands the player has typed in the console or bound to a key.
 * Sort out the "client intern" commands and expand or pre process them for the server.
 * @param cmd Command to check
 * @return 0 to send command to server, 1 to not send it */
int client_command_check(char *cmd)
{
	if (!strncasecmp(cmd, "/ready_spell", 12))
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == 0)
		{
			draw_info("Usage: /ready_spell <spell name>", COLOR_GREEN);
		}
		else
		{
			int i, ii;

			for (i = 0; i < SPELL_LIST_MAX; i++)
			{
				for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
				{
					if (spell_list[i].entry[0][ii].flag >= LIST_ENTRY_USED)
					{
						if (!strcmp(spell_list[i].entry[0][ii].name, cmd))
						{
							if (spell_list[i].entry[0][ii].flag == LIST_ENTRY_KNOWN)
							{
								fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[i].entry[0][ii];
								RangeFireMode = FIRE_MODE_SPELL;
								sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
								draw_info("Spell ready.", COLOR_GREEN);
								return 1;
							}
						}
					}

					if (spell_list[i].entry[1][ii].flag >= LIST_ENTRY_USED)
					{
						if (!strcmp(spell_list[i].entry[1][ii].name, cmd))
						{
							if (spell_list[i].entry[1][ii].flag==LIST_ENTRY_KNOWN)
							{
								fire_mode_tab[FIRE_MODE_SPELL].spell = &spell_list[i].entry[1][ii];
								RangeFireMode = FIRE_MODE_SPELL;
								sound_play_effect("scroll.ogg", MENU_SOUND_VOL);
								draw_info("Spell ready.", COLOR_GREEN);
								return 1;
							}
						}
					}
				}
			}
		}

		draw_info("Unknown spell.", COLOR_GREEN);
		return 1;
	}
	else if (!strncasecmp(cmd, "/pray", 5))
	{
		/* Give out "You are at full grace." when needed -
		 * server will not send us anything when this happens */
		if (cpl.stats.grace == cpl.stats.maxgrace)
			draw_info("You are at full grace. You stop praying.", COLOR_WHITE);
	}
	else if (!strncasecmp(cmd, "/keybind", 8))
	{
		map_udate_flag = 2;

		if (cpl.menustatus != MENU_KEYBIND)
		{
			keybind_status = KEYBIND_STATUS_NO;
			cpl.menustatus = MENU_KEYBIND;
		}
		else
		{
			save_keybind_file(KEYBIND_FILE);
			cpl.menustatus = MENU_NO;
		}

		sound_play_effect("scroll.ogg", 100);
		reset_keys();
		return 1;
	}
	else if (!strncmp(cmd, "/target", 7))
	{
		/* Logic is: if first parameter char is a digit, is enemy, friend or self.
		 * If first char a character - then it's a name of a living object. */
		if (!strncmp(cmd, "/target friend", 14))
			strcpy(cmd, "/target 1");
		else if (!strncmp(cmd,"/target enemy", 13))
			strcpy(cmd, "/target 0");
		else if (!strncmp(cmd, "/target self", 12))
			strcpy(cmd, "/target 2");
	}
	else if (!strncmp(cmd, "/help", 5))
	{
		cmd += 5;

		if (cmd == NULL || strcmp(cmd, "") == 0)
			show_help("main");
		else
			show_help(cmd + 1);

		return 1;
	}
	else if (!strncmp(cmd, "/script ", 8))
	{
		cmd += 8;

		if (!strncmp(cmd, "load ", 5))
		{
			cmd += 5;

			script_load(cmd);
		}
		if (!strncmp(cmd, "unload ", 7))
		{
			cmd += 7;

			script_unload(cmd);
		}
		else if (!strncmp(cmd, "list", 4))
		{
			script_list();
		}
		else if (!strncmp(cmd, "send ", 5))
		{
			cmd += 5;

			script_send(cmd);
		}

		return 1;
	}
	else if (!strncmp(cmd, "/ignore", 7))
	{
		ignore_command(cmd + 7);
		return 1;
	}
	else if (!strncmp(cmd, "/reply", 6))
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == '\0')
		{
			draw_info("Usage: /reply <message>", COLOR_RED);
		}
		else
		{
			if (!cpl.player_reply[0])
			{
				draw_info("There is no one you can /reply.", COLOR_RED);
			}
			else
			{
				char buf[2048];

				snprintf(buf, sizeof(buf), "/tell %s %s", cpl.player_reply, cmd);
				send_command(buf);
			}
		}

		return 1;
	}
	else if (!strncmp(cmd, "/resetwidgets", 13))
	{
		reset_widget(NULL);
		return 1;
	}
	else if (!strncmp(cmd, "/resetwidget", 12))
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == '\0')
		{
			draw_info("Usage: /resetwidget <name>", COLOR_RED);
		}
		else
		{
			reset_widget(cmd);
		}

		return 1;
	}

	return 0;
}

/**
 * Blit face from inventory located by tag.
 * @param tag Item tag to locate
 * @param x X position to blit the item
 * @param y Y position to blit the item */
void blt_inventory_face_from_tag(int tag, int x, int y)
{
	object *tmp;

	/* Check item is in inventory and faces are loaded, etc */
	tmp = object_find(tag);

	if (!tmp)
		return;

	blt_inv_item_centered(tmp, x, y);
}

/**
 * Show one of the menus (book, party, etc). */
void show_menu()
{
	SDL_Rect box;

	if (!cpl.menustatus)
		return;

	if (cpl.menustatus == MENU_KEYBIND)
		show_keybind();
	else if (cpl.menustatus == MENU_BOOK)
		book_show();
	else if (cpl.menustatus == MENU_REGION_MAP)
	{
		region_map_show();
	}
	else if (cpl.menustatus == MENU_PARTY)
		show_party();
	else if (cpl.menustatus == MENU_SPELL)
	{
		show_spelllist();
		box.x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
		box.y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2 - 42;
		box.h = 42;
		box.w = Bitmaps[BITMAP_DIALOG_BG]->bitmap->w;
		SDL_FillRect(ScreenSurface, &box, 0);
		show_quickslots(box.x + 120, box.y + 3, 0);
	}
	else if (cpl.menustatus == MENU_SKILL)
		show_skilllist();
	else if (cpl.menustatus == MENU_OPTION)
		show_optwin();
}

/**
 * Blit a window slider.
 * @param slider Sprite of the slider
 * @param maxlen Maximum length of the slider
 * @param winlen Window length
 * @param startoff Start position offset
 * @param len Length of the slider
 * @param x X position of the slider to display
 * @param y Y position of the slider to display */
void blt_window_slider(_Sprite *slider, int maxlen, int winlen, int startoff, int len, int x, int y)
{
	SDL_Rect box;
	double temp;
	int startpos, len_h;

	if (len != -1)
		len_h = len;
	else
		len_h = slider->bitmap->h;

	if (maxlen < winlen)
		maxlen = winlen;

	if (startoff + winlen > maxlen)
		maxlen = startoff + winlen;

	box.x = 0;
	box.y = 0;
	box.w = slider->bitmap->w;

	/* now we have 100% = 1.0 to 0% = 0.0 of the length */

	/* between 0.0 <-> 1.0 */
	temp = (double)winlen / (double)maxlen;

	/* startpixel */
	startpos = (int)((double)startoff * ((double)len_h / (double )maxlen));

	temp = (double)len_h * temp;
	box.h = (Uint16) temp;

	if (!box.h)
		box.h = 1;

	if (startoff + winlen >= maxlen && startpos + box.h < len_h)
		startpos ++;

	sprite_blt(slider, x, y + startpos, &box, NULL);
}

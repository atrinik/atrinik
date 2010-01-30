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

/** Spell list entries */
struct _spell_list spell_list[SPELL_LIST_MAX];

/** Skill list entries */
struct _skill_list skill_list[SKILL_LIST_MAX];

/** Spell list set */
struct _dialog_list_set spell_list_set;

/** Skill list set */
struct _dialog_list_set skill_list_set;

/** Option list set */
struct _dialog_list_set option_list_set;

/** Bindkey list set */
struct _dialog_list_set bindkey_list_set;

/** Create list set */
struct _dialog_list_set create_list_set;

static char *get_range_item_name(int id);

/** Quick slot entries */
_quickslot quick_slots[MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS];

/** Current quickslot group */
int quickslot_group = 1;

/** Quickslot positions, because some things change depending on
  * which quickslot bitmap is displayed. */
int quickslots_pos[MAX_QUICK_SLOTS][2] =
{
	{17,	1},
	{50,	1},
	{83,	1},
	{116,	1},
	{149,	1},
	{182,	1},
	{215,	1},
	{248,	1}
};

/**
 * Wait for console input.
 * If ESC was pressed or this is input for party menu, just close the console. */
void do_console()
{
	map_udate_flag = 2;

	/* If ESC was pressed or console_party() returned 1, close console. */
	if (InputStringEscFlag || console_party())
	{
		if (gui_interface_party)
			clear_party_interface();

		sound_play_effect(SOUND_CONSOLE, 0, 100);
		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_CONSOLE_ID].show = 0;
	}

	/* If set, we've got a finished input */
	if (InputStringFlag == 0 && InputStringEndFlag)
	{
		sound_play_effect(SOUND_CONSOLE, 0, 100);

		if (InputString[0])
		{
			char buf[MAX_INPUT_STRING + 32];

#if 0
			sprintf(buf, ":%s", InputString);
			draw_info(buf, COLOR_DGOLD);
#endif

			/* If it's not command, it's say */
			if (*InputString != '/')
			{
				snprintf(buf, sizeof(buf), "/say %s", InputString);
			}
			else
			{
				strcpy(buf, InputString);
			}

			if (!client_command_check(InputString))
				send_command(buf, -1, SC_NORMAL);
		}

		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_CONSOLE_ID].show = 0;
	}
	else
		cur_widget[IN_CONSOLE_ID].show = 1;
}

/**
 * Analyze /cmd type commands the player has typed in the console or bound to a key.
 * Sort out the "client intern" commands and expand or pre process them for the server.
 * @param cmd Command to check
 * @return 0 to send command to server, 1 to not send it */
int client_command_check(char *cmd)
{
	char tmp[256];
	char cpar1[256];
	int par2;

	if (!strnicmp(cmd, "/ready_spell", 12))
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
								sound_play_effect(SOUND_SCROLL, 0, MENU_SOUND_VOL);
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
								sound_play_effect(SOUND_SCROLL, 0, MENU_SOUND_VOL);
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
	else if (!strnicmp(cmd, "/pray", 5))
	{
		/* Give out "You are at full grace." when needed -
		 * server will not send us anything when this happens */
		if (cpl.stats.grace == cpl.stats.maxgrace)
			draw_info("You are at full grace. You stop praying.", COLOR_WHITE);
	}
	else if (!strnicmp(cmd, "/setwinalpha", 12))
	{
		int wrong = 0;
		par2 = -1;
		sscanf(cmd, "%s %s %d", tmp, cpar1, &par2);

		if (!strnicmp(cpar1, "ON", 2))
		{
			if (par2 != -1)
			{
				if (par2 < 0 || par2 > 255)
					par2 = 128;

				options.textwin_alpha = par2;
			}

			options.use_TextwinAlpha = 1;
			sprintf(tmp, ">>set textwin alpha ON (alpha=%d).", options.textwin_alpha);
			draw_info(tmp, COLOR_GREEN);
		}
		else if (!strnicmp(cpar1, "OFF", 3))
		{
			draw_info(">>set textwin alpha mode OFF.", COLOR_GREEN);
			options.use_TextwinAlpha = 0;
		}
		else
			wrong = 1;

		if (wrong)
			draw_info("Usage: '/setwinalpha on|off |<alpha>|'\nExample:\n/setwinalpha ON 172\n/setwinalpha OFF",COLOR_WHITE);

		return 1;
	}
	else if (!strnicmp(cmd, "/keybind", 8))
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

		sound_play_effect(SOUND_SCROLL, 0, 100);
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
	else if (!strncmp(cmd, "/party ", 7))
	{
		cmd += 7;

		if (!strncmp(cmd, "join ", 5))
		{
			cmd += 5;

			sprintf(tmp, "pt join %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "leave", 5))
		{
			strcpy(cpl.partyname, "");
		}
		else if (!strncmp(cmd, "form ", 5))
		{
			cmd += 5;

			sprintf(tmp, "pt form %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "password ", 9))
		{
			cmd += 9;

			sprintf(tmp, "pt password %s", cmd);
			cs_write_string(csocket.fd, tmp, strlen(tmp));

			return 1;
		}
		else if (!strncmp(cmd, "who", 3))
		{
			cs_write_string(csocket.fd, "pt who", 6);

			return 1;
		}
		else if (!strncmp(cmd, "list", 4))
		{
			cs_write_string(csocket.fd, "pt list", 7);

			return 1;
		}
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
	else if (!strncmp(cmd, "/shop", 5))
	{
		initialize_shop(SHOP_STATE_NONE);

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
				send_command(buf, -1, SC_NORMAL);
			}
		}

		return 1;
	}

	return 0;
}

/**
 * Show a help GUI.
 * Uses book GUI to show the help.
 * @param helpname Help to be shown. */
void show_help(char *helpname)
{
	int len;
	unsigned char *data;
	char message[HUGE_BUF * 16];
	help_files_struct *help_files_tmp;

	/* This will be the default message if we don't find the help we want */
	snprintf(message, sizeof(message), "<b t=\"Help not found\"><t t=\"The specified help file could not be found.\">\n");

	/* Loop through the linked list of help files */
	for (help_files_tmp = help_files; help_files_tmp; help_files_tmp = help_files_tmp->next)
	{
		/* If title or message are empty or helpname doesn't match, just continue to the next item */
		if (help_files_tmp->title[0] == '\0' || help_files_tmp->message[0] == '\0' || strcmp(help_files_tmp->helpname, helpname))
		{
			continue;
		}

		/* Got what we wanted, replace it with the default message */
		snprintf(message, sizeof(message), "<b t=\"%s\"><t t=\"%s\">%s", help_files_tmp->helpname, help_files_tmp->title, help_files_tmp->message);

		/* Break out */
		break;
	}

	data = (uint8 *) message;

	len = strlen((char *) data);

	cpl.menustatus = MENU_BOOK;

	gui_interface_book = book_gui_load((char *) data, len);
}

/**
 * Wait for number input.
 * If ESC was pressed, close the input widget. */
void do_number()
{
	map_udate_flag = 2;

	if (InputStringEscFlag)
	{
		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_NUMBER_ID].show = 0;
	}

	/* if set, we got a finished input!*/
	if (InputStringFlag == 0 && InputStringEndFlag)
	{
		if (InputString[0])
		{
			int tmp;
			char buf[300];
			tmp = atoi(InputString);

			/* If you enter a number higher than the real nrof, you will pickup all */
			if (tmp > cpl.nrof)
				tmp = cpl.nrof;

			if (tmp > 0 && tmp <= cpl.nrof)
			{
				client_send_move(cpl.loc, cpl.tag, tmp);
				snprintf(buf, sizeof(buf), "%s %d from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", tmp, cpl.nrof, cpl.num_text);

				if (cpl.nummode == NUM_MODE_GET)
					sound_play_effect(SOUND_GET, 0, 100);
				else
					sound_play_effect(SOUND_DROP, 0, 100);

				draw_info(buf, COLOR_DGOLD);
			}
		}

		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_NUMBER_ID].show = 0;
	}
	else
		cur_widget[IN_NUMBER_ID].show = 1;
}

/**
 * Wait for input in the keybind menu.
 * If ESC was pressed, close the input. */
void do_keybind_input()
{
	if (InputStringEscFlag)
	{
		reset_keys();
		sound_play_effect(SOUND_CLICKFAIL, 0, 100);
		cpl.input_mode = INPUT_MODE_NO;
		keybind_status = KEYBIND_STATUS_NO;
		map_udate_flag = 2;
	}

	/* If set, we got a finished input */
	if (InputStringFlag == 0 && InputStringEndFlag)
	{
		if (InputString[0])
		{
			strcpy(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text, InputString);
			/* Now get the key code */
			keybind_status = KEYBIND_STATUS_EDITKEY;
		}
		/* Cleared string - delete entry */
		else
		{
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text[0] = '\0';
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].keyname[0] = '\0';
			bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].key = '\0';
			keybind_status = KEYBIND_STATUS_NO;
		}

		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		map_udate_flag = 2;
	}
}

/** Few letters representation of a protection ID */
static char *protections[20] =
{
	"I", 	"S", 	"C", 	"P", 	"W",
	"F",	"C",	"E",	"P",	"A",
	"M",	"Mi",	"B",	"P",	"F",
	"N",	"Ch",	"D",	"Sp",	"Co"
};

/**
 * Show the protection table widget.
 * @param x X position of the widget
 * @param y Y position of the widget */
void widget_show_resist(int x, int y)
{
	char buf[12];
	SDL_Rect box;
	_BLTFX bltfx;
	int protectionID, protection_x = 0, protection_y = 2;

	if (!widgetSF[RESIST_ID])
		widgetSF[RESIST_ID] = SDL_ConvertSurface(Bitmaps[BITMAP_RESIST_BG]->bitmap, Bitmaps[BITMAP_RESIST_BG]->bitmap->format, Bitmaps[BITMAP_RESIST_BG]->bitmap->flags);

	if (cur_widget[RESIST_ID].redraw)
	{
		cur_widget[RESIST_ID].redraw = 0;

		bltfx.surface = widgetSF[RESIST_ID];
		bltfx.flags = 0;
		bltfx.alpha = 0;

		sprite_blt(Bitmaps[BITMAP_RESIST_BG], 0, 0, NULL, &bltfx);

		StringBlt(widgetSF[RESIST_ID], &Font6x3Out, "Protection Table", 5, 1, COLOR_HGOLD, NULL, NULL);

		/* This is a dynamic protection table, unlike the old one.
		 * It reduces the code by a considerable amount. */
		for (protectionID = 0; protectionID < (int) sizeof(cpl.stats.protection) / 2; protectionID++)
		{
			/* We have 4 groups of protections. That means we
			 * will need 4 lines to output them all. Adjust
			 * the x and y for it. */
			if (protectionID == 0 || protectionID == 5 || protectionID == 10 || protectionID == 15)
			{
				protection_y += 15;
				protection_x = 43;
			}

			/* Switch the protection ID, so we can output the groups. */
			switch (protectionID)
			{
					/* Physical */
				case 0:
					StringBlt(widgetSF[RESIST_ID], &Font6x3Out, "Physical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Elemental */
				case 5:
					StringBlt(widgetSF[RESIST_ID], &Font6x3Out, "Elemental", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Magical */
				case 10:
					StringBlt(widgetSF[RESIST_ID], &Font6x3Out, "Magical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;

					/* Spherical */
				case 15:
					StringBlt(widgetSF[RESIST_ID], &Font6x3Out, "Spherical", 5, protection_y, COLOR_HGOLD, NULL, NULL);
					break;
			}

			/* Output the protection few letters name from the table 'protections'. */
			StringBlt(widgetSF[RESIST_ID], &SystemFont, protections[protectionID], protection_x + 2 - strlen(protections[protectionID]) * 2, protection_y, COLOR_HGOLD, NULL, NULL);

			/* Now output the protection value. No protection will be drawn gray,
			 * some protection will be white, and immunity (100%) will be orange. */
			snprintf(buf, sizeof(buf), "%02d", cpl.stats.protection[protectionID]);
			StringBlt(widgetSF[RESIST_ID], &SystemFont, buf, protection_x + 10, protection_y, cpl.stats.protection[protectionID] ? (cpl.stats.protection[protectionID] == 100 ? COLOR_ORANGE : COLOR_WHITE) : COLOR_GREY, NULL, NULL);

			protection_x += 30;
		}
	}

	box.x = x;
	box.y = y;
	SDL_BlitSurface(widgetSF[RESIST_ID], NULL, ScreenSurface, &box);
}

/** Default icon length */
#define ICONDEFLEN 32

/**
 * Locate and return the name of range item.
 * @param tag Item tag
 * @return "Nothing" if no range item, the range item name otherwise */
static char *get_range_item_name(int tag)
{
	item *tmp;

	if (tag != FIRE_ITEM_NO)
	{
		tmp = locate_item(tag);

		if (tmp)
			return tmp->s_name;
	}

	return "Nothing";
}

/**
 * Blit face from inventory located by tag.
 * @param tag Item tag to locate
 * @param x X position to blit the item
 * @param y Y position to blit the item */
void blt_inventory_face_from_tag(int tag, int x, int y)
{
	item *tmp;

	/* Check item is in inventory and faces are loaded, etc */
	tmp = locate_item(tag);

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
		book_gui_show();
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
		show_quickslots(box.x + 120, box.y + 3);
	}
	else if (cpl.menustatus == MENU_SKILL)
		show_skilllist();
	else if (cpl.menustatus == MENU_OPTION)
		show_optwin();
	else if (cpl.menustatus == MENU_CREATE)
		show_newplayer_server();
}


/**
 * Initialize media from tag.
 * @param tag String to init the media from, usually comes from map data
 * @return 1 if success, 0 if not */
int init_media_tag(char *tag)
{
	char *p1, *p2;
	int ret = 0;

	if (tag == NULL)
	{
		LOG(LOG_MSG, "MediaTagError: Tag == NULL\n");
		return ret;
	}

	p1 = strchr(tag, '|');
	p2 = strrchr(tag, '|');

	if (p1 == NULL || p2 == NULL)
	{
		LOG(LOG_MSG, "MediaTagError: Parameter == NULL (%x %x)\n", p1, p2);
		return ret;
	}

	*p1++ = 0;
	*p2++ = 0;

	if (strstr(tag, ".ogg"))
	{
		sound_play_music(tag, options.music_volume, atoi(p1), atoi(p2), MUSIC_MODE_NORMAL);
		/* Because we have called sound_play_music, we don't have to fade out extern */
		ret = 1;
	}

	return ret;
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

/**
 * Load temporary animations. */
static int load_anim_tmp()
{
	int i, anim_len = 0, new_anim = 1;
	uint8 faces = 0;
	uint16 count = 0, face_id;
	FILE *stream;
	char buf[HUGE_BUF];
	unsigned char anim_cmd[2048];

	/* Clear both animation tables
	 * This *must* be reloaded every time we connect
	 * - remember that different servers can have different
	 * animations! */
	for (i = 0; i < MAXANIM; i++)
	{
		if (animations[i].faces)
			free(animations[i].faces);

		if (anim_table[i].anim_cmd)
			free(anim_table[i].anim_cmd);
	}

	memset(animations, 0, sizeof(animations));

	/* Animation #0 is like face id #0 a bug catch - if ever
	 * appear in game flow its a sign of a uninit of simply
	 * buggy operation. */
	anim_cmd[0] = (unsigned char)((count >> 8) & 0xff);
	anim_cmd[1] = (unsigned char)(count & 0xff);

	/* flags ... */
	anim_cmd[2] = 0;
	anim_cmd[3] = 1;
	/* face id o */
	anim_cmd[4] = 0;
	anim_cmd[5] = 0;

	anim_table[count].anim_cmd = malloc(6);
	memcpy(anim_table[count].anim_cmd, anim_cmd, 6);
	anim_table[count].len = 6;

	count++;

	if ((stream = fopen_wrapper(FILE_ANIMS_TMP, "rt")) == NULL)
	{
		LOG(LOG_ERROR, "load_anim_tmp: Error reading anim.tmp!");
		/* Fatal */
		SYSTEM_End();
		exit(0);
	}

	while (fgets(buf, HUGE_BUF - 1, stream) != NULL)
	{
		/* Are we outside an anim body? */
		if (new_anim == 1)
		{
			if (!strncmp(buf, "anim ", 5))
			{
				new_anim = 0;
				faces = 0;
				anim_cmd[0] = (unsigned char)((count >> 8) & 0xff);
				anim_cmd[1] = (unsigned char)(count & 0xff);
				faces = 1;
				anim_len = 4;
			}
			/* we should never hit this point */
			else
			{
				LOG(LOG_ERROR, "ERROR: load_anim_tmp(): Error parsing anims.tmp - unknown cmd: >%s<!\n", buf);
			}
		}
		/* No, we are inside! */
		else
		{
			if (!strncmp(buf, "facings ", 8))
			{
				faces = atoi(buf + 8);
			}
			else if (!strncmp(buf, "mina", 4))
			{
#if 0
				LOG(LOG_DEBUG, "LOAD ANIM: #%d - len: %d (%d)\n", count, anim_len, faces);
#endif
				/* flags ... */
				anim_cmd[2] = 0;
				/* facings */
				anim_cmd[3] = faces;
				anim_table[count].len = anim_len;
				anim_table[count].anim_cmd = malloc(anim_len);
				memcpy(anim_table[count].anim_cmd, anim_cmd, anim_len);
				count++;
				new_anim = 1;
			}
			else
			{
				face_id = (uint16) atoi(buf);
				anim_cmd[anim_len++] = (unsigned char)((face_id >> 8) & 0xff);
				anim_cmd[anim_len++] = (unsigned char)(face_id & 0xff);
			}
		}
	}

	fclose(stream);
	return 1;
}


/**
 * Read temporary animations. */
int read_anim_tmp()
{
	FILE *stream, *ftmp;
	int i, new_anim = 1, count = 1;
	char buf[HUGE_BUF], cmd[HUGE_BUF];
	struct stat	stat_bmap, stat_anim, stat_tmp;

	/* if this fails, we have a urgent problem somewhere before */
	if ((stream = fopen_wrapper(FILE_BMAPS_TMP, "rb" )) == NULL)
	{
		LOG(LOG_ERROR, "read_anim_tmp:Error reading bmap.tmp for anim.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	fstat(fileno(stream), &stat_bmap);
	fclose(stream);

	if ( (stream = fopen_wrapper(FILE_CLIENT_ANIMS, "rb" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error reading bmap.tmp for anim.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}
	fstat(fileno(stream), &stat_anim);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_ANIMS_TMP, "rb" )) != NULL )
	{
		fstat(fileno(stream), &stat_tmp);
		fclose( stream );

		/* our anim file must be newer as our default anim file */
		if (difftime(stat_tmp.st_mtime, stat_anim.st_mtime) > 0.0f)
		{
			/* our anim file must be newer as our bmaps.tmp */
			if (difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
				return load_anim_tmp(); /* all fine - load file */
		}
	}

	unlink(FILE_ANIMS_TMP); /* for some reason - recreate this file */
	if ( (ftmp = fopen_wrapper(FILE_ANIMS_TMP, "wt" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error opening anims.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}

	if ( (stream = fopen_wrapper(FILE_CLIENT_ANIMS, "rt" )) == NULL )
	{
		LOG(LOG_ERROR,"read_anim_tmp:Error reading client_anims for anims.tmp!");
		SYSTEM_End(); /* fatal */
		exit(0);
	}

	while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%s",cmd);
		if (new_anim == 1) /* we are outside a anim body ? */
		{
			if (!strncmp(buf, "anim ",5))
			{
				sprintf(cmd, "anim %d -> %s",count++, buf);
				fputs(cmd,ftmp); /* safe this key string! */
				new_anim = 0;
			}
			else /* we should never hit this point */
			{
				LOG(LOG_ERROR,"read_anim_tmp:Error parsing client_anim - unknown cmd: >%s<!\n", cmd);
			}
		}
		else /* no, we are inside! */
		{
			if (!strncmp(buf, "facings ",8))
			{
				fputs(buf, ftmp); /* safe this key word! */
			}
			else if (!strncmp(cmd, "mina",4))
			{
				fputs(buf, ftmp); /* safe this key word! */
				new_anim = 1;
			}
			else
			{
				/* this is really slow when we have more pictures - we
				 * browsing #anim * #bmaps times the same table -
				 * pretty bad - when we stay to long here, we must create
				 * for bmaps.tmp entries a hash table too. */
				for (i=0;i<bmaptype_table_size;i++)
				{
					if (!strcmp(bmaptype_table[i].name,cmd))
						break;
				}

				if (i>=bmaptype_table_size)
				{
					/* if we are here then we have a picture name in the anims file
					 * which we don't have in our bmaps file! Pretty bad. But because
					 * face #0 is ALWAYS bug.101 - we simply use it here! */
					i=0;
					LOG(LOG_ERROR,"read_anim_tmp: Invalid anim name >%s< - set to #0 (bug.101)!\n", cmd);
				}
				sprintf(cmd, "%d\n",i);
				fputs(cmd, ftmp);
			}
		}
	}

	fclose( stream );
	fclose( ftmp );
	return load_anim_tmp(); /* all fine - load file */
}

/**
 * Read animations from file. */
void read_anims()
{
	FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	LOG(LOG_DEBUG, "Loading %s...", FILE_CLIENT_ANIMS);
	srv_client_files[SRV_CLIENT_ANIMS].len = 0;
	srv_client_files[SRV_CLIENT_ANIMS].crc = 0;

	if ((stream = fopen_wrapper(FILE_CLIENT_ANIMS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_ANIMS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_ANIMS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_ANIMS].len, srv_client_files[SRV_CLIENT_ANIMS].crc);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * After we tested and/or created bmaps.p0, load the data from it. */
static void load_bmaps_p0()
{
	char buf[HUGE_BUF];
	char name[HUGE_BUF];
	int len, pos, num;
	unsigned int crc;
	_bmaptype *at;
	FILE *fbmap;

	/* Clear bmap hash table */
	memset((void *) bmap_table, 0, BMAPTABLE * sizeof(_bmaptype *));

	/* Try to open bmaps_p0 file */
	if ((fbmap = fopen_wrapper(FILE_BMAPS_P0, "rb")) == NULL)
	{
		LOG(LOG_ERROR, "ERROR: Error loading bmaps.p0!\n");
		/* Fatal */
		SYSTEM_End();
		unlink(FILE_BMAPS_P0);
		exit(0);
	}

	while (fgets(buf, HUGE_BUF - 1, fbmap) != NULL)
	{
		sscanf(buf, "%d %d %x %d %s", &num, &pos, &crc, &len, name);

		at = (_bmaptype *) malloc(sizeof(_bmaptype));
		at->name = (char *) malloc(strlen(name) + 1);
		strcpy(at->name, name);
		at->crc = crc;
		at->num = num;
		at->len = len;
		at->pos = pos;
		add_bmap(at);
#if 0
		LOG(LOG_DEBUG, "%d %d %d %x >%s<\n", num, pos, len, crc, name);
#endif
	}

	fclose(fbmap);
}

/**
 * Read and/or create the bmaps.p0 file out of the
 * atrinik.p0 file. */
void read_bmaps_p0()
{
	FILE *fbmap, *fpic;
	char *temp_buf, *cp;
	int bufsize, len, num,  pos;
	unsigned int crc;
	char buf[HUGE_BUF];
	struct stat	bmap_stat, pic_stat;

	if ((fpic = fopen_wrapper(FILE_ATRINIK_P0, "rb")) == NULL)
	{
		LOG(LOG_ERROR, "ERROR: Can't find atrinik.p0 file!\n");
		/* Fatal */
		SYSTEM_End();
		unlink(FILE_BMAPS_P0);
		exit(0);
	}

	/* Get time stamp of the file atrinik.p0 */
	fstat(fileno(fpic), &pic_stat);

	/* Try to open bmaps_p0 file */
	if ((fbmap = fopen_wrapper(FILE_BMAPS_P0, "r" )) == NULL)
		goto create_bmaps;

	/* Get time stamp of the file */
	fstat(fileno(fbmap), &bmap_stat);
	fclose(fbmap);

	if (difftime(pic_stat.st_mtime, bmap_stat.st_mtime) > 0.0f)
		goto create_bmaps;

	fclose(fpic);
	load_bmaps_p0();
	return;

	/* If we are here, then we have to (re)create the bmaps.p0 file */
create_bmaps:
	if ((fbmap = fopen_wrapper(FILE_BMAPS_P0, "w")) == NULL)
	{
		LOG(LOG_ERROR, "ERROR: Can't create bmaps.p0 file!\n");
		/* Fatal */
		SYSTEM_End();
		fclose(fbmap);
		unlink(FILE_BMAPS_P0);
		exit(0);
	}

	temp_buf = malloc((bufsize = 24 * 1024));

	while (fgets(buf, HUGE_BUF - 1, fpic) != NULL)
	{
		if (strncmp(buf, "IMAGE ", 6) != 0)
		{
			LOG(LOG_ERROR, "ERROR: read_client_images(): Bad image line - not IMAGE, instead\n%s\n", buf);
			/* Fatal */
			SYSTEM_End();
			fclose(fbmap);
			fclose(fpic);
			unlink(FILE_BMAPS_P0);
			exit(0);
		}

		num = atoi(buf + 6);

		/* Skip accross the number data */
		for (cp = buf + 6; *cp != ' '; cp++);

		len = atoi(cp);

		strcpy(buf, cp);
		pos = (int) ftell(fpic);

		/* Dynamic buffer adjustment */
		if (len > bufsize)
		{
			free(temp_buf);

			/* We assume that this is nonsense */
			if (len > 128 * 1024)
			{
				LOG(LOG_ERROR, "ERROR: read_client_images(): Size of picture out of bounds!(len:%d)(pos:%d)\n", len, pos);
				/* Fatal */
				SYSTEM_End();
				fclose(fbmap);
				fclose(fpic);
				unlink(FILE_BMAPS_P0);
				exit(0);
			}

			bufsize = len;
			temp_buf = malloc(bufsize);
		}

		if (!fread(temp_buf, 1, len, fpic))
			return;

		crc = crc32(1L, (const unsigned char FAR *) temp_buf,len);

		/* Now we got all we needed */
		sprintf(temp_buf, "%d %d %x %s", num, pos, crc, buf);
		fputs(temp_buf, fbmap);
#if 0
		LOG(LOG_DEBUG, "FOUND: %s", temp_buf);
#endif
	}

	free(temp_buf);
	fclose(fbmap);
	fclose(fpic);
	load_bmaps_p0();
}

/**
 * Free temporary bitmaps. */
void delete_bmap_tmp()
{
	int i;

	bmaptype_table_size = 0;

	for (i = 0; i < MAX_BMAPTYPE_TABLE; i++)
	{
		if (bmaptype_table[i].name)
			free(bmaptype_table[i].name);

		bmaptype_table[i].name = NULL;
	}
}

/**
 * Load temporary bitmaps. */
static int load_bmap_tmp()
{
	FILE *stream;
	char buf[HUGE_BUF],name[HUGE_BUF];
	int i=0,len, pos;
	unsigned int crc;

	delete_bmap_tmp();
	if ( (stream = fopen_wrapper(FILE_BMAPS_TMP, "rt" )) == NULL )
	{
		LOG(LOG_ERROR,"bmaptype_table(): error open file <bmap.tmp>");
		SYSTEM_End(); /* fatal */
		exit(0);
	}

	while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
	{
		sscanf(buf,"%d %d %x %s\n", &pos, &len, &crc, name);
		bmaptype_table[i].crc = crc;
		bmaptype_table[i].len = len;
		bmaptype_table[i].pos = pos;
		bmaptype_table[i].name =(char*) malloc(strlen(name)+1);
		strcpy(bmaptype_table[i].name,name);
		i++;
	}

	bmaptype_table_size=i;
	fclose( stream );
	return 0;
}


/**
 * Read temporary bitmaps file. */
int read_bmap_tmp()
{
	FILE *stream, *fbmap0;
	char buf[HUGE_BUF],name[HUGE_BUF];
	struct stat	stat_bmap, stat_tmp, stat_bp0;
	int len;
	unsigned int crc;
	_bmaptype *at;

	if ( (stream = fopen_wrapper(FILE_CLIENT_BMAPS, "rb" )) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}

	fstat(fileno(stream), &stat_bmap);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_BMAPS_P0, "rb" )) == NULL )
	{
		/* we can't make bmaps.tmp without this file */
		unlink(FILE_BMAPS_TMP);
		return 1;
	}

	fstat(fileno(stream), &stat_bp0);
	fclose( stream );

	if ( (stream = fopen_wrapper(FILE_BMAPS_TMP, "rb" )) == NULL )
		goto create_bmap_tmp;

	fstat(fileno(stream), &stat_tmp);
	fclose( stream );

	/* ok - client_bmap & bmaps.p0 are there - now check
	 * our bmap_tmp is newer - is not newer as both, we
	 * create it new - then it is newer. */

	if (difftime(stat_tmp.st_mtime, stat_bmap.st_mtime) > 0.0f)
	{
		if (difftime(stat_tmp.st_mtime, stat_bp0.st_mtime) > 0.0f)
			return load_bmap_tmp(); /* all fine */
	}

create_bmap_tmp:
	unlink(FILE_BMAPS_TMP);

	/* NOW we are sure... we must create us a new bmaps.tmp */
	if ( (stream = fopen_wrapper(FILE_CLIENT_BMAPS, "rb" )) != NULL )
	{
		/* we can use text mode here, its local */
		if ( (fbmap0 = fopen_wrapper(FILE_BMAPS_TMP, "wt" )) != NULL )
		{
			/* read in the bmaps from the server, check with the
			 * loaded bmap table (from bmaps.p0) and create with
			 * this information the bmaps.tmp file. */
			while (fgets(buf, HUGE_BUF-1, stream)!=NULL)
			{
				sscanf(buf,"%x %x %s", (unsigned int *)&len, &crc, name);
				at=find_bmap(name);

				/* now we can check, our local file package has
				 * the right png - if not, we mark this pictures
				 * as "in cache". We don't check it here now -
				 * that will happens at runtime.
				 * That can change when we include later a forbidden
				 * flag in the server (no face send) - then we need
				 * to break and upddate the picture and/or check the cache. */
				/* position -1 mark "not i the atrinik.p0 file */
				if (!at || at->len != len || at->crc != crc) /* is different or not there! */
					sprintf(buf,"-1 %d %x %s\n", len, crc, name);
				else /* we have it */
					sprintf(buf,"%d %d %x %s\n", at->pos, len, crc, name);
				fputs(buf, fbmap0);
			}

			fclose( fbmap0 );
		}

		fclose( stream );
	}

	return load_bmap_tmp(); /* all fine */
}


/**
 * Read bitmaps file. */
void read_bmaps()
{
	FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_BMAPS].len = 0;
	srv_client_files[SRV_CLIENT_BMAPS].crc = 0;
	LOG(LOG_DEBUG, "Reading %s...", FILE_CLIENT_BMAPS);

	if ((stream = fopen_wrapper(FILE_CLIENT_BMAPS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat (fileno (stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_BMAPS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_BMAPS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_BMAPS].len, srv_client_files[SRV_CLIENT_BMAPS].crc);
	}
	else
	{
		unlink(FILE_BMAPS_TMP);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * In the settings file we have a list of character templates
 * for character building. This function deletes this list. */
void delete_server_chars()
{
	_server_char *tmp, *tmp1;

	for (tmp1 = tmp = first_server_char; tmp1; tmp = tmp1)
	{
		tmp1 = tmp->next;
		free(tmp->name);
		free(tmp->desc[0]);
		free(tmp->desc[1]);
		free(tmp->desc[2]);
		free(tmp->desc[3]);
		free(tmp->char_arch[0]);
		free(tmp->char_arch[1]);
		free(tmp->char_arch[2]);
		free(tmp->char_arch[3]);
		free(tmp);
	}

	first_server_char = NULL;
}

/**
 * Find a face ID by name. Request the face by finding it, loading it or requesting it.
 * @param name Face name to find
 * @return Face ID if found, -1 otherwise */
static int get_bmap_id(char *name)
{
	int i;

	for (i=0;i<bmaptype_table_size;i++)
	{
		if (!strcmp(bmaptype_table[i].name,name))
		{
			request_face(i, 0);
			return i;
		}
	}

	return -1;
}

/**
 * Load settings file. */
void load_settings()
{
	FILE *stream;
	char buf[HUGE_BUF], buf1[HUGE_BUF], buf2[HUGE_BUF];
	char cmd[HUGE_BUF];
	char para[HUGE_BUF];
	int para_count = 0, last_cmd = 0;
	int tmp_level = 0;

	delete_server_chars();
	LOG(LOG_DEBUG, "Loading %s...\n", FILE_CLIENT_SETTINGS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SETTINGS, "rb")) != NULL)
	{
		while (fgets(buf, HUGE_BUF - 1, stream) != NULL)
		{
			if (buf[0] == '#' || buf[0] == '\0')
				continue;

			if (last_cmd == 0)
			{
				sscanf(adjust_string(buf), "%s %s", cmd, para);

				if (!strcmp(cmd, "char"))
				{
					_server_char *serv_char = malloc( sizeof(_server_char));

					memset(serv_char, 0, sizeof(_server_char));
					/* Copy name */
					serv_char->name = malloc(strlen(para) + 1);
					strcpy(serv_char->name, para);

					/* Get next legal line */
					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%s %d %d %d %d %d %d", buf1, &serv_char->bar[0], &serv_char->bar[1], &serv_char->bar[2], &serv_char->bar_add[0], &serv_char->bar_add[1], &serv_char->bar_add[2]);

					serv_char->pic_id = get_bmap_id(buf1);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[0], buf1, buf2);
					serv_char->char_arch[0] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[0], buf1);
					serv_char->face_id[0] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[1], buf1, buf2);
					serv_char->char_arch[1] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[1], buf1);
					serv_char->face_id[1] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %s %s", &serv_char->gender[2], buf1, buf2);
					serv_char->char_arch[2] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[2], buf1);
					serv_char->face_id[2] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf),"%d %s %s", &serv_char->gender[3], buf1, buf2);
					serv_char->char_arch[3] = malloc(strlen(buf1) + 1);
					strcpy(serv_char->char_arch[3], buf1);
					serv_char->face_id[3] = get_bmap_id(buf2);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					sscanf(adjust_string(buf), "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &serv_char->stat_points, &serv_char->stats[0], &serv_char->stats_min[0], &serv_char->stats_max[0], &serv_char->stats[1], &serv_char->stats_min[1], &serv_char->stats_max[1], &serv_char->stats[2], &serv_char->stats_min[2], &serv_char->stats_max[2], &serv_char->stats[3], &serv_char->stats_min[3], &serv_char->stats_max[3], &serv_char->stats[4], &serv_char->stats_min[4], &serv_char->stats_max[4], &serv_char->stats[5], &serv_char->stats_min[5], &serv_char->stats_max[5], &serv_char->stats[6], &serv_char->stats_min[6], &serv_char->stats_max[6]);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[0] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[0], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[1] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[1], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[2] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[2], buf);

					while (fgets(buf, HUGE_BUF - 1, stream) != NULL && (buf[0] == '#' || buf[0] == '\0'));

					serv_char->desc[3] = malloc(strlen(adjust_string(buf)) + 1);
					strcpy(serv_char->desc[3], buf);

					/* Add this char template to list */
					if (!first_server_char)
						first_server_char = serv_char;
					else
					{
						_server_char *tmpc;

						for (tmpc = first_server_char; tmpc->next; tmpc = tmpc->next);

						tmpc->next = serv_char;
						serv_char->prev = tmpc;
					}
				}
				else if (!strcmp(cmd, "level"))
				{
					tmp_level = atoi(para);

					if (tmp_level < 0 || tmp_level > 450)
					{
						fclose(stream);
						LOG(LOG_ERROR, "ERROR: load_settings(): Level command out of bounds! >%s<\n", buf);
						return;
					}

					server_level.level = tmp_level;
					/* Command 'level' */
					last_cmd = 1;
					para_count = 0;
				}
				/* We close here... better we include later a fallback to login */
				else
				{
					fclose(stream);
					LOG(LOG_ERROR, "ERROR: Unknown command in client_settings! >%s<\n", buf);
					return;
				}
			}
			else if (last_cmd == 1)
			{
				server_level.exp[para_count++] = strtoul(buf, NULL, 16);

				if (para_count >tmp_level)
					last_cmd = 0;
			}
		}

		fclose(stream);
	}

	if (first_server_char)
	{
		int g;

		memcpy(&new_character, first_server_char, sizeof(_server_char));

		/* Adjust gender */
		for (g = 0; g < 4; g++)
		{
			if (new_character.gender[g])
			{
				new_character.gender_selected = g;
				break;
			}
		}
	}
}


/**
 * Read settings file. */
void read_settings()
{
	FILE *stream;
	char *temp_buf;
	struct stat statbuf;
	int i;

	srv_client_files[SRV_CLIENT_SETTINGS].len = 0;
	srv_client_files[SRV_CLIENT_SETTINGS].crc = 0;
	LOG(LOG_DEBUG, "Reading %s...", FILE_CLIENT_SETTINGS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SETTINGS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SETTINGS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_SETTINGS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_SETTINGS].len, srv_client_files[SRV_CLIENT_SETTINGS].crc);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * Read spells file. */
void read_spells()
{
	int i, ii, panel;
	char type, nchar, *tmp, *tmp2;
	struct stat statbuf;
	FILE *stream;
	char *temp_buf;
	char line[255], name[255], d1[255], d2[255], d3[255], d4[255], icon[128];

	for (i = 0; i < SPELL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			spell_list[i].entry[0][ii].flag = LIST_ENTRY_UNUSED;
			spell_list[i].entry[1][ii].flag = LIST_ENTRY_UNUSED;
			spell_list[i].entry[0][ii].name[0] = 0;
			spell_list[i].entry[1][ii].name[0] = 0;
		}
	}

	spell_list_set.class_nr = 0;
	spell_list_set.entry_nr = 0;
	spell_list_set.group_nr = 0;

	srv_client_files[SRV_CLIENT_SPELLS].len = 0;
	srv_client_files[SRV_CLIENT_SPELLS].crc = 0;
	LOG(LOG_DEBUG, "Reading %s...", FILE_CLIENT_SPELLS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SPELLS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SPELLS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_SPELLS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		rewind(stream);

		for (i = 0; ; i++)
		{
			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(name, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			sscanf(line, "%c %c %d %s", &type, &nchar, &panel, icon);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d1, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d2, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d3, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d4, tmp + 1);
			panel--;

			spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].flag = LIST_ENTRY_USED;
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].icon_name, icon);

			sprintf(line, "%s%s", GetIconDirectory(), icon);
			spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].icon = sprite_load_file(line, SURFACE_FLAG_DISPLAYFORMAT);

			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].name, name);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[0], d1);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[1], d2);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[2], d3);
			strcpy(spell_list[panel].entry[type == 'w' ? 0 : 1][nchar - 'a'].desc[3], d4);
		}

		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_SPELLS].len, srv_client_files[SRV_CLIENT_SPELLS].crc);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * Frees the ::help_files structure. */
void free_help_files()
{
	help_files_struct *help_file_tmp, *help_file = help_files;

	while (help_file)
	{
		help_file_tmp = help_file->next;
		free(help_file);
		help_file = help_file_tmp;
	}

	help_files = NULL;
}

/**
 * Read help files from file.
 *
 * This will also initialize and fill the linked list of help
 * files. The {@link #show_help} function then only needs to
 * loop through that list. */
void read_help_files()
{
	FILE *stream;
	char *temp_buf, buf[HUGE_BUF];
	struct stat statbuf;
	int i;

	if (help_files)
	{
		free_help_files();
	}

	srv_client_files[SRV_CLIENT_HFILES].len = 0;
	srv_client_files[SRV_CLIENT_HFILES].crc = 0;

	LOG(LOG_DEBUG, "Reading %s...", FILE_CLIENT_HFILES);

	if ((stream = fopen_wrapper(FILE_CLIENT_HFILES, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_HFILES].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_HFILES].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);

		rewind(stream);

		/* Loop through the lines */
		while (fgets(buf, sizeof(buf), stream))
		{
			char helpname[MAX_BUF], title[MAX_BUF], message[HUGE_BUF * 12];
			help_files_struct *help_files_tmp;

			/* Null initialize the character arrays */
			title[0] = message[0] = helpname[0] = '\0';

			/* If this first line has "Name: " as first 6 characters (legal help file should) */
			if (strncmp(buf, "Name: ", 6) == 0)
			{
				/* Store it */
				snprintf(helpname, sizeof(helpname), "%s", buf + 6);

				/* Null terminate the new line of it */
				helpname[strlen(helpname) - 1] = '\0';
			}

			/* Loop trough the next lines, after the stop marker */
			while (fgets(buf, sizeof(buf), stream) && strcmp(buf, "==========\n"))
			{
				/* If this has "Title: " as first 7 characters */
				if (strncmp(buf, "Title: ", 7) == 0)
				{
					/* Store the title */
					snprintf(title, sizeof(title), "%s", buf + 7);

					/* Null terminate the new line of the title */
					title[strlen(title) - 1] = '\0';
				}
				/* Otherwise, it's a message */
				else
				{
					/* Put it at the end of our buffer with the message */
					strncat(message, buf, sizeof(message) - strlen(message) - 1);
				}
			}

			/* Allocate a new help files list */
			help_files_tmp = (help_files_struct *) malloc(sizeof(help_files_struct));

			/* Append the old help files list to it */
			help_files_tmp->next = help_files;

			/* Switch the old help files list with this one */
			help_files = help_files_tmp;

			/* Store help name */
			snprintf(help_files_tmp->helpname, sizeof(help_files_tmp->helpname), "%s", helpname);

			/* Store help title */
			snprintf(help_files_tmp->title, sizeof(help_files_tmp->title), "%s", title);

			/* Store help message (the important bit) */
			snprintf(help_files_tmp->message, sizeof(help_files_tmp->message), "%s", message);
		}

		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_HFILES].len, srv_client_files[SRV_CLIENT_HFILES].crc);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * Read skills from file */
void read_skills()
{
	int i, ii, panel;
	char *temp_buf;
	char nchar, *tmp, *tmp2;
	struct stat statbuf;
	FILE *stream;
	char line[255], name[255], d1[255], d2[255], d3[255], d4[255], icon[128];

	for (i = 0; i < SKILL_LIST_MAX; i++)
	{
		for (ii = 0; ii < DIALOG_LIST_ENTRY; ii++)
		{
			skill_list[i].entry[ii].flag = LIST_ENTRY_UNUSED;
			skill_list[i].entry[ii].name[0] = 0;
		}
	}

	skill_list_set.group_nr = 0;
	skill_list_set.entry_nr = 0;

	srv_client_files[SRV_CLIENT_SKILLS].len = 0;
	srv_client_files[SRV_CLIENT_SKILLS].crc = 0;
	LOG(LOG_DEBUG, "Reading %s...", FILE_CLIENT_SKILLS);

	if ((stream = fopen_wrapper(FILE_CLIENT_SKILLS, "rb")) != NULL)
	{
		/* Temporary load the file and get the data we need for compare with server */
		fstat(fileno(stream), &statbuf);
		i = (int) statbuf.st_size;
		srv_client_files[SRV_CLIENT_SKILLS].len = i;
		temp_buf = malloc(i);

		if (fread(temp_buf, 1, i, stream))
			srv_client_files[SRV_CLIENT_SKILLS].crc = crc32(1L, (const unsigned char FAR *) temp_buf, i);

		free(temp_buf);
		rewind(stream);

		for (i = 0; ; i++)
		{
			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(name, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			sscanf(line, "%d %c %s", &panel, &nchar, icon);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d1, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d2, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d3, tmp + 1);

			if (fgets(line, 255, stream) == NULL)
				break;

			line[250] = 0;
			tmp = strchr(line, '"');
			tmp2 = strchr(tmp + 1, '"');
			*tmp2 = 0;
			strcpy(d4, tmp + 1);

			skill_list[panel].entry[nchar - 'a'].flag = LIST_ENTRY_USED;
			skill_list[panel].entry[nchar - 'a'].exp = 0;
			skill_list[panel].entry[nchar - 'a'].exp_level = 0;

			strcpy(skill_list[panel].entry[nchar - 'a'].icon_name, icon);
			snprintf(line, sizeof(line), "%s%s", GetIconDirectory(), icon);
			skill_list[panel].entry[nchar - 'a'].icon = sprite_load_file(line, SURFACE_FLAG_DISPLAYFORMAT);

			strcpy(skill_list[panel].entry[nchar - 'a'].name, name);
			strcpy(skill_list[panel].entry[nchar - 'a'].desc[0], d1);
			strcpy(skill_list[panel].entry[nchar - 'a'].desc[1], d2);
			strcpy(skill_list[panel].entry[nchar - 'a'].desc[2], d3);
			strcpy(skill_list[panel].entry[nchar - 'a'].desc[3], d4);
		}

		fclose(stream);
		LOG(LOG_DEBUG, " Found file! (%d/%x)", srv_client_files[SRV_CLIENT_SKILLS].len, srv_client_files[SRV_CLIENT_SKILLS].crc);
	}

	LOG(LOG_DEBUG, " Done.\n");
}

/**
 * Mouse event on range widget.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param event SDL event type
 * @param MEvent Mouse event type (MOUSE_DOWN, MOUSE_UP) */
void widget_range_event(int x, int y, SDL_Event event, int MEvent)
{
	if (x > cur_widget[RANGE_ID].x1 + 5 && x < cur_widget[RANGE_ID].x1 + 38 && y >= cur_widget[RANGE_ID].y1 + 3 && y <= cur_widget[RANGE_ID].y1 + 33)
	{
		if (MEvent == MOUSE_DOWN)
		{
			if (event.button.button == SDL_BUTTON_LEFT)
				process_macro_keys(KEYFUNC_RANGE, 0);
			/* Mousewheel up */
			else if (event.button.button == 4)
				process_macro_keys(KEYFUNC_RANGE, 0);
		}
		else if (MEvent == MOUSE_UP)
		{
			if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
			{
				/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
				 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
				int old_inv_win = cpl.inventory_win;
				int old_inv_tag = cpl.win_inv_tag;
				cpl.inventory_win = IWIN_INV;

				/* Range field */
				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV && x >= cur_widget[RANGE_ID].x1 && x <= cur_widget[RANGE_ID].x1 + 78 && y >= cur_widget[RANGE_ID].y1 && y <= cur_widget[RANGE_ID].y1 + 35)
				{
					RangeFireMode = 4;

					/* Drop to player doll */
					process_macro_keys(KEYFUNC_APPLY, 0);
				}

				cpl.inventory_win = old_inv_win;
				cpl.win_inv_tag = old_inv_tag;
			}
		}
	}
}

/**
 * Mouse event for number input widget.
 * @param x Mouse X position
 * @param y Mouse Y position */
void widget_number_event(int x, int y)
{
	int mx = 0, my = 0;
	mx = x - cur_widget[IN_NUMBER_ID].x1;
	my = y - cur_widget[IN_NUMBER_ID].y1;

	/* Close number input */
	if (InputStringFlag && cpl.input_mode == INPUT_MODE_NUMBER)
	{
		if (mx > 239 && mx < 249 && my > 5 && my < 17)
		{
			SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
			InputStringFlag = 0;
			InputStringEndFlag = 1;
		}
	}
}

/**
 * Show widget console.
 * @param x X position of the console
 * @param y Y position of the console */
void widget_show_console(int x, int y)
{
	sprite_blt(Bitmaps[BITMAP_TEXTINPUT], x, y, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, show_input_string(InputString, &SystemFont, 239), x + 9, y + 7, COLOR_WHITE, NULL, NULL);
}

/**
 * Show widget number input.
 * @param x X position of the number input
 * @param y Y position of the number input */
void widget_show_number(int x, int y)
{
	SDL_Rect tmp;
	char buf[512];

	tmp.w = 238;

	sprite_blt(Bitmaps[BITMAP_NUMBER], x, y, NULL, NULL);
	snprintf(buf, sizeof(buf), "%s how many from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", cpl.nrof, cpl.num_text);

	StringBlt(ScreenSurface, &SystemFont, buf, x + 8, y + 6, COLOR_HGOLD, &tmp, NULL);
	StringBlt(ScreenSurface, &SystemFont, show_input_string(InputString, &SystemFont, Bitmaps[BITMAP_NUMBER]->bitmap->w - 22), x + 8, y + 25, COLOR_WHITE, &tmp, NULL);
}

/**
 * Show map name widget.
 * @param x X position of the map name
 * @param y Y position of the map name */
void widget_show_mapname(int x, int y)
{
	StringBlt(ScreenSurface, &SystemFont, MapData.name, x, y, COLOR_DEFAULT, NULL, NULL);
}

/**
 * Show range widget.
 * @param x X position of the range
 * @param y Y position of the range */
void widget_show_range(int x, int y)
{
	char buf[256];
	SDL_Rect rec_range;
	SDL_Rect rec_item;
	item *op;
	item *tmp;

	rec_range.w = 160;
	rec_item.w = 185;
	examine_range_inv();

	sprite_blt(Bitmaps[BITMAP_RANGE], x - 2, y, NULL, NULL);

	switch (RangeFireMode)
	{
		case FIRE_MODE_BOW:
			if (fire_mode_tab[FIRE_MODE_BOW].item != FIRE_ITEM_NO)
			{
				snprintf(buf, sizeof(buf), "using %s", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].item));
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].item, x + 3, y + 2);

				StringBlt(ScreenSurface, &SystemFont, buf, x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);

				if (fire_mode_tab[FIRE_MODE_BOW].amun != FIRE_ITEM_NO)
				{
					tmp = locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_BOW].amun);

					if (tmp)
					{
						if (tmp->itype == TYPE_ARROW)
							snprintf(buf, sizeof(buf), "ammo %s (%d)", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun), tmp->nrof);
						else
							snprintf(buf, sizeof(buf), "ammo %s", get_range_item_name(fire_mode_tab[FIRE_MODE_BOW].amun));
					}
					else
						snprintf(buf, sizeof(buf), "ammo not selected");

					blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_BOW].amun,x+43,y+2);
				}
				else
				{
					snprintf(buf, sizeof(buf), "ammo not selected");
				}

				StringBlt(ScreenSurface, &SystemFont, buf, x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}
			else
			{
				StringBlt(ScreenSurface, &SystemFont, "no range weapon applied", x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);
			}

			sprite_blt(Bitmaps[BITMAP_RANGE_MARKER], x + 3, y + 2, NULL, NULL);
			break;

			/* Wands, staves, rods and horns */
		case FIRE_MODE_WAND:
			if (!locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_WAND].item))
				fire_mode_tab[FIRE_MODE_WAND].item = FIRE_ITEM_NO;

			if (fire_mode_tab[FIRE_MODE_WAND].item != FIRE_ITEM_NO)
			{
				snprintf(buf, sizeof(buf), "%s", get_range_item_name(fire_mode_tab[FIRE_MODE_WAND].item));
				StringBlt(ScreenSurface, &SystemFont, buf, x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL], x + 3, y + 2, NULL, NULL);
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_WAND].item, x + 43, y + 2);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_TOOL_NO],x+3, y+2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "nothing applied", x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "use range tool", x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);
			break;

			/* The summon range ctrl will come from server only after the player cast a summon spell */
		case FIRE_MODE_SUMMON:
			if (fire_mode_tab[FIRE_MODE_SUMMON].item != FIRE_ITEM_NO)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_CTRL], x + 3, y + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SUMMON].name, x + 3, y + 46, COLOR_WHITE, NULL, NULL);
				sprite_blt(FaceList[fire_mode_tab[FIRE_MODE_SUMMON].item].sprite, x + 43, y + 2, NULL, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_CTRL_NO], x + 3, y + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no golem summoned", x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "mind control", x + 3, y + 35, COLOR_WHITE, &rec_item, NULL);
			break;

			/* These are client only, no server signal needed */
		case FIRE_MODE_SKILL:
			if (fire_mode_tab[FIRE_MODE_SKILL].skill)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL], x + 3, y + 2, NULL, NULL);

				if (fire_mode_tab[FIRE_MODE_SKILL].skill->flag != -1)
				{
					sprite_blt(fire_mode_tab[FIRE_MODE_SKILL].skill->icon, x + 43, y + 2, NULL, NULL);
					StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SKILL].skill->name, x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
				}
				else
					fire_mode_tab[FIRE_MODE_SKILL].skill = NULL;
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_SKILL_NO], x + 3, y + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no skill selected", x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "use skill", x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);

			break;

		case FIRE_MODE_SPELL:
			if (fire_mode_tab[FIRE_MODE_SPELL].spell)
			{
				/* We use wizard spells as default */
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD], x + 3, y + 2, NULL, NULL);

				if (fire_mode_tab[FIRE_MODE_SPELL].spell->flag != -1)
				{
					sprite_blt(fire_mode_tab[FIRE_MODE_SPELL].spell->icon, x + 43, y + 2, NULL, NULL);
					StringBlt(ScreenSurface, &SystemFont, fire_mode_tab[FIRE_MODE_SPELL].spell->name, x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
				}
				else
					fire_mode_tab[FIRE_MODE_SPELL].spell = NULL;
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_WIZARD_NO], x + 3, y + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no spell selected", x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}

			StringBlt(ScreenSurface, &SystemFont, "cast spell", x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);

			break;

		case FIRE_MODE_THROW:
			if (!(op = locate_item_from_item(cpl.ob, fire_mode_tab[FIRE_MODE_THROW].item)))
				fire_mode_tab[FIRE_MODE_THROW].item = FIRE_ITEM_NO;

			if (fire_mode_tab[FIRE_MODE_THROW].item != FIRE_ITEM_NO)
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW],x+3, y+2, NULL, NULL);
				blt_inventory_face_from_tag(fire_mode_tab[FIRE_MODE_THROW].item,x+43,y+2);

				if (op->nrof > 1)
				{
					if (op->nrof > 9999)
						snprintf(buf, sizeof(buf), "many");
					else
						snprintf(buf, sizeof(buf), "%d",op->nrof);

					StringBlt(ScreenSurface, &Font6x3Out, buf, x + 43 + (ICONDEFLEN / 2) - (get_string_pixel_length(buf, &Font6x3Out) / 2), y + 22, COLOR_WHITE, NULL, NULL);
				}

				snprintf(buf, sizeof(buf), "%s", get_range_item_name(fire_mode_tab[FIRE_MODE_THROW].item));
				StringBlt(ScreenSurface, &SystemFont,buf, x+3, y+46, COLOR_WHITE, &rec_item, NULL);
			}
			else
			{
				sprite_blt(Bitmaps[BITMAP_RANGE_THROW_NO], x + 3, y + 2, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, "no item ready", x + 3, y + 46, COLOR_WHITE, &rec_item, NULL);
			}

			snprintf(buf, sizeof(buf), "throw item");
			StringBlt(ScreenSurface, &SystemFont, buf, x + 3, y + 35, COLOR_WHITE, &rec_range, NULL);

			break;
	}
}

/**
 * Handle mouse events for target widget.
 * @param x Mouse X position
 * @param y Mouse Y position */
void widget_event_target(int x, int y)
{
	/* Combat modes */
	if (y > cur_widget[TARGET_ID].y1 + 3 && y < cur_widget[TARGET_ID].y1 + 38 && x > cur_widget[TARGET_ID].x1 + 3 && x < cur_widget[TARGET_ID].x1 + 30)
		check_keys(SDLK_c);

	/* Talk button */
	if (y > cur_widget[TARGET_ID].y1 + 7 && y < cur_widget[TARGET_ID].y1 + 25 && x > cur_widget[TARGET_ID].x1 + 223 && x < cur_widget[TARGET_ID].x1 + 259)
	{
		if (cpl.target_code)
			send_command("/t_tell hello", -1, SC_NORMAL);
	}
}

/**
 * Show target widget.
 * @param x X position of the target
 * @param y Y position of the target */
void widget_show_target(int x, int y)
{
	char *ptr = NULL;
	SDL_Rect box;
	double temp;
	int hp_tmp;

	sprite_blt(Bitmaps[BITMAP_TARGET_BG], x, y, NULL, NULL);

	sprite_blt(Bitmaps[cpl.target_mode ? BITMAP_TARGET_ATTACK : BITMAP_TARGET_NORMAL], x + 5, y + 4, NULL, NULL);

	sprite_blt(Bitmaps[BITMAP_TARGET_HP_B], x + 4, y + 24, NULL, NULL);

	hp_tmp = (int) cpl.target_hp;

	/* Redirect target_hp to our hp - server doesn't send it
	 * because we should know our hp exactly */
	if (cpl.target_code == 0)
		hp_tmp = (int)(((float) cpl.stats.hp / (float) cpl.stats.maxhp) * 100.0f);

	if (cpl.target_code == 0)
	{
		if (cpl.target_mode)
			ptr = "target self (hold attack)";
		else
			ptr = "target self";
	}
	else if (cpl.target_code == 1)
	{
		if (cpl.target_mode)
			ptr = "target and attack enemy";
		else
			ptr = "target enemy";
	}
	else if (cpl.target_code == 2)
	{
		if (cpl.target_mode)
			ptr = "target friend (hold attack)";
		else
			ptr = "target friend";
	}

	if (cpl.target_code)
	{
		int mx, my, mb;

		mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);

		sprite_blt(Bitmaps[BITMAP_TARGET_TALK], x + 223, y + 7, NULL, NULL);

		if (mx > x + 200 && mx < x + 200 + 20 && my > y + 3 && my < y + 13)
		{
			static int delta = 0;

			if (!(SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)))
			{
				delta = 0;
			}
			else if (mb && mb_clicked && !(delta++ & 7))
			{
				char tmp_buf[MAX_BUF];

				snprintf(tmp_buf, sizeof(tmp_buf), "shop load %s", cpl.target_name);
				cs_write_string(csocket.fd, tmp_buf, strlen(tmp_buf));
			}

			StringBlt(ScreenSurface, &SystemFont, "Shop", x + 200, y + 3, COLOR_HGOLD, NULL, NULL);
		}
		else
		{
			StringBlt(ScreenSurface, &SystemFont, "Shop", x + 200, y + 3, COLOR_WHITE, NULL, NULL);
		}
	}

	if (options.show_target_self || cpl.target_code != 0)
	{
		if (hp_tmp)
		{
			temp = (double) hp_tmp * 0.01;
			box.x = 0;
			box.y = 0;
			box.h = Bitmaps[BITMAP_TARGET_HP]->bitmap->h;
			box.w = (int) (Bitmaps[BITMAP_TARGET_HP]->bitmap->w * temp);

			if (!box.w)
				box.w = 1;

			if (box.w > Bitmaps[BITMAP_TARGET_HP]->bitmap->w)
				box.w = Bitmaps[BITMAP_TARGET_HP]->bitmap->w;

			sprite_blt(Bitmaps[BITMAP_TARGET_HP], x + 5, y + 25, &box, NULL);
		}

		if (ptr)
		{
			/* Draw the name of the target */
			StringBlt(ScreenSurface, &SystemFont, cpl.target_name, x + 35, y + 3, cpl.target_color, NULL, NULL);

			/* Either draw HP remaining percent and description... */
			if (hp_tmp > 0)
			{
				char hp_text[MAX_BUF];
				int hp_color;
				int xhpoffset = 0;

				snprintf(hp_text, sizeof(hp_text), "HP: %d%%", hp_tmp);

				if (hp_tmp > 90)
					hp_color = COLOR_GREEN;
				else if (hp_tmp > 75)
					hp_color = COLOR_DGOLD;
				else if (hp_tmp > 50)
					hp_color = COLOR_HGOLD;
				else if (hp_tmp > 25)
					hp_color = COLOR_ORANGE;
				else if (hp_tmp > 10)
					hp_color = COLOR_YELLOW;
				else
					hp_color = COLOR_RED;

				StringBlt(ScreenSurface, &SystemFont, hp_text, x + 35, y + 14, hp_color, NULL, NULL);
				xhpoffset = 50;

				StringBlt(ScreenSurface, &SystemFont, ptr, x + 35 + xhpoffset, y + 14, cpl.target_color, NULL, NULL);
			}
			/* Or draw just the description */
			else
				StringBlt(ScreenSurface, &SystemFont, ptr, x + 35, y + 14, cpl.target_color, NULL, NULL);
		}
	}
}

/**
 * Get the current quickslot ID based on mouse coordinates.
 * @param x Mouse X
 * @param y Mouse Y
 * @return Quickslot ID if mouse is over it, -1 if not */
int get_quickslot(int x, int y)
{
	int i, j;
	int qsx, qsy, xoff;

	if (cur_widget[QUICKSLOT_ID].ht > 34)
	{
		qsx = 1;
		qsy = 0;
		xoff = 0;
	}
	else
	{
		qsx = 0;
		qsy = 1;
		xoff = -17;
	}

	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		if (x >= cur_widget[QUICKSLOT_ID].x1 + quickslots_pos[i][qsx] + xoff && x <= cur_widget[QUICKSLOT_ID].x1 + quickslots_pos[i][qsx] + xoff + 32 && y >= cur_widget[QUICKSLOT_ID].y1 + quickslots_pos[i][qsy] && y <= cur_widget[QUICKSLOT_ID].y1 + quickslots_pos[i][qsy] + 32)
			return j;
	}

	return -1;
}

/**
 * Show quickslots widget.
 * @param x X position of the quickslots
 * @param y Y position of the quickslots */
void widget_quickslots(int x, int y)
{
	int i, j, mx, my;
	char buf[16];
	int qsx, qsy, xoff;

	/* Figure out which bitmap to use */
	if (cur_widget[QUICKSLOT_ID].ht > 34)
	{
		qsx = 1;
		qsy = 0;
		xoff = 0;
		sprite_blt(Bitmaps[BITMAP_QUICKSLOTSV], x, y, NULL, NULL);
	}
	else
	{
		qsx = 0;
		qsy = 1;
		xoff = -17;
		sprite_blt(Bitmaps[BITMAP_QUICKSLOTS], x, y, NULL, NULL);
	}

	SDL_GetMouseState(&mx, &my);
	update_quickslots(-1);

	/* Loop through quickslots. Do not loop through all the quickslots,
	 * like MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS. */
	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		/* Now calculate the real quickslot, according to the selected group */
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		/* If it's not empty */
		if (quick_slots[j].tag != -1)
		{
			/* Spell in quickslot */
			if (quick_slots[j].spell)
			{
				/* Output the sprite */
				sprite_blt(spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].icon, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], NULL, NULL);

				/* If mouse is over the quickslot, show a tooltip */
				if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33 && GetMouseState(&mx, &my, QUICKSLOT_ID))
					show_tooltip(mx, my, spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].name);
			}
			/* Item in quickslot */
			else
			{
				item *tmp = locate_item_from_item(cpl.ob, quick_slots[j].tag);

				/* If we located the item */
				if (tmp)
				{
					/* Show it */
					blt_inv_item(tmp, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], 0);

					/* And show tooltip, if mouse is over it */
					if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33 && GetMouseState(&mx, &my, QUICKSLOT_ID))
					{
						show_tooltip(mx, my, tmp->s_name);
					}
				}
			}
		}

		/* For each quickslot, output the F1-F8 shortcut */
		snprintf(buf, sizeof(buf), "F%d", i + 1);
		StringBlt(ScreenSurface, &Font6x3Out, buf, x + quickslots_pos[i][qsx] + xoff + 12, y + quickslots_pos[i][qsy] - 6, COLOR_DEFAULT, NULL, NULL);
	}

	snprintf(buf, sizeof(buf), "Group %d", quickslot_group);

	/* Now output the group */
	if (cur_widget[QUICKSLOT_ID].ht > 34)
		StringBlt(ScreenSurface, &Font6x3Out, buf, x + 3, y + Bitmaps[BITMAP_QUICKSLOTSV]->bitmap->h, COLOR_DEFAULT, NULL, NULL);
	else
		StringBlt(ScreenSurface, &Font6x3Out, buf, x, y + Bitmaps[BITMAP_QUICKSLOTS]->bitmap->h, COLOR_DEFAULT, NULL, NULL);
}

/**
 * Handle mouse events over quickslots.
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param MEvent Mouse event */
void widget_quickslots_mouse_event(int x, int y, int MEvent)
{
	/* Mouseup Event */
	if (MEvent == 1)
	{
		if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
		{
			int ind = get_quickslot(x, y);
			char buf[MAX_BUF];

			/* Valid slot */
			if (ind != -1)
			{
				if (draggingInvItem(DRAG_GET_STATUS) == DRAG_QUICKSLOT_SPELL)
				{
					quick_slots[ind].spell = 1;
					quick_slots[ind].groupNr = quick_slots[cpl.win_quick_tag].groupNr;
					quick_slots[ind].classNr = quick_slots[cpl.win_quick_tag].classNr;
					quick_slots[ind].tag = quick_slots[cpl.win_quick_tag].spellNr;

					/* Tell server to set this quickslot to spell */
					snprintf(buf, sizeof(buf), "qs setspell %d %d %d %d", ind + 1,  quick_slots[ind].groupNr, quick_slots[ind].classNr, quick_slots[ind].tag);
					cs_write_string(csocket.fd, buf, strlen(buf));

					cpl.win_quick_tag = -1;
				}
				else
				{
					if (draggingInvItem(DRAG_GET_STATUS) == DRAG_IWIN_INV)
						cpl.win_quick_tag = cpl.win_inv_tag;
					else if (draggingInvItem(DRAG_GET_STATUS) == DRAG_PDOLL)
						cpl.win_quick_tag = cpl.win_pdoll_tag;

					update_quickslots(cpl.win_quick_tag);

					quick_slots[ind].tag = cpl.win_quick_tag;
					quick_slots[ind].invSlot = ind;
					quick_slots[ind].spell = 0;

					/* Now we do some tests... First, ensure this item can fit */
					update_quickslots(-1);

					/* Now: if this is null, item is *not* in the main inventory
					 * of the player - then we can't put it in quickbar!
					 * Server will not allow apply of items in containers! */
					if (!locate_item_from_inv(cpl.ob->inv, cpl.win_quick_tag))
					{
						sound_play_effect(SOUND_CLICKFAIL, 0, 100);
						draw_info("Only items from main inventory allowed in quickbar!", COLOR_WHITE);
					}
					else
					{
						/* We 'get' it in quickslots */
						sound_play_effect(SOUND_GET, 0, 100);

						/* Send to server to set this item */
						snprintf(buf, sizeof(buf), "qs set %d %d", ind + 1, cpl.win_quick_tag);
						cs_write_string(csocket.fd, buf, strlen(buf));

						snprintf(buf, sizeof(buf), "Set F%d of group %d to %s", ind + 1 - MAX_QUICK_SLOTS * quickslot_group + MAX_QUICK_SLOTS, quickslot_group, locate_item(cpl.win_quick_tag)->s_name);
						draw_info(buf, COLOR_DGOLD);
					}
				}
			}

			draggingInvItem(DRAG_NONE);
			/* ready for next item */
			itemExamined = 0;
		}
	}
	/* Mousedown Event */
	else
	{
		/* Drag from quickslots */
		int ind = get_quickslot(x, y);
		char buf[MAX_BUF];

		/* valid slot */
		if (ind != -1 && quick_slots[ind].tag != -1)
		{
			cpl.win_quick_tag = quick_slots[ind].tag;

			if ((SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
			{
				if (quick_slots[ind].spell)
				{
					draggingInvItem(DRAG_QUICKSLOT_SPELL);
					quick_slots[ind].spellNr = quick_slots[ind].tag;
					cpl.win_quick_tag = ind;
				}
				else
				{
					draggingInvItem(DRAG_QUICKSLOT);
				}

				quick_slots[ind].tag = -1;
			}
			else
			{
				int stemp = cpl.inventory_win, itemp = cpl.win_inv_tag;

				cpl.inventory_win = IWIN_INV;
				cpl.win_inv_tag = quick_slots[ind].tag;
				process_macro_keys(KEYFUNC_APPLY, 0);
				cpl.inventory_win = stemp;
				cpl.win_inv_tag = itemp;
			}

			/* Unset this item */
			snprintf(buf, sizeof(buf), "qs unset %d", ind + 1);
			cs_write_string(csocket.fd, buf, strlen(buf));
		}
		else if (x >= cur_widget[QUICKSLOT_ID].x1 + 266 && x <= cur_widget[QUICKSLOT_ID].x1 + 282 && y >= cur_widget[QUICKSLOT_ID].y1 && y <= cur_widget[QUICKSLOT_ID].y1 + 34 && (cur_widget[QUICKSLOT_ID].ht <= 34))
		{
			cur_widget[QUICKSLOT_ID].wd = 34;
			cur_widget[QUICKSLOT_ID].ht = 282;
			cur_widget[QUICKSLOT_ID].x1 += 266;
		}
		else if (x >= cur_widget[QUICKSLOT_ID].x1 && x <= cur_widget[QUICKSLOT_ID].x1 + 34 && y >= cur_widget[QUICKSLOT_ID].y1 && y <= cur_widget[QUICKSLOT_ID].y1 + 15 && (cur_widget[QUICKSLOT_ID].ht > 34))
		{
			cur_widget[QUICKSLOT_ID].wd = 282;
			cur_widget[QUICKSLOT_ID].ht = 34;
			cur_widget[QUICKSLOT_ID].x1 -= 266;
		}
	}
}

/**
 * Show non-widget quickslots.
 * Used to display quickslots in the spell menu.
 * @param x X position of the quickslots
 * @param y Y position of the quickslots */
void show_quickslots(int x, int y)
{
	int i, mx, my, j;
	char buf[16];
	int qsx, qsy, xoff;

	qsx = 0;
	qsy = 1;
	xoff = -17;
	sprite_blt(Bitmaps[BITMAP_QUICKSLOTS], x, y, NULL, NULL);

	SDL_GetMouseState(&mx, &my);
	update_quickslots(-1);

	/* Loop through the quickslots */
	for (i = 0; i < MAX_QUICK_SLOTS; i++)
	{
		/* Calculate the real quickslot */
		j = MAX_QUICK_SLOTS * quickslot_group - MAX_QUICK_SLOTS + i;

		/* If it's not empty */
		if (quick_slots[j].tag != -1)
		{
			/* Spell in quickslot */
			if (quick_slots[j].spell)
			{
				sprite_blt(spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].icon, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], NULL, NULL);

				/* Show tooltip if mouse is over it */
				if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33)
					show_tooltip(mx, my, spell_list[quick_slots[j].groupNr].entry[quick_slots[j].classNr][quick_slots[j].tag].name);
			}
			/* Item in quickslot */
			else
			{
				item *tmp = locate_item_from_item(cpl.ob, quick_slots[j].tag);

				if (tmp)
				{
					blt_inv_item(tmp, x + quickslots_pos[i][qsx] + xoff, y + quickslots_pos[i][qsy], 0);

					/* Show tooltip if mouse is over it */
					if (mx >= x + quickslots_pos[i][qsx] + xoff && mx < x + quickslots_pos[i][qsx] + xoff + 33 && my >= y + quickslots_pos[i][qsy] && my < y + quickslots_pos[i][qsy] + 33)
						show_tooltip(mx, my, tmp->s_name);
				}
			}
		}

		snprintf(buf, sizeof(buf), "F%d", i + 1);
		StringBlt(ScreenSurface, &Font6x3Out, buf, x + quickslots_pos[i][qsx] + xoff + 12, y + quickslots_pos[i][qsy] - 6, COLOR_DEFAULT, NULL, NULL);
	}

	/* Write out what group we're in */
	snprintf(buf, sizeof(buf), "Group %d", quickslot_group);
	StringBlt(ScreenSurface, &Font6x3Out, buf, x - 30, y + Bitmaps[BITMAP_QUICKSLOTS]->bitmap->h - 12, COLOR_DEFAULT, NULL, NULL);
}

/**
 * Update quickslots. Makes sure no items are where they shouldn't be.
 * @param del_item Item tag to remove from quickslots, -1 to not remove anything */
void update_quickslots(int del_item)
{
	int i;

	for (i = 0; i < MAX_QUICK_SLOTS * MAX_QUICKSLOT_GROUPS; i++)
	{
		if (quick_slots[i].tag == del_item)
			quick_slots[i].tag = -1;

		if (quick_slots[i].tag == -1)
			continue;

		/* Only items in the *main* inventory can be used with quickslots */
		if (quick_slots[i].spell == 0)
		{
			if (!locate_item_from_inv(cpl.ob->inv, quick_slots[i].tag))
				quick_slots[i].tag = -1;

			if (quick_slots[i].tag != -1)
				quick_slots[i].nr = locate_item_nr_from_tag(cpl.ob->inv, quick_slots[i].tag);
		}
	}
}

/**
 * Show the frames per second widget.
 * @param x X position.
 * @param y Y position. */
void widget_show_fps(int x, int y)
{
	char buf[MAX_BUF];

	if (!options.show_frame || !FrameCount)
	{
		return;
	}

	snprintf(buf, sizeof(buf), "fps %d (%d) (%d %d) %s%s%s%s%s%s%s%s%s%s %d %d", ((LastTick - tmpGameTick) / FrameCount) ? 1000 / ((LastTick - tmpGameTick) / FrameCount) : 0, (LastTick - tmpGameTick) / FrameCount, GameStatus, cpl.input_mode, ScreenSurface->flags & SDL_FULLSCREEN ? "F" : "", ScreenSurface->flags & SDL_HWSURFACE ? "H" : "S", ScreenSurface->flags & SDL_HWACCEL ? "A" : "", ScreenSurface->flags & SDL_DOUBLEBUF ? "D" : "", ScreenSurface->flags & SDL_ASYNCBLIT ? "a" : "", ScreenSurface->flags & SDL_ANYFORMAT ? "f" : "", ScreenSurface->flags & SDL_HWPALETTE ? "P" : "", options.rleaccel_flag ? "R" : "", options.force_redraw ? "r" : "", options.use_rect ? "u" : "", options.used_video_bpp, options.real_video_bpp);

	StringBlt(ScreenSurface, &SystemFont, buf, x, y, COLOR_DEFAULT, NULL, NULL);
}

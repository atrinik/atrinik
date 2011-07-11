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
 * Menu related functions. */

#include <global.h>

/** Keybind menu */
int keybind_status;

/**
 * Analyze /cmd type commands the player has typed in the console or bound to a key.
 * Sort out the "client intern" commands and expand or pre process them for the server.
 * @param cmd Command to check
 * @return 0 to send command to server, 1 to not send it */
int client_command_check(const char *cmd)
{
	if (!strncasecmp(cmd, "/ready_spell", 12))
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == '\0')
		{
			draw_info(COLOR_GREEN, "Usage: /ready_spell <spell name>");
		}
		else
		{
			size_t spell_path, spell_id;

			if (spell_find(cmd, &spell_path, &spell_id))
			{
				spell_entry_struct *spell = spell_get(spell_path, spell_id);

				if (spell->known)
				{
					fire_mode_tab[FIRE_MODE_SPELL].spell = spell;
					RangeFireMode = FIRE_MODE_SPELL;
					draw_info_format(COLOR_GREEN, "Readied %s.", spell->name);
					return 1;
				}
				else
				{
					draw_info_format(COLOR_RED, "You have no knowledge of the spell %s.", spell->name);
					return 1;
				}
			}
		}

		draw_info(COLOR_RED, "Unknown spell.");
		return 1;
	}
	else if (!strncasecmp(cmd, "/ready_skill", 12))
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == '\0')
		{
			draw_info(COLOR_RED, "Usage: /ready_skill <skill name>");
		}
		else
		{
			size_t type, id;

			if (skill_find(cmd, &type, &id))
			{
				skill_entry_struct *skill = skill_get(type, id);

				if (skill->known)
				{
					char buf[MAX_BUF];

					fire_mode_tab[FIRE_MODE_SKILL].skill = skill;
					RangeFireMode = FIRE_MODE_SKILL;
					draw_info_format(COLOR_GREEN, "Readied %s.", skill->name);

					snprintf(buf, sizeof(buf), "/ready_skill %s", skill->name);
					send_command(buf);
					return 1;
				}
				else
				{
					draw_info_format(COLOR_RED, "You have no knowledge of the skill %s.", skill->name);
					return 1;
				}
			}
		}

		draw_info(COLOR_RED, "Unknown skill.");
		return 1;
	}
	else if (!strncasecmp(cmd, "/pray", 5))
	{
		/* Give out "You are at full grace." when needed -
		 * server will not send us anything when this happens */
		if (cpl.stats.grace == cpl.stats.maxgrace)
			draw_info(COLOR_WHITE, "You are at full grace. You stop praying.");
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
			draw_info(COLOR_RED, "Usage: /reply <message>");
		}
		else
		{
			if (!cpl.player_reply[0])
			{
				draw_info(COLOR_RED, "There is no one you can /reply.");
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
			draw_info(COLOR_RED, "Usage: /resetwidget <name>");
		}
		else
		{
			reset_widget(cmd);
		}

		return 1;
	}
	else if (!strncmp(cmd, "/effect ", 8))
	{
		if (!strcmp(cmd + 8, "none"))
		{
			effect_stop();
			draw_info(COLOR_GREEN, "Stopped effect.");
			return 1;
		}

		if (effect_start(cmd + 8))
		{
			draw_info_format(COLOR_GREEN, "Started effect %s.", cmd + 8);
		}
		else
		{
			draw_info_format(COLOR_RED, "No such effect %s.", cmd + 8);
		}

		return 1;
	}
	else if (!strncmp(cmd, "/d_effect ", 10))
	{
		effect_debug(cmd + 10);
		return 1;
	}
	else if (!strncmp(cmd, "/np", 3))
	{
		mplayer_now_playing();
		return 1;
	}
	else if (!strncmp(cmd, "/music_pause", 12))
	{
		sound_pause_music();
		return 1;
	}
	else if (!strncmp(cmd, "/music_resume", 13))
	{
		sound_resume_music();
		return 1;
	}
	else if (!strncmp(cmd, "/party joinpassword ", 20))
	{
		cmd += 20;

		if (cpl.partyjoin[0] != '\0')
		{
			char buf[MAX_BUF];

			snprintf(buf, sizeof(buf), "/party join %s\t%s", cpl.partyjoin, cmd ? cmd : " ");
			send_command(buf);
		}

		return 1;
	}

	return 0;
}

/**
 * Same as send_command(), but also check client commands.
 * @param cmd Command to send. */
void send_command_check(const char *cmd)
{
	if (!client_command_check(cmd))
	{
		send_command(cmd);
	}
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
	if (!cpl.menustatus)
		return;

	if (cpl.menustatus == MENU_BOOK)
		book_show();
	else if (cpl.menustatus == MENU_REGION_MAP)
	{
		region_map_show();
	}
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

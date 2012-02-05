/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Crossfire (Multiplayer game for X-windows).                 *
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

/**
 * Analyze /cmd type commands the player has typed in the console or bound to a key.
 * Sort out the "client intern" commands and expand or pre process them for the server.
 * @param cmd Command to check
 * @return 0 to send command to server, 1 to not send it */
int client_command_check(const char *cmd)
{
	if (cmd_aliases_handle(cmd))
	{
		return 1;
	}
	else if (strncasecmp(cmd, "/ready_spell", 12) == 0)
	{
		cmd = strchr(cmd, ' ');

		if (!cmd || *++cmd == '\0')
		{
			draw_info(COLOR_GREEN, "Usage: /ready_spell <spell name>");
			return 1;
		}
		else
		{
			object *tmp;

			for (tmp = cpl.ob->inv; tmp; tmp = tmp->next)
			{
				if (tmp->itype == TYPE_SPELL && strncasecmp(tmp->s_name, cmd, strlen(cmd)) == 0)
				{
					if (!(tmp->flags & CS_FLAG_APPLIED))
					{
						client_send_apply(tmp->tag);
					}

					return 1;
				}
			}
		}

		draw_info(COLOR_RED, "Unknown spell.");
		return 1;
	}
	else if (!strncmp(cmd, "/help", 5))
	{
		cmd += 5;

		if (!cmd || *cmd == '\0')
		{
			help_show("main");
		}
		else
		{
			help_show(cmd + 1);
		}

		return 1;
	}
	else if (!strncmp(cmd, "/ignore", 7))
	{
		ignore_command(cmd + 7);
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
	else if (!strncmp(cmd, "/invfilter ", 11))
	{
		cmd += 11;

		if (!strcmp(cmd, "all"))
		{
			inventory_filter_set(INVENTORY_FILTER_ALL);
		}
		else if (!strcmp(cmd, "applied"))
		{
			inventory_filter_set(INVENTORY_FILTER_APPLIED);
		}
		else if (!strcmp(cmd, "container"))
		{
			inventory_filter_set(INVENTORY_FILTER_CONTAINER);
		}
		else if (!strcmp(cmd, "magical"))
		{
			inventory_filter_set(INVENTORY_FILTER_MAGICAL);
		}
		else if (!strcmp(cmd, "cursed"))
		{
			inventory_filter_set(INVENTORY_FILTER_CURSED);
		}
		else if (!strcmp(cmd, "unidentified"))
		{
			inventory_filter_set(INVENTORY_FILTER_UNIDENTIFIED);
		}
		else if (!strcmp(cmd, "unapplied"))
		{
			inventory_filter_set(INVENTORY_FILTER_UNAPPLIED);
		}
		else if (!strcmp(cmd, "locked"))
		{
			inventory_filter_set(INVENTORY_FILTER_LOCKED);
		}

		return 1;
	}
	else if (!strncasecmp(cmd, "/screenshot", 11))
	{
		SDL_Surface *surface_save;

		cmd += 11;

		if (!strncasecmp(cmd, " map", 4))
		{
			surface_save = cur_widget[MAP_ID]->widgetSF;
		}
		else
		{
			surface_save = ScreenSurface;
		}

		if (!surface_save)
		{
			draw_info(COLOR_RED, "No surface to save.");
			return 1;
		}

		screenshot_create(surface_save);
		return 1;
	}
	else if (!strncasecmp(cmd, "/console-load ", 14))
	{
		FILE *fp;
		char path[HUGE_BUF], buf[HUGE_BUF * 4], *cp;
		StringBuffer *sb;

		cmd += 14;

		snprintf(path, sizeof(path), "%s/.atrinik/console/%s", get_config_dir(), cmd);

		fp = fopen(path, "r");

		if (!fp)
		{
			draw_info_format(COLOR_RED, "Could not read %s.", path);
			return 1;
		}

		send_command("/console noinf::");

		while (fgets(buf, sizeof(buf) - 1, fp))
		{
			cp = strchr(buf, '\n');

			if (cp)
			{
				*cp = '\0';
			}

			sb = stringbuffer_new();
			stringbuffer_append_string(sb, "/console noinf::");
			stringbuffer_append_string(sb, buf);
			cp = stringbuffer_finish(sb);
			send_command(cp);
			free(cp);
		}

		send_command("/console noinf::");

		fclose(fp);

		return 1;
	}
	else if (strncasecmp(cmd, "/console-obj", 11) == 0)
	{
		object *ob;

		ob = widget_inventory_get_selected(cur_widget[cpl.inventory_focus]);

		if (ob)
		{
			char buf[HUGE_BUF];

			snprintf(buf, sizeof(buf), "/console noinf::obj = find_obj(me, count = %d)", ob->tag);
			send_command(buf);
		}

		return 1;
	}
	else if (strncasecmp(cmd, "/cast", 5) == 0)
	{
		char buf[HUGE_BUF];
		size_t spell_path, spell_id;

		cmd += 6;

		if (!cmd || *cmd == '\0')
		{
			return 1;
		}

		if (!spell_find(cmd, &spell_path, &spell_id))
		{
			draw_info(COLOR_RED, "Unknown spell.");
			return 1;
		}

		snprintf(buf, sizeof(buf), "/ready_spell %s", cmd);
		client_command_check(buf);

		cpl.fire_on = 1;
		move_keys(5);
		cpl.fire_on = 0;

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

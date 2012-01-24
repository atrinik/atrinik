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
 *  */

#include <global.h>

/**
 * Mouse event for number input widget.
 * @param x Mouse X position
 * @param y Mouse Y position */
void widget_number_event(widgetdata *widget, int x, int y)
{
	int mx = 0, my = 0;
	mx = x - widget->x1;
	my = y - widget->y1;

	/* Close number input */
	if (text_input_string_flag && cpl.input_mode == INPUT_MODE_NUMBER)
	{
		if (mx > 239 && mx < 249 && my > 5 && my < 17)
		{
			SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
			text_input_string_flag = 0;
			text_input_string_end_flag = 1;
		}
	}
}

/**
 * Show widget console.
 * @param x X position of the console
 * @param y Y position of the console */
void widget_show_console(widgetdata *widget)
{
	SDL_Rect box;

	box.x = 3;
	box.y = 0;
	box.w = Bitmaps[BITMAP_TEXTINPUT]->bitmap->w;
	box.h = Bitmaps[BITMAP_TEXTINPUT]->bitmap->h;
	text_input_show(ScreenSurface, widget->x1, widget->y1, FONT_ARIAL11, text_input_string, COLOR_WHITE, 0, BITMAP_TEXTINPUT, &box);
}

/**
 * Show widget number input.
 * @param x X position of the number input
 * @param y Y position of the number input */
void widget_show_number(widgetdata *widget)
{
	SDL_Rect box;
	char buf[MAX_BUF];

	box.x = 3;
	box.y = 9;
	box.w = Bitmaps[BITMAP_NUMBER]->bitmap->w - 22;
	box.h = Bitmaps[BITMAP_NUMBER]->bitmap->h;
	text_input_show(ScreenSurface, widget->x1, widget->y1, FONT_ARIAL11, text_input_string, COLOR_WHITE, 0, BITMAP_NUMBER, &box);

	snprintf(buf, sizeof(buf), "%s how many from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", cpl.nrof, cpl.num_text);
	string_truncate_overflow(FONT_ARIAL10, buf, 220);
	string_blt(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 8, widget->y1 + 6, COLOR_HGOLD, 0, NULL);
}

void widget_input_do(widgetdata *widget)
{
	/* ESC was pressed. */
	if (text_input_string_esc_flag)
	{
		widget->show = 0;
		text_input_close();
		return;
	}

	/* Handle ?DROP and ?GET key repeating for number input. */
	if (cpl.input_mode == INPUT_MODE_NUMBER)
	{
		keybind_struct *keybind = NULL;

		if (cpl.nummode == NUM_MODE_GET)
		{
			keybind = keybind_find_by_command("?GET");
		}
		else if (cpl.nummode == NUM_MODE_DROP)
		{
			keybind = keybind_find_by_command("?DROP");
		}

		/* If the macro key is active and enough time has passed, end the
		 * input string. */
		if (keybind && keys[keybind->key].pressed && SDL_GetTicks() - text_input_opened > 125)
		{
			text_input_string_flag = 0;
			text_input_string_end_flag = 1;
			keys[keybind->key].time = SDL_GetTicks() + 125;
		}
	}

	/* Is there a finished input? */
	if (text_input_string_flag == 0 && text_input_string_end_flag)
	{
		/* Any input? */
		if (text_input_string[0] != '\0')
		{
			/* Handle number input. */
			if (cpl.input_mode == INPUT_MODE_NUMBER)
			{
				int tmp;

				tmp = atoi(text_input_string);

				/* If you enter a number higher than the real nrof, you
				 * will pickup all. */
				if (tmp > cpl.nrof)
				{
					tmp = cpl.nrof;
				}

				if (tmp > 0 && tmp <= cpl.nrof)
				{
					client_send_move(cpl.loc, cpl.tag, tmp);
					draw_info_format(COLOR_DGOLD, "%s %d from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", tmp, cpl.nrof, cpl.num_text);

					if (cpl.nummode == NUM_MODE_GET)
					{
						sound_play_effect("get.ogg", 100);
					}
					else
					{
						sound_play_effect("drop.ogg", 100);
					}
				}
			}
			/* Handle console input. */
			else if (cpl.input_mode == INPUT_MODE_CONSOLE)
			{
				char buf[MAX_INPUT_STRING + 32];

				/* If it's not command, it's say */
				if (*text_input_string != '/')
				{
					snprintf(buf, sizeof(buf), "/say %s", text_input_string);
				}
				else
				{
					strcpy(buf, text_input_string);
				}

				if (!client_command_check(text_input_string))
				{
					send_command(buf);
				}
			}
		}

		widget->show = 0;
		text_input_close();
	}
	else
	{
		widget->show = 1;
	}
}

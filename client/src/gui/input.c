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
	text_input_show(ScreenSurface, widget->x1, widget->y1, FONT_ARIAL11, text_input_string, COLOR_SIMPLE(COLOR_WHITE), 0, BITMAP_TEXTINPUT, &box);
}

/**
 * Show widget number input.
 * @param x X position of the number input
 * @param y Y position of the number input */
void widget_show_number(widgetdata *widget)
{
	SDL_Rect tmp;
	char buf[512];

	tmp.w = 238;

	sprite_blt(Bitmaps[BITMAP_NUMBER], widget->x1, widget->y1, NULL, NULL);
	snprintf(buf, sizeof(buf), "%s how many from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", cpl.nrof, cpl.num_text);

	StringBlt(ScreenSurface, &SystemFont, buf, widget->x1 + 8, widget->y1 + 6, COLOR_HGOLD, &tmp, NULL);
	StringBlt(ScreenSurface, &SystemFont, show_input_string(text_input_string, &SystemFont, Bitmaps[BITMAP_NUMBER]->bitmap->w - 22), widget->x1 + 8, widget->y1 + 25, COLOR_WHITE, &tmp, NULL);
}

/**
 * Wait for number input.
 * If ESC was pressed, close the input widget. */
void do_number()
{
	int held = 0;

	map_udate_flag = 2;

	if (text_input_string_esc_flag)
	{
		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_NUMBER_ID]->show = 0;
	}

	if (((cpl.nummode == NUM_MODE_GET && key_is_pressed(get_action_keycode)) || (cpl.nummode == NUM_MODE_DROP && key_is_pressed(drop_action_keycode))) && SDL_GetTicks() - text_input_opened > 125)
	{
		SDL_EnableKeyRepeat(0, SDL_DEFAULT_REPEAT_INTERVAL);
		text_input_string_flag = 0;
		text_input_string_end_flag = 1;
		held = 1;
	}

	/* if set, we got a finished input!*/
	if (text_input_string_flag == 0 && text_input_string_end_flag)
	{
		if (text_input_string[0])
		{
			int tmp;
			char buf[300];
			tmp = atoi(text_input_string);

			/* If you enter a number higher than the real nrof, you will pickup all */
			if (tmp > cpl.nrof)
				tmp = cpl.nrof;

			if (tmp > 0 && tmp <= cpl.nrof)
			{
				client_send_move(cpl.loc, cpl.tag, tmp);
				snprintf(buf, sizeof(buf), "%s %d from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", tmp, cpl.nrof, cpl.num_text);

				if (cpl.nummode == NUM_MODE_GET)
					sound_play_effect("get.ogg", 100);
				else
					sound_play_effect("drop.ogg", 100);

				draw_info(COLOR_DGOLD, buf);
			}
		}

		reset_keys();

		if (held)
		{
			SDLKey key = cpl.nummode == NUM_MODE_GET ? get_action_keycode : drop_action_keycode;

			keys[key].pressed = 1;
			keys[key].time = LastTick + 125;
		}

		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_NUMBER_ID]->show = 0;
	}
	else
		cur_widget[IN_NUMBER_ID]->show = 1;
}

/**
 * Wait for input in the keybind menu.
 * If ESC was pressed, close the input. */
void do_keybind_input()
{
	if (text_input_string_esc_flag)
	{
		reset_keys();
		sound_play_effect("click_fail.ogg", 100);
		cpl.input_mode = INPUT_MODE_NO;
		keybind_status = KEYBIND_STATUS_NO;
		map_udate_flag = 2;
	}

	/* If set, we got a finished input */
	if (text_input_string_flag == 0 && text_input_string_end_flag)
	{
		if (text_input_string[0])
		{
			strcpy(bindkey_list[bindkey_list_set.group_nr].entry[bindkey_list_set.entry_nr].text, text_input_string);
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

/**
 * Wait for console input.
 * If ESC was pressed or this is input for party menu, just close the console. */
void do_console()
{
	map_udate_flag = 2;

	/* If ESC was pressed or console_party() returned 1, close console. */
	if (text_input_string_esc_flag || console_party())
	{
		if (gui_interface_party)
			clear_party_interface();

		sound_play_effect("console.ogg", 100);
		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_CONSOLE_ID]->show = 0;
	}

	/* If set, we've got a finished input */
	if (text_input_string_flag == 0 && text_input_string_end_flag)
	{
		sound_play_effect("console.ogg", 100);

		if (text_input_string[0])
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
				send_command(buf);
		}

		reset_keys();
		cpl.input_mode = INPUT_MODE_NO;
		cur_widget[IN_CONSOLE_ID]->show = 0;
	}
	else
		cur_widget[IN_CONSOLE_ID]->show = 1;
}

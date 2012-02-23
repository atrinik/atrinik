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

static void widget_input_handle_enter(widgetdata *widget)
{
	text_input_struct *text_input;

	widget->show = 0;
	text_input = &WIDGET_INPUT(widget)->text_input;

	if (*text_input->str != '\0')
	{
		if (widget->WidgetTypeID == IN_NUMBER_ID)
		{
			int tmp;

			tmp = atoi(text_input->str);

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
		else if (widget->WidgetTypeID == IN_CONSOLE_ID)
		{
			char buf[sizeof(text_input->str) + 32];

			/* If it's not command, it's say */
			if (*text_input->str != '/')
			{
				snprintf(buf, sizeof(buf), "/say %s", text_input->str);
			}
			else
			{
				strcpy(buf, text_input->str);
			}

			send_command_check(buf);
		}
	}
}

/**
 * Show widget console.
 * @param x X position of the console
 * @param y Y position of the console */
void widget_show_console(widgetdata *widget)
{
	text_input_struct *text_input;

	text_input = &WIDGET_INPUT(widget)->text_input;
	text_input_show(text_input, ScreenSurface, widget->x1, widget->y1);
}

/**
 * Show widget number input.
 * @param x X position of the number input
 * @param y Y position of the number input */
void widget_show_number(widgetdata *widget)
{
	text_input_struct *text_input;
	keybind_struct *keybind;
	char buf[MAX_BUF];

	text_input = &WIDGET_INPUT(widget)->text_input;
	keybind = NULL;
	text_input_show(text_input, ScreenSurface, widget->x1, widget->y1);

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
	if (keybind && keys[keybind->key].pressed && SDL_GetTicks() - widget->showed_ticks > 125)
	{
		widget_input_handle_enter(widget);
		keys[keybind->key].time = SDL_GetTicks() + 125;
	}

	snprintf(buf, sizeof(buf), "%s how many from %d %s", cpl.nummode == NUM_MODE_GET ? "get" : "drop", cpl.nrof, cpl.num_text);
	string_truncate_overflow(FONT_ARIAL10, buf, 220);
	string_show(ScreenSurface, FONT_ARIAL10, buf, widget->x1 + 8, widget->y1 + 6, COLOR_HGOLD, 0, NULL);
}

int widget_input_handle_key(widgetdata *widget, SDL_Event *event)
{
	text_input_struct *text_input;

	if (widget->show == 0 || event->type != SDL_KEYDOWN)
	{
		return 0;
	}

	text_input = &WIDGET_INPUT(widget)->text_input;

	if (event->key.keysym.sym == SDLK_ESCAPE)
	{
		widget->show = 0;
	}
	else if (IS_ENTER(event->key.keysym.sym))
	{
		widget_input_handle_enter(widget);
	}
	else if (event->key.keysym.sym == SDLK_TAB && widget->WidgetTypeID == IN_CONSOLE_ID)
	{
		help_handle_tabulator(text_input);
	}
	else
	{
		text_input_event(text_input, event);
	}

	return 1;
}

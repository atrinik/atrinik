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
 * Implements menu buttons code. */

#include <include.h>

/** The different buttons inside the widget. */
enum
{
	/** Spells. */
	BUTTON_SPELLS,
	/** Skills. */
	BUTTON_SKILLS,
	/** Party. */
	BUTTON_PARTY,
	/** Music player. */
	BUTTON_MPLAYER,
	/** Region map. */
	BUTTON_MAP,
	/** Quest list. */
	BUTTON_QUEST,
	/** Help. */
	BUTTON_HELP,
	/** Esc menu. */
	BUTTON_SETTINGS,

	/** Total number of the buttons. */
	NUM_BUTTONS
};

/** Button buffers. */
static button_struct buttons[NUM_BUTTONS];
/** Images to render on top of the buttons, -1 for none. */
static int button_images[NUM_BUTTONS] =
{
	BITMAP_ICON_MAGIC, BITMAP_ICON_SKILL, BITMAP_ICON_PARTY, BITMAP_ICON_MUSIC, BITMAP_ICON_MAP, BITMAP_ICON_QUEST, -1, BITMAP_ICON_COGS
};
/** Whether the ::buttons have been initialized. */
static uint8 did_init = 0;

/**
 * Show menu buttons widget.
 * @param widget The widget. */
void widget_menubuttons(widgetdata *widget)
{
	size_t i;
	const char *text;
	int x, y;

	/* Initialize buttons. */
	if (!did_init)
	{
		did_init = 1;

		for (i = 0; i < NUM_BUTTONS; i++)
		{
			button_create(&buttons[i]);
			buttons[i].bitmap = BITMAP_BUTTON_RECT;
			buttons[i].bitmap_pressed = BITMAP_BUTTON_RECT_DOWN;
		}

		buttons[BUTTON_HELP].flags |= TEXT_MARKUP;
		buttons[BUTTON_HELP].font = FONT_SANS16;
	}

	sprite_blt(Bitmaps[BITMAP_MENU_BUTTONS], widget->x1, widget->y1, NULL, NULL);

	x = 4;
	y = 3;

	/* Render the buttons. */
	for (i = 0; i < NUM_BUTTONS; i++)
	{
		if (i && !(i % 2))
		{
			x = 4;
			y += Bitmaps[buttons[i].bitmap]->bitmap->h + 1;
		}

		text = NULL;

		if (i == BUTTON_HELP)
		{
			text = "<y=2>?";
		}
		else if (i == BUTTON_MPLAYER)
		{
			buttons[i].pressed = cur_widget[MPLAYER_ID]->show;
		}

		buttons[i].x = widget->x1 + x;
		buttons[i].y = widget->y1 + y;
		button_render(&buttons[i], text);

		if (button_images[i] != -1)
		{
			sprite_blt(Bitmaps[button_images[i]], widget->x1 + x, widget->y1 + y, NULL, NULL);
		}

		x += Bitmaps[buttons[i].bitmap]->bitmap->w + 3;
	}
}

/**
 * Handle mouse events over the menu buttons widget.
 *
 * Basically calls the right functions depending on which button was
 * clicked.
 * @param widget The widget object.
 * @param event The event to handle. */
void widget_menubuttons_event(widgetdata *widget, SDL_Event *event)
{
	size_t i;

	(void) widget;

	for (i = 0; i < NUM_BUTTONS; i++)
	{
		if (button_event(&buttons[i], event))
		{
			if (i == BUTTON_SPELLS)
			{
				check_menu_macros("?M_SPELL_LIST");
			}
			else if (i == BUTTON_SKILLS)
			{
				check_menu_macros("?M_SKILL_LIST");
			}
			else if (i == BUTTON_PARTY)
			{
				send_command("/party list");
			}
			else if (i == BUTTON_MPLAYER)
			{
				cur_widget[MPLAYER_ID]->show = !cur_widget[MPLAYER_ID]->show;
				SetPriorityWidget(cur_widget[MPLAYER_ID]);
			}
			else if (i == BUTTON_MAP)
			{
				send_command("/region_map");
			}
			else if (i == BUTTON_QUEST)
			{
				process_macro_keys(KEYFUNC_QLIST, 0);
			}
			else if (i == BUTTON_HELP)
			{
				show_help("main");
			}
			else if (i == BUTTON_SETTINGS)
			{
				map_udate_flag = 1;
				esc_menu_flag = 1;
				esc_menu_index = ESC_MENU_BACK;
			}

			break;
		}
	}
}

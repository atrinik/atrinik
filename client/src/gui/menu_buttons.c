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
 * Implements menu buttons code.
 *
 * @author Alex Tokar */

#include <global.h>

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
static const char *button_images[NUM_BUTTONS] =
{
	"magic", "skill", "party", "music", "map", "quest", NULL, "cogs"
};
/** Tooltip texts for the buttons. */
static const char *const button_tooltips[NUM_BUTTONS] =
{
	"Spells", "Skills", "Party", "Music player", "Region map", "Quest list", "Help", "Settings"
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
			buttons[i].texture = texture_get(TEXTURE_TYPE_CLIENT, "button_rect");
			buttons[i].texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_rect_over");
			buttons[i].texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_rect_down");
		}

		buttons[BUTTON_HELP].flags |= TEXT_MARKUP;
		buttons[BUTTON_HELP].font = FONT_SANS16;
	}

	surface_show(ScreenSurface, widget->x1, widget->y1, NULL, TEXTURE_CLIENT("menu_buttons"));

	x = 4;
	y = 3;

	/* Render the buttons. */
	for (i = 0; i < NUM_BUTTONS; i++)
	{
		if (i && !(i % 2))
		{
			x = 4;
			y += TEXTURE_SURFACE(buttons[i].texture)->h + 1;
		}

		text = NULL;

		if (i == BUTTON_HELP)
		{
			text = "<y=2>?";
		}
		else if (i == BUTTON_SPELLS)
		{
			buttons[i].pressed_forced = cur_widget[SPELLS_ID]->show;
		}
		else if (i == BUTTON_MPLAYER)
		{
			buttons[i].pressed_forced = cur_widget[MPLAYER_ID]->show;
		}
		else if (i == BUTTON_SKILLS)
		{
			buttons[i].pressed_forced = cur_widget[SKILLS_ID]->show;
		}
		else if (i == BUTTON_PARTY)
		{
			buttons[i].pressed_forced = cur_widget[PARTY_ID]->show;
		}

		buttons[i].x = widget->x1 + x;
		buttons[i].y = widget->y1 + y;
		button_show(&buttons[i], text);
		button_tooltip(&buttons[i], FONT_ARIAL10, button_tooltips[i]);

		if (button_images[i])
		{
			char buf[MAX_BUF];

			snprintf(buf, sizeof(buf), "icon_%s", button_images[i]);
			surface_show(ScreenSurface, widget->x1 + x, widget->y1 + y, NULL, TEXTURE_CLIENT(buf));
		}

		x += TEXTURE_SURFACE(buttons[i].texture)->w + 3;
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
				cur_widget[SPELLS_ID]->show = !cur_widget[SPELLS_ID]->show;
				SetPriorityWidget(cur_widget[SPELLS_ID]);
			}
			else if (i == BUTTON_SKILLS)
			{
				cur_widget[SKILLS_ID]->show = !cur_widget[SKILLS_ID]->show;
				SetPriorityWidget(cur_widget[SKILLS_ID]);
			}
			else if (i == BUTTON_PARTY)
			{
				if (cur_widget[PARTY_ID]->show)
				{
					cur_widget[PARTY_ID]->show = 0;
				}
				else
				{
					send_command_check("/party list");
				}
			}
			else if (i == BUTTON_MPLAYER)
			{
				cur_widget[MPLAYER_ID]->show = !cur_widget[MPLAYER_ID]->show;
				SetPriorityWidget(cur_widget[MPLAYER_ID]);
			}
			else if (i == BUTTON_SETTINGS)
			{
				settings_open();
			}
			else if (i == BUTTON_MAP)
			{
				send_command("/region_map");
			}
			else if (i == BUTTON_QUEST)
			{
				keybind_process_command("?QLIST");
			}
			else if (i == BUTTON_HELP)
			{
				help_show("main");
			}

			break;
		}
	}
}

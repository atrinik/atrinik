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
 * Implements the characters chooser.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * Progress dots buffer. */
static progress_dots progress;
/**
 * Characters list. */
static list_struct *list_characters;

/** @copydoc list_struct::post_column_func */
static void list_post_column(list_struct *list, uint32 row, uint32 col)
{
}

/** @copydoc list_struct::row_color_func */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
	SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 25, 25, 25));
}

/** @copydoc popup_struct::popup_draw_func */
static int popup_draw(popup_struct *popup)
{
	SDL_Rect box;

	/* Waiting to log in. */
	if (cpl.state == ST_WAITFORPLAY)
	{
		box.w = popup->surface->w;
		box.h = popup->surface->h;
		string_show_shadow(popup->surface, FONT_SERIF12, "Logging in, please wait...", 0, 0, COLOR_HGOLD, COLOR_BLACK, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER, &box);
		return 1;
	}
	else if (cpl.state == ST_PLAY || cpl.state < ST_STARTCONNECT)
	{
		return 0;
	}

	return 1;
}

/** @copydoc popup_struct::popup_event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
	return -1;
}

/** @copydoc popup_struct::destroy_callback_func */
static int popup_destroy_callback(popup_struct *popup)
{
	list_remove(list_characters);

	if (cpl.state != ST_PLAY)
	{
		cpl.state = ST_START;
	}

	return 1;
}

/**
 * Open the characters chooser popup. */
void characters_open(void)
{
	popup_struct *popup;

	progress_dots_create(&progress);

	popup = popup_create("popup");
	popup->draw_func = popup_draw;
	popup->event_func = popup_event;
	popup->destroy_callback_func = popup_destroy_callback;

	list_characters = list_create(5, 2, 8);
	list_characters->post_column_func = list_post_column;
	list_characters->row_color_func = list_row_color;
	list_characters->row_selected_func = NULL;
	list_characters->row_highlight_func = NULL;
	list_characters->surface = popup->surface;
	list_characters->row_height_adjust = 70;
	list_characters->text_flags = TEXT_MARKUP;
	list_set_font(list_characters, -1);
	list_set_column(list_characters, 0, 50, 0, NULL, -1);
	list_set_column(list_characters, 1, 100, 0, NULL, -1);
	list_scrollbar_enable(list_characters);

	cpl.state = ST_CHARACTERS;
}

static int archname_to_character(const char *archname, size_t *race, size_t *gender)
{
	for (*race = 0; *race < s_settings->num_characters; (*race)++)
	{
		for (*gender = 0; *gender < GENDER_MAX; (*gender)++)
		{
			if (s_settings->characters[*race].gender_archetypes[*gender] && strcmp(s_settings->characters[*race].gender_archetypes[*gender], archname) == 0)
			{
				return 1;
			}
		}
	}

	return 0;
}

void socket_command_characters(uint8 *data, size_t len, size_t pos)
{
	char archname[MAX_BUF], name[MAX_BUF];
	uint8 level;
	size_t race, gender;

	list_clear(list_characters);

	packet_to_string(data, len, &pos, cpl.account, sizeof(cpl.account));
	packet_to_string(data, len, &pos, cpl.host, sizeof(cpl.host));
	packet_to_string(data, len, &pos, cpl.last_host, sizeof(cpl.last_host));
	packet_to_string(data, len, &pos, cpl.last_time, sizeof(cpl.last_time));

	while (pos < len)
	{
		packet_to_string(data, len, &pos, archname, sizeof(archname));
		packet_to_string(data, len, &pos, name, sizeof(name));
		level = packet_to_uint8(data, len, &pos);

		if (archname_to_character(archname, &race, &gender))
		{
			char buf[MAX_BUF];

			snprintf(buf, sizeof(buf), "<img=%s> %d", s_settings->characters[race].gender_faces[gender], level);
			list_add(list_characters, list_characters->rows, 0, buf);
		}
	}
}

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
 * Handles the party widget. */

#include <include.h>

#define STAT_BAR_WIDTH 60

static void list_handle_enter(list_struct *list)
{
	(void) list;
}

static void list_row_highlight(list_struct *list, SDL_Rect box)
{
	box.w -= STAT_BAR_WIDTH + list->col_spacings[0];
	SDL_FillRect(list->surface, &box, sdl_dgreen);
}

static void list_row_selected(list_struct *list, SDL_Rect box)
{
	box.w -= STAT_BAR_WIDTH + list->col_spacings[0];
	SDL_FillRect(list->surface, &box, sdl_blue1);
}

void widget_party_render(widgetdata *widget)
{
	list_struct *list;
	SDL_Rect box, dst;

	/* Create the surface. */
	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_CONTENT]->bitmap, Bitmaps[BITMAP_CONTENT]->bitmap->format, Bitmaps[BITMAP_CONTENT]->bitmap->flags);
	}

	list = list_exists(LIST_PARTY);

	/* Create the skill list. */
	if (!list)
	{
		list = list_create(LIST_PARTY, 10, 2, 12, 2, 8);
		list->handle_enter_func = list_handle_enter;
		list->surface = widget->widgetSF;
		list->text_flags = TEXT_MARKUP;
		list->row_highlight_func = list_row_highlight;
		list->row_selected_func = list_row_selected;
		list_scrollbar_enable(list);
		list_set_column(list, 0, 130, 7, NULL, -1);
		list_set_column(list, 1, 60, 7, NULL, -1);
	}

	if (widget->redraw)
	{
		_BLTFX bltfx;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[BITMAP_CONTENT], 0, 0, NULL, &bltfx);

		widget->redraw = 0;

		box.h = 0;
		box.w = widget->wd;
		string_blt(widget->widgetSF, FONT_SERIF12, "Party", 0, 3, COLOR_SIMPLE(COLOR_HGOLD), TEXT_ALIGN_CENTER, &box);
		list->focus = 1;
		list_show(list);
	}

	dst.x = widget->x1;
	dst.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &dst);
}

void widget_party_mevent(widgetdata *widget, SDL_Event *event)
{
	list_struct *list = list_exists(LIST_PARTY);

	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list && list_handle_mouse(list, event->motion.x - widget->x1, event->motion.y - widget->y1, event))
	{
		widget->redraw = 1;
	}
}

/**
 * Party command.
 * @param data Data.
 * @param len Length of the data. */
void PartyCmd(unsigned char *data, int len)
{
	uint8 type;
	int pos;
	list_struct *list;

	pos = 0;
	type = data[pos++];
	list = list_exists(LIST_PARTY);

	if (type == CMD_PARTY_LIST)
	{
		char party_name[MAX_BUF], party_leader[MAX_BUF];

		list_clear(list);

		while (pos < len)
		{
			GetString_String(data, &pos, party_name, sizeof(party_name));
			GetString_String(data, &pos, party_leader, sizeof(party_leader));
			list_add(list, list->rows, 0, party_name);
			list_add(list, list->rows - 1, 1, party_leader);
		}

		cur_widget[PARTY_ID]->redraw = 1;
		cur_widget[PARTY_ID]->show = 1;
		SetPriorityWidget(cur_widget[SKILLS_ID]);
	}
	else if (type == CMD_PARTY_WHO)
	{
		char name[MAX_BUF], bars[MAX_BUF];
		uint8 hp, sp, grace;

		list_clear(list);

		while (pos < len)
		{
			GetString_String(data, &pos, name, sizeof(name));
			hp = data[pos++];
			sp = data[pos++];
			grace = data[pos++];
			list_add(list, list->rows, 0, name);
			snprintf(bars, sizeof(bars), "<x=5><bar=#cb0202 %d 4><y=4><bar=#1818a4 %d 4><y=4><bar=#15bc15 %d 4>", (int) (STAT_BAR_WIDTH * (hp / 100.0)), (int) (STAT_BAR_WIDTH * (sp / 100.0)), (int) (STAT_BAR_WIDTH * (grace / 100.0)));
			list_add(list, list->rows - 1, 1, bars);
		}

		cur_widget[PARTY_ID]->redraw = 1;
		cur_widget[PARTY_ID]->show = 1;
		SetPriorityWidget(cur_widget[SKILLS_ID]);
	}
}

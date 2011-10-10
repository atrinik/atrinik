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

#include <global.h>

/** Width of the hp/sp/grace stat bar. */
#define STAT_BAR_WIDTH 60

/** Macro to create the stat bar markup. */
#define PARTY_STAT_BAR() \
	snprintf(bars, sizeof(bars), "<x=5><bar=#000000 %d 4><bar=#cb0202 %d 4><y=4><bar=#000000 %d 4><bar=#1818a4 %d 4><y=4><bar=#000000 %d 4><bar=#15bc15 %d 4>", STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (hp / 100.0)), STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (sp / 100.0)), STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (grace / 100.0)));

/** Button buffer. */
static button_struct button_close, button_help, button_parties, button_members, button_form, button_leave, button_password, button_chat;

/** The party list. */
static list_struct *list_party = NULL;

/**
 * What type of data is currently in the list; -1 means no data,
 * otherwise one of @ref CMD_PARTY_xxx. */
static sint8 list_contents = -1;

/**
 * Handle enter/double click for the party list.
 * @param list List. */
static void list_handle_enter(list_struct *list)
{
	if (list_contents == CMD_PARTY_LIST && list->text)
	{
		char buf[MAX_BUF];

		snprintf(buf, sizeof(buf), "/party join %s", list->text[list->row_selected - 1][0]);
		send_command(buf);
	}
}

/**
 * Highlight a row in the party list.
 * @param list List.
 * @param box Dimensions for the row. */
static void list_row_highlight(list_struct *list, SDL_Rect box)
{
	if (list_contents == CMD_PARTY_WHO)
	{
		box.w -= STAT_BAR_WIDTH + list->col_spacings[0];
	}

	SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 0x00, 0x80, 0x00));
}

/**
 * Highlight selected row in the party list.
 * @param list List.
 * @param box Dimensions for the row. */
static void list_row_selected(list_struct *list, SDL_Rect box)
{
	if (list_contents == CMD_PARTY_WHO)
	{
		box.w -= STAT_BAR_WIDTH + list->col_spacings[0];
	}

	SDL_FillRect(list->surface, &box, SDL_MapRGB(list->surface->format, 0x00, 0x00, 0xef));
}

/**
 * Process the party widget in the background (even if it's hidden).
 *
 * This is mostly so the surface and the list are always created, in the
 * case that the widget is hidden, but player uses "/party list" or
 * similar.
 * @param widget The widget. */
void widget_party_background(widgetdata *widget)
{
	/* Create the surface. */
	if (!widget->widgetSF)
	{
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_CONTENT]->bitmap, Bitmaps[BITMAP_CONTENT]->bitmap->format, Bitmaps[BITMAP_CONTENT]->bitmap->flags);
	}

	/* Create the party list. */
	if (!list_party)
	{
		list_party = list_create(12, 2, 8);
		list_party->handle_enter_func = list_handle_enter;
		list_party->surface = widget->widgetSF;
		list_party->text_flags = TEXT_MARKUP;
		list_party->row_highlight_func = list_row_highlight;
		list_party->row_selected_func = list_row_selected;
		list_scrollbar_enable(list_party);
		list_set_column(list_party, 0, 130, 7, NULL, -1);
		list_set_column(list_party, 1, 60, 7, NULL, -1);
		list_party->header_height = 6;

		/* Create various buttons... */
		button_create(&button_close);
		button_create(&button_help);
		button_create(&button_parties);
		button_create(&button_members);
		button_create(&button_form);
		button_create(&button_leave);
		button_create(&button_password);
		button_create(&button_chat);
		button_close.bitmap = button_help.bitmap = BITMAP_BUTTON_ROUND;
		button_close.bitmap_pressed = button_help.bitmap_pressed = BITMAP_BUTTON_ROUND_DOWN;
		button_close.bitmap_over = button_help.bitmap_over = BITMAP_BUTTON_ROUND_HOVER;

		button_parties.flags = button_members.flags = TEXT_MARKUP;
		widget->redraw = 1;
		list_contents = -1;
	}
}

/**
 * Render the party widget.
 * @param widget The widget. */
void widget_party_render(widgetdata *widget)
{
	SDL_Rect box, dst;

	if (widget->redraw)
	{
		_BLTFX bltfx;

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[BITMAP_CONTENT], 0, 0, NULL, &bltfx);

		box.h = 0;
		box.w = widget->wd;
		string_blt(widget->widgetSF, FONT_SERIF12, "Party", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		if (list_party)
		{
			list_set_parent(list_party, widget->x1, widget->y1);
			list_show(list_party, 10, 23);
		}

		widget->redraw = list_need_redraw(list_party);
	}

	dst.x = widget->x1;
	dst.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &dst);

	/* Render the various buttons. */
	button_close.x = widget->x1 + widget->wd - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w - 4;
	button_close.y = widget->y1 + 4;
	button_render(&button_close, "X");

	button_help.x = widget->x1 + widget->wd - Bitmaps[BITMAP_BUTTON_ROUND]->bitmap->w * 2 - 4;
	button_help.y = widget->y1 + 4;
	button_render(&button_help, "?");

	button_parties.x = widget->x1 + 244;
	button_parties.y = widget->y1 + 38;
	button_render(&button_parties, list_contents == CMD_PARTY_LIST ? "<u>Parties</u>" : "Parties");

	button_members.x = button_form.x = widget->x1 + 244;
	button_members.y = button_form.y = widget->y1 + 60;

	if (cpl.partyname[0] == '\0')
	{
		button_render(&button_form, "Form");
	}
	else
	{
		button_render(&button_members, list_contents == CMD_PARTY_WHO ? "<u>Members</u>" : "Members");
		button_leave.x = button_password.x = button_chat.x = widget->x1 + 244;
		button_leave.y = widget->y1 + 82;
		button_password.y = widget->y1 + 104;
		button_chat.y = widget->y1 + 126;
		button_render(&button_leave, "Leave");
		button_render(&button_password, "Password");
		button_render(&button_chat, "Chat");
	}
}

/**
 * Handle mouse events inside the party widget.
 * @param widget The widget.
 * @param event Event to handle. */
void widget_party_mevent(widgetdata *widget, SDL_Event *event)
{
	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_party && list_handle_mouse(list_party, event))
	{
		widget->redraw = 1;
	}
	else if (button_event(&button_close, event))
	{
		widget->show = 0;
	}
	else if (button_event(&button_help, event))
	{
		help_show("party list");
	}
	else if (button_event(&button_parties, event))
	{
		send_command("/party list");
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_members, event))
	{
		send_command("/party who");
	}
	else if (cpl.partyname[0] == '\0' && button_event(&button_form, event))
	{
		cpl.input_mode = INPUT_MODE_CONSOLE;
		text_input_open(253);
		text_input_set_string("/party form ");
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_password, event))
	{
		cpl.input_mode = INPUT_MODE_CONSOLE;
		text_input_open(253);
		text_input_set_string("/party password ");
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_leave, event))
	{
		send_command("/party leave");
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_chat, event))
	{
		cpl.input_mode = INPUT_MODE_CONSOLE;
		text_input_open(253);
		text_input_set_string("/gsay ");
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

	pos = 0;
	type = data[pos++];

	/* List of parties, or list of party members. */
	if (type == CMD_PARTY_LIST || type == CMD_PARTY_WHO)
	{
		list_clear(list_party);

		while (pos < len)
		{
			if (type == CMD_PARTY_LIST)
			{
				char party_name[MAX_BUF], party_leader[MAX_BUF];

				GetString_String(data, &pos, party_name, sizeof(party_name));
				GetString_String(data, &pos, party_leader, sizeof(party_leader));
				list_add(list_party, list_party->rows, 0, party_name);
				list_add(list_party, list_party->rows - 1, 1, party_leader);
			}
			else if (type == CMD_PARTY_WHO)
			{
				char name[MAX_BUF], bars[MAX_BUF];
				uint8 hp, sp, grace;

				GetString_String(data, &pos, name, sizeof(name));
				hp = data[pos++];
				sp = data[pos++];
				grace = data[pos++];
				list_add(list_party, list_party->rows, 0, name);
				PARTY_STAT_BAR();
				list_add(list_party, list_party->rows - 1, 1, bars);
			}
		}

		/* Sort the list of party members alphabetically. */
		if (type == CMD_PARTY_WHO)
		{
			list_sort(list_party, LIST_SORT_ALPHA);
		}

		/* Update column names, depending on the list contents. */
		list_set_column(list_party, 0, -1, -1, type == CMD_PARTY_LIST ? "Party name" : "Player", -1);
		list_set_column(list_party, 1, -1, -1, type == CMD_PARTY_LIST ? "Leader" : "Stats", -1);

		list_contents = type;
		cur_widget[PARTY_ID]->redraw = 1;
		cur_widget[PARTY_ID]->show = 1;
		SetPriorityWidget(cur_widget[PARTY_ID]);
	}
	/* Join command; store the party name we're member of, and show the
	 * list of party members, if the party widget is not hidden. */
	else if (type == CMD_PARTY_JOIN)
	{
		GetString_String(data, &pos, cpl.partyname, sizeof(cpl.partyname));

		if (cur_widget[PARTY_ID]->show)
		{
			send_command("/party who");
		}
	}
	/* Leave; clear the party name and switch to list of parties (unless
	 * the party widget is hidden). */
	else if (type == CMD_PARTY_LEAVE)
	{
		cpl.partyname[0] = '\0';

		if (cur_widget[PARTY_ID]->show)
		{
			send_command("/party list");
		}
	}
	/* Party requires password, bring up the console for the player to
	 * enter the password. */
	else if (type == CMD_PARTY_PASSWORD)
	{
		GetString_String(data, &pos, cpl.partyjoin, sizeof(cpl.partyjoin));
		cpl.input_mode = INPUT_MODE_CONSOLE;
		text_input_open(253);
		text_input_set_string("/party joinpassword ");
	}
	/* Update list of party members. */
	else if (type == CMD_PARTY_UPDATE)
	{
		char name[MAX_BUF], bars[MAX_BUF];
		uint8 hp, sp, grace;
		uint32 row;

		if (list_contents != CMD_PARTY_WHO)
		{
			return;
		}

		GetString_String(data, &pos, name, sizeof(name));
		hp = data[pos++];
		sp = data[pos++];
		grace = data[pos++];

		PARTY_STAT_BAR();
		cur_widget[PARTY_ID]->redraw = 1;

		for (row = 0; row < list_party->rows; row++)
		{
			if (!strcmp(list_party->text[row][0], name))
			{
				free(list_party->text[row][1]);
				list_party->text[row][1] = strdup(bars);
				return;
			}
		}

		list_add(list_party, list_party->rows, 0, name);
		list_add(list_party, list_party->rows - 1, 1, bars);
		list_sort(list_party, LIST_SORT_ALPHA);
	}
	/* Remove member from the list of party members. */
	else if (type == CMD_PARTY_REMOVE_MEMBER)
	{
		char name[MAX_BUF];
		uint32 row;

		if (list_contents != CMD_PARTY_WHO)
		{
			return;
		}

		GetString_String(data, &pos, name, sizeof(name));
		cur_widget[PARTY_ID]->redraw = 1;

		for (row = 0; row < list_party->rows; row++)
		{
			if (!strcmp(list_party->text[row][0], name))
			{
				list_remove_row(list_party, row);
				return;
			}
		}
	}
}

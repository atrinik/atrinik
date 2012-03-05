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
 * Implements party type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** Width of the hp/sp stat bar. */
#define STAT_BAR_WIDTH 60

/** Macro to create the stat bar markup. */
#define PARTY_STAT_BAR() \
	snprintf(bars, sizeof(bars), "<x=5><bar=#000000 %d 6><bar=#cb0202 %d 6><y=6><bar=#000000 %d 6><bar=#1818a4 %d 6>", STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (hp / 100.0)), STAT_BAR_WIDTH, (int) (STAT_BAR_WIDTH * (sp / 100.0)));

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

/** @copydoc socket_command_struct::handle_func */
void socket_command_party(uint8 *data, size_t len, size_t pos)
{
	uint8 type;

	type = packet_to_uint8(data, len, &pos);

	/* List of parties, or list of party members. */
	if (type == CMD_PARTY_LIST || type == CMD_PARTY_WHO)
	{
		list_clear(list_party);

		while (pos < len)
		{
			if (type == CMD_PARTY_LIST)
			{
				char party_name[MAX_BUF], party_leader[MAX_BUF];

				packet_to_string(data, len, &pos, party_name, sizeof(party_name));
				packet_to_string(data, len, &pos, party_leader, sizeof(party_leader));
				list_add(list_party, list_party->rows, 0, party_name);
				list_add(list_party, list_party->rows - 1, 1, party_leader);
			}
			else if (type == CMD_PARTY_WHO)
			{
				char name[MAX_BUF], bars[MAX_BUF];
				uint8 hp, sp;

				packet_to_string(data, len, &pos, name, sizeof(name));
				hp = packet_to_uint8(data, len, &pos);
				sp = packet_to_uint8(data, len, &pos);
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
		packet_to_string(data, len, &pos, cpl.partyname, sizeof(cpl.partyname));

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
		char buf[MAX_BUF];

		packet_to_string(data, len, &pos, cpl.partyjoin, sizeof(cpl.partyjoin));
		snprintf(buf, sizeof(buf), "?MCON /joinpassword ");
		keybind_process_command(buf);
	}
	/* Update list of party members. */
	else if (type == CMD_PARTY_UPDATE)
	{
		char name[MAX_BUF], bars[MAX_BUF];
		uint8 hp, sp;
		uint32 row;

		if (list_contents != CMD_PARTY_WHO)
		{
			return;
		}

		packet_to_string(data, len, &pos, name, sizeof(name));
		hp = packet_to_uint8(data, len, &pos);
		sp = packet_to_uint8(data, len, &pos);

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

		packet_to_string(data, len, &pos, name, sizeof(name));
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

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
	SDL_Rect box, dst;

	if (widget->redraw)
	{
		surface_show(widget->surface, 0, 0, NULL, TEXTURE_CLIENT("content"));

		box.h = 0;
		box.w = widget->w;
		string_show(widget->surface, FONT_SERIF12, "Party", 0, 3, COLOR_HGOLD, TEXT_ALIGN_CENTER, &box);

		if (list_party)
		{
			list_set_parent(list_party, widget->x, widget->y);
			list_show(list_party, 10, 23);
		}

		widget->redraw = list_need_redraw(list_party);
	}

	dst.x = widget->x;
	dst.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &dst);

	/* Render the various buttons. */
	button_close.x = widget->x + widget->w - TEXTURE_CLIENT("button_round")->w - 4;
	button_close.y = widget->y + 4;
	button_show(&button_close, "X");

	button_help.x = widget->x + widget->w - TEXTURE_CLIENT("button_round")->w * 2 - 4;
	button_help.y = widget->y + 4;
	button_show(&button_help, "?");

	button_parties.x = widget->x + 244;
	button_parties.y = widget->y + 38;
	button_show(&button_parties, list_contents == CMD_PARTY_LIST ? "<u>Parties</u>" : "Parties");

	button_members.x = button_form.x = widget->x + 244;
	button_members.y = button_form.y = widget->y + 60;

	if (cpl.partyname[0] == '\0')
	{
		button_show(&button_form, "Form");
	}
	else
	{
		button_show(&button_members, list_contents == CMD_PARTY_WHO ? "<u>Members</u>" : "Members");
		button_leave.x = button_password.x = button_chat.x = widget->x + 244;
		button_leave.y = widget->y + 82;
		button_password.y = widget->y + 104;
		button_chat.y = widget->y + 126;
		button_show(&button_leave, "Leave");
		button_show(&button_password, "Password");
		button_show(&button_chat, "Chat");
	}
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
	/* Create the surface. */
	if (!widget->surface)
	{
		SDL_Surface *texture;

		texture = TEXTURE_CLIENT("content");
		widget->surface = SDL_ConvertSurface(texture, texture->format, texture->flags);
	}

	/* Create the party list. */
	if (!list_party)
	{
		list_party = list_create(12, 2, 8);
		list_party->handle_enter_func = list_handle_enter;
		list_party->surface = widget->surface;
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
		button_close.texture = button_help.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
		button_close.texture_pressed = button_help.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
		button_close.texture_over = button_help.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");

		button_parties.flags = button_members.flags = TEXT_MARKUP;
		widget->redraw = 1;
		list_contents = -1;
	}
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	char buf[MAX_BUF];

	/* If the list has handled the mouse event, we need to redraw the
	 * widget. */
	if (list_party && list_handle_mouse(list_party, event))
	{
		widget->redraw = 1;
		return 1;
	}
	else if (button_event(&button_close, event))
	{
		widget->show = 0;
		return 1;
	}
	else if (button_event(&button_help, event))
	{
		help_show("party list");
		return 1;
	}
	else if (button_event(&button_parties, event))
	{
		send_command("/party list");
		return 1;
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_members, event))
	{
		send_command("/party who");
		return 1;
	}
	else if (cpl.partyname[0] == '\0' && button_event(&button_form, event))
	{
		snprintf(buf, sizeof(buf), "?MCON /party_form ");
		keybind_process_command(buf);
		return 1;
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_password, event))
	{
		snprintf(buf, sizeof(buf), "?MCON /party password ");
		keybind_process_command(buf);
		return 1;
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_leave, event))
	{
		send_command("/party leave");
		return 1;
	}
	else if (cpl.partyname[0] != '\0' && button_event(&button_chat, event))
	{
		snprintf(buf, sizeof(buf), "?MCON /gsay ");
		keybind_process_command(buf);
		return 1;
	}

	return 0;
}

/**
 * Initialize one party widget. */
void widget_party_init(widgetdata *widget)
{
	widget->draw_func = widget_draw;
	widget->background_func = widget_background;
	widget->event_func = widget_event;
}

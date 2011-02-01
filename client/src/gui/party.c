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
 * This file controls the party Graphical User Interface. */

#include <include.h>

/** Party GUI tabs */
static char *party_tabs[] =
{
	"List", 	"Who",
	"Leave",	"Password"
};

/**
 * Switch a tab. Called on switching tabs, to call the required function. */
void switch_tabs()
{
	switch (gui_interface_party->tab)
	{
		case PARTY_TAB_LIST:
			send_command("/party list");
			break;

		case PARTY_TAB_WHO:
			send_command("/party who");
			break;

		case PARTY_TAB_LEAVE:
			sprintf(gui_interface_party->command, "askleave");
			break;

		case PARTY_TAB_PASSWORD:
			sprintf(gui_interface_party->command, "askpassword");
			break;
	}
}

/**
 * Draw party tabs.
 * @param x X position where to draw tabs
 * @param y Y position where to draw tabs */
void draw_party_tabs(int x, int y)
{
	int i = 0, max = PARTY_TABS;
	int mx, my, mb;
	static int active = 0;

	if (!gui_interface_party)
	{
		return;
	}

	/* If we're not member of a party, we only have 1 tab available... */
	if (cpl.partyname[0] == '\0')
	{
		max = PARTY_TAB_LIST + 1;
	}

	mb = SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_START], x, y - 10, NULL, NULL);
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Party Actions", x + 15, y + 4, COLOR_BLACK, NULL, NULL);
	StringBlt(ScreenSurface, &SystemFont, "Party Actions", x + 14, y + 3, COLOR_WHITE, NULL, NULL);

	y += 17;
	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);
	y += 17;

	/* Loop through the tabs */
	while (i < max)
	{
		sprite_blt(Bitmaps[BITMAP_DIALOG_TAB], x, y, NULL, NULL);

		/* If this is a selected tab */
		if (i == gui_interface_party->tab)
			sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_SEL], x, y, NULL, NULL);

		/* Mouse over it? */
		if (mx > x && mx < x + 100 && my > y && my < y + 17)
		{
			StringBlt(ScreenSurface, &SystemFont, party_tabs[i], x + 24, y + 3, COLOR_HGOLD, NULL, NULL);

			if (mb && mb_clicked)
				active = 1;

			/* If we clicked this tab, change the tab ID and switch tab. */
			if (active)
			{
				gui_interface_party->tab = i;
				switch_tabs();
			}
		}
		else
			StringBlt(ScreenSurface, &SystemFont, party_tabs[i], x + 24, y + 3, COLOR_WHITE, NULL, NULL);

		y += 17;
		i++;
	}

	sprite_blt(Bitmaps[BITMAP_DIALOG_TAB_STOP], x, y, NULL, NULL);

	if (!mb)
		active = 0;
}

/**
 * Show the party interface. */
void show_party()
{
	char partyname[MAX_BUF], partyleader[MAX_BUF];
	_gui_party_line *party_line;
	SDL_Rect box;
	char buf[256];
	int x, y, i = 0;
	static int active = 0, dblclk = 0;
	static Uint32 Ticks = 0;
	int mx, my, mb;

	mb = SDL_GetMouseState(&mx, &my);

	/* Party list command */
	if (strcmp(gui_interface_party->command, "list") == 0)
	{
		/* Background */
		x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
		y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
		sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_PARTY], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_PARTY]->bitmap->w / 2, y + 16, NULL, NULL);
		add_close_button(x, y, MENU_PARTY);

		/* Headline */
		StringBlt(ScreenSurface, &SystemFont, "Party name", x + 136 + 1, y + 82 - 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Party name", x + 136, y + 82 - 2, COLOR_WHITE, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Leader", x + 370 + 1, y + 82 - 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Leader", x + 370, y + 82 - 2, COLOR_WHITE, NULL, NULL);

		draw_party_tabs(x + 8, y + 70);

		/* Show usage */
		sprintf(buf, "~SHIFT~ + ~%c%c~ to switch tab                   ~%c%c~ to select party                    ~RETURN~ to join", ASCII_UP, ASCII_DOWN, ASCII_UP, ASCII_DOWN);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

		/* And some info */
		StringBlt(ScreenSurface, &SystemFont, "This is a list of all current parties in the game.", x + 156, y + 432, COLOR_WHITE, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Green marks the party you're member of.", x + 156, y + 444, COLOR_WHITE, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Press ~f~ to form a new party.", x + 156, y + 456, COLOR_WHITE, NULL, NULL);

		box.x = x + 133;
		box.y = y + 83;
		box.w = 329;
		box.h = 12;

		/* Frame for selection field */
		draw_frame(ScreenSurface, box.x - 1, box.y + 11, box.w + 1, 313);

		/* Frame for scrollbar */
		draw_frame(ScreenSurface, box.x + box.w + 4, box.y + 11, 10, 313);

		/* Show scrollbar, and adjust its position by yoff */
		blt_window_slider(Bitmaps[BITMAP_SLIDER_LONG], gui_interface_party->lines - 18, 8, gui_interface_party->yoff, -1, box.x + box.w + 5, box.y + 12);

		if (!(mb & SDL_BUTTON(SDL_BUTTON_LEFT)))
			active = 0;

		/* Determine the selected row */
		if (mx > x + 136 && mx < x + 136 + 327 && my > y + 82 && my < y + 12 + 82 + DIALOG_LIST_ENTRY * 12)
		{
			if (!mb)
			{
				if (dblclk == 1)
					dblclk = 2;

				if (dblclk == 3)
				{
					dblclk = 0;

					check_menu_keys(MENU_PARTY, SDLK_RETURN);
				}
			}
			else
			{
				if (dblclk == 0)
				{
					dblclk = 1;
					Ticks = SDL_GetTicks();
				}

				if (dblclk == 2)
				{
					dblclk = 3;
					if (SDL_GetTicks() - Ticks > 300)
						dblclk = 0;
				}

				/* mb was pressed in the selection field */
				if (mb_clicked)
					active = 1;

				if (active && gui_interface_party->selected != (my - y - 12 - 82) / 12 + gui_interface_party->yoff)
				{
					gui_interface_party->selected = (my - y - 12 - 82) / 12 + gui_interface_party->yoff;

					if (gui_interface_party->selected >= gui_interface_party->lines - 1)
						gui_interface_party->selected = gui_interface_party->lines - 1;

					dblclk = 0;
				}
			}
		}

		party_line = gui_interface_party->start;

		/* Loop through the lines */
		while (party_line)
		{
			/* If it's within yoff... */
			if (gui_interface_party->yoff <= i)
			{
				y += 12;
				box.y += 12;

				/* Determine what is party name and what is party leader */
				sscanf(party_line->line, "Name: %32[^\t]\tLeader: %s", partyname, partyleader);

				/* If this is a party we're member of */
				if (strcmp(partyname, cpl.partyname) == 0)
				{
					SDL_FillRect(ScreenSurface, &box, sdl_dgreen);
				}
				/* Party we're not member of and it's not selected */
				else if (i != gui_interface_party->selected)
				{
					/* Draw them gray, but every other row will be different shade of gray */
					if (i & 1)
						SDL_FillRect(ScreenSurface, &box, sdl_gray2);
					else
						SDL_FillRect(ScreenSurface, &box, sdl_gray1);
				}
				/* Selected row... Blue */
				else
					SDL_FillRect(ScreenSurface, &box, sdl_blue1);

				/* Print out the party name & leader */
				StringBlt(ScreenSurface, &SystemFont, partyname, x + 136, y + 82, COLOR_WHITE, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, partyleader, x + 370, y + 82, COLOR_WHITE, NULL, NULL);
			}

			party_line = party_line->next;
			i++;

			/* Never more than maximum */
			if (i - gui_interface_party->yoff >= DIALOG_LIST_ENTRY)
				break;
		}

		/* So that any remaining rows are printed empty (if number of parties is smaller than our maximum) */
		if (gui_interface_party->yoff)
			i -= gui_interface_party->yoff;

		/* Print out those empty rows */
		while (i < DIALOG_LIST_ENTRY)
		{
			y += 12;
			box.y += 12;

			/* Not selected row */
			if (i != gui_interface_party->selected || gui_interface_party->selected == 0)
			{
				/* Draw them gray, but every other row will be different shade of gray */
				if (i & 1)
					SDL_FillRect(ScreenSurface, &box, sdl_gray2);
				else
					SDL_FillRect(ScreenSurface, &box, sdl_gray1);
			}
			/* Selected row */
			else
				SDL_FillRect(ScreenSurface, &box, sdl_blue1);

			i++;
		}
	}
	/* Party who command */
	else if (strcmp(gui_interface_party->command, "who") == 0)
	{
		char membername[HUGE_BUF], memberlevel[MAX_BUF], membermap[HUGE_BUF];

		/* Background */
		x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
		y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
		sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_PARTY], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_PARTY]->bitmap->w / 2, y + 16, NULL, NULL);
		add_close_button(x, y, MENU_PARTY);

		/* Headline */
		StringBlt(ScreenSurface, &SystemFont, "Name", x + 136 + 1, y + 82 - 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Name", x + 136, y + 82 - 2, COLOR_WHITE, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Level", x + 210 + 1, y + 82 - 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Level", x + 210, y + 82 - 2, COLOR_WHITE, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Map", x + 250 + 1, y + 82 - 1, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Map", x + 250, y + 82 - 2, COLOR_WHITE, NULL, NULL);

		draw_party_tabs(x + 8, y + 70);

		/* Show usage */
		sprintf(buf, "~SHIFT~ + ~%c%c~ to switch tab", ASCII_UP, ASCII_DOWN);
		StringBlt(ScreenSurface, &SystemFont, buf, x + 135, y + 410, COLOR_WHITE, NULL, NULL);

		/* And some info */
		StringBlt(ScreenSurface, &SystemFont, "This is a list of members in your party.", x + 156, y + 432, COLOR_WHITE, NULL, NULL);

		box.x = x + 133;
		box.y = y + 83;
		box.w = 329;
		box.h = 12;

		/* Frame for selection field */
		draw_frame(ScreenSurface, box.x - 1, box.y + 11, box.w + 1, 313);

		/* Frame for scrollbar */
		draw_frame(ScreenSurface, box.x + box.w + 4, box.y + 11, 10, 313);

		/* Show scrollbar, and adjust its position by yoff */
		blt_window_slider(Bitmaps[BITMAP_SLIDER_LONG], gui_interface_party->lines - 18, 8, gui_interface_party->yoff, -1, box.x + box.w + 5, box.y + 12);

		if (!(mb & SDL_BUTTON(SDL_BUTTON_LEFT)))
			active = 0;

		/* Determine the selected row */
		if (mx > x + 136 && mx < x + 136 + 327 && my > y + 82 && my < y + 12 + 82 + DIALOG_LIST_ENTRY * 12)
		{
			if (mb)
			{
				if (dblclk == 0)
				{
					dblclk = 1;
					Ticks = SDL_GetTicks();
				}

				if (dblclk == 2)
				{
					dblclk = 3;
					if (SDL_GetTicks() - Ticks > 300)
						dblclk = 0;
				}

				/* mb was pressed in the selection field */
				if (mb_clicked)
					active = 1;

				if (active && gui_interface_party->selected != (my - y - 12 - 82) / 12 + gui_interface_party->yoff)
				{
					gui_interface_party->selected = (my - y - 12 - 82) / 12 + gui_interface_party->yoff;

					if (gui_interface_party->selected >= gui_interface_party->lines - 1)
						gui_interface_party->selected = gui_interface_party->lines - 1;

					dblclk = 0;
				}
			}
		}

		party_line = gui_interface_party->start;

		/* Loop through the lines */
		while (party_line)
		{
			/* If it's within yoff... */
			if (gui_interface_party->yoff <= i)
			{
				y += 12;
				box.y += 12;

				/* Determine what is party name and what is party leader */
				sscanf(party_line->line, "Name: %32[^\t]\tMap: %32[^\t]\tLevel: %s", membername, membermap, memberlevel);

				/* Not selected row */
				if (i != gui_interface_party->selected)
				{
					/* Draw them gray, but every other row will be different shade of gray */
					if (i & 1)
						SDL_FillRect(ScreenSurface, &box, sdl_gray2);
					else
						SDL_FillRect(ScreenSurface, &box, sdl_gray1);
				}
				/* Selected row */
				else
					SDL_FillRect(ScreenSurface, &box, sdl_blue1);

				/* Print out the party name & leader */
				StringBlt(ScreenSurface, &SystemFont, membername, x + 136, y + 82, COLOR_WHITE, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, memberlevel, x + 210, y + 82 , COLOR_WHITE, NULL, NULL);
				StringBlt(ScreenSurface, &SystemFont, membermap, x + 250, y + 82, COLOR_WHITE, NULL, NULL);
			}

			party_line = party_line->next;
			i++;

			/* Never more than maximum */
			if (i - gui_interface_party->yoff >= DIALOG_LIST_ENTRY)
				break;
		}

		/* So that any remaining rows are printed empty (if number of parties is smaller than our maximum) */
		if (gui_interface_party->yoff)
			i -= gui_interface_party->yoff;

		/* Print out those empty rows */
		while (i < DIALOG_LIST_ENTRY)
		{
			y += 12;
			box.y += 12;

			/* Not selected row */
			if (i != gui_interface_party->selected || gui_interface_party->selected == 0)
			{
				/* Draw them gray, but every other row will be different shade of gray */
				if (i & 1)
					SDL_FillRect(ScreenSurface, &box, sdl_gray2);
				else
					SDL_FillRect(ScreenSurface, &box, sdl_gray1);
			}
			/* Selected row */
			else
				SDL_FillRect(ScreenSurface, &box, sdl_blue1);

			i++;
		}
	}
	else if (strcmp(gui_interface_party->command, "password") == 0)
	{
		cpl.input_mode = INPUT_MODE_CONSOLE;
		text_input_open(8);
		cpl.menustatus = MENU_NO;
	}
	/* Screen to show a confirmation to leave party from the GUI */
	else if (strcmp(gui_interface_party->command, "askleave") == 0)
	{
		/* Background */
		x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
		y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
		sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_PARTY], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_PARTY]->bitmap->w / 2, y + 16, NULL, NULL);
		add_close_button(x, y, MENU_PARTY);

		StringBlt(ScreenSurface, &SystemFont, "Are you sure you want to leave your current party?", x + 188, y + 141, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Are you sure you want to leave your current party?", x + 187, y + 140, COLOR_WHITE, NULL, NULL);

		StringBlt(ScreenSurface, &SystemFont, "Press ~y~ to leave, or ~n~ to cancel.", x + 232, y + 171, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Press ~y~ to leave, or ~n~ to cancel.", x + 233, y + 170, COLOR_WHITE, NULL, NULL);

		draw_party_tabs(x + 8, y + 70);
	}
	/* Screen to show a confirmation to change the password from the GUI */
	else if (strcmp(gui_interface_party->command, "askpassword") == 0)
	{
		/* Background */
		x = Screensize->x / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->w / 2;
		y = Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2;
		sprite_blt(Bitmaps[BITMAP_DIALOG_BG], x, y, NULL, NULL);
		sprite_blt(Bitmaps[BITMAP_DIALOG_TITLE_PARTY], x + 250 - Bitmaps[BITMAP_DIALOG_TITLE_PARTY]->bitmap->w / 2, y + 16, NULL, NULL);
		add_close_button(x, y, MENU_PARTY);

		StringBlt(ScreenSurface, &SystemFont, "Are you sure you want to change the password of your current party?", x + 148, y + 141, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Are you sure you want to change the password of your current party?", x + 147, y + 140, COLOR_WHITE, NULL, NULL);

		StringBlt(ScreenSurface, &SystemFont, "Press ~y~ to continue, or ~n~ to cancel.", x + 221, y + 171, COLOR_BLACK, NULL, NULL);
		StringBlt(ScreenSurface, &SystemFont, "Press ~y~ to continue, or ~n~ to cancel.", x + 220, y + 170, COLOR_WHITE, NULL, NULL);

		draw_party_tabs(x + 8, y + 70);
	}
}

/**
 * Called on mouse event in party GUI
 * @param e SDL event */
void gui_party_interface_mouse(SDL_Event *e)
{
	if (!gui_interface_party)
		return;

	/* Mousewheel up/down */
	if (e->button.button == SDL_BUTTON_WHEELUP || e->button.button == SDL_BUTTON_WHEELDOWN)
	{
		/* Scroll down... */
		if (e->button.button == SDL_BUTTON_WHEELDOWN)
			gui_interface_party->yoff++;
		/* .. or up */
		else
			gui_interface_party->yoff--;

		/* Sanity checks for going out of bounds */
		if (gui_interface_party->yoff < 0 || gui_interface_party->lines < DIALOG_LIST_ENTRY)
			gui_interface_party->yoff = 0;
		else if (gui_interface_party->yoff >= gui_interface_party->lines - DIALOG_LIST_ENTRY)
			gui_interface_party->yoff = gui_interface_party->lines - DIALOG_LIST_ENTRY;
	}
}

/**
 * Free and clear the party GUI */
void clear_party_interface()
{
	if (!gui_interface_party)
		return;

	free(gui_interface_party);
	gui_interface_party = NULL;
}

/**
 * Initialize the party interface.
 * @param data Data to initialize the interface from
 * @param len Length of the data
 * @return Party structure */
_gui_party_struct *load_party_interface(char *data, int len)
{
	_gui_party_line *party_line;
	int i = 0, tab = gui_interface_party ? gui_interface_party->tab : 0, pos = 0;
	uint8 command = 0;

	/* Start clean */
	clear_party_interface();

	if (len != -1)
	{
		command = (uint8) (data[pos++]);

		switch (command)
		{
			case CMD_PARTY_LEAVE:
				cpl.partyname[0] = '\0';
				return NULL;

			case CMD_PARTY_JOIN:
				strncpy(cpl.partyname, data + pos, sizeof(cpl.partyname) - 1);
				return NULL;
		}
	}

	/* Initialize the structure */
	gui_interface_party = (_gui_party_struct *) malloc(sizeof(_gui_party_struct));
	gui_interface_party->start = NULL;

	if (len != -1)
	{
		switch (command)
		{
			case CMD_PARTY_LIST:
				tab = PARTY_TAB_LIST;
				strcpy(gui_interface_party->command, "list");
				break;

			case CMD_PARTY_WHO:
				tab = PARTY_TAB_WHO;
				strcpy(gui_interface_party->command, "who");
				break;

			case CMD_PARTY_PASSWORD:
				strcpy(gui_interface_party->command, "password");
				break;
		}

		while (pos < len)
		{
			party_line = (_gui_party_line *) malloc(sizeof(_gui_party_line));

			party_line->next = gui_interface_party->start;
			gui_interface_party->start = party_line;
			i++;

			if (command == CMD_PARTY_LIST)
			{
				size_t j;
				char c, party_name[MAX_BUF], party_leader[MAX_BUF];

				j = 0;

				while ((c = (char) (data[pos++])))
				{
					party_name[j++] = c;
				}

				party_name[j] = '\0';
				j = 0;

				while ((c = (char) (data[pos++])))
				{
					party_leader[j++] = c;
				}

				party_leader[j] = '\0';
				snprintf(party_line->line, sizeof(party_line->line), "Name: %s\tLeader: %s", party_name, party_leader);
			}
			else if (command == CMD_PARTY_WHO)
			{
				size_t j;
				char c, player_name[MAX_BUF], map_name[MAX_BUF];
				uint8 level;

				j = 0;

				while ((c = (char) (data[pos++])))
				{
					player_name[j++] = c;
				}

				player_name[j] = '\0';
				j = 0;

				while ((c = (char) (data[pos++])))
				{
					map_name[j++] = c;
				}

				map_name[j] = '\0';
				level = (uint8) (data[pos++]);
				snprintf(party_line->line, sizeof(party_line->line), "Name: %s\tMap: %s\tLevel: %d", player_name, map_name, level);
			}
			else if (command == CMD_PARTY_PASSWORD)
			{
				strncpy(party_line->line, data + pos, sizeof(party_line->line) - 1);
				break;
			}
			else
			{
				break;
			}
		}
	}
	else
	{
		strcpy(gui_interface_party->command, data);
	}

	/* Total of lines */
	gui_interface_party->lines = i;
	gui_interface_party->yoff = 0;
	gui_interface_party->selected = 0;
	gui_interface_party->tab = tab;

	return gui_interface_party;
}

/**
 * Called from do_console() in menu.c, this function is used when
 * we open console in order to type party password when joining,
 * or setting a new password.
 * @return 1 if we want to close the console, 0 otherwise */
int console_party()
{
	/* No GUI or ESC was pressed */
	if (!gui_interface_party || text_input_string_esc_flag)
	{
		return 0;
	}

	/* Password command - used when server asks us for a password when joining party. */
	if (strcmp(gui_interface_party->command, "password") == 0)
	{
		if (gui_interface_party->start && text_input_string_flag == 0 && text_input_string_end_flag)
		{
			char buf[HUGE_BUF];

			snprintf(buf, sizeof(buf), "/party join %s\t%s", gui_interface_party->start->line, text_input_string);
			send_command(buf);
			clear_party_interface();

			return 1;
		}
	}
	/* Form command - used when forming a party from the GUI. */
	else if (strcmp(gui_interface_party->command, "form") == 0)
	{
		/* If we found a selected line, and we got a finished string... */
		if (text_input_string_flag == 0 && text_input_string_end_flag)
		{
			char buf[HUGE_BUF];

			snprintf(buf, sizeof(buf), "/party form %s", text_input_string);
			send_command(buf);
			clear_party_interface();

			return 1;
		}
	}
	/* Password set command - used when changing party password from the GUI. */
	else if (strcmp(gui_interface_party->command, "passwordset") == 0)
	{
		/* If we found a selected line, and we got a finished string... */
		if (text_input_string_flag == 0 && text_input_string_end_flag)
		{
			char buf[HUGE_BUF];

			snprintf(buf, sizeof(buf), "/party password %s", text_input_string);
			send_command(buf);
			clear_party_interface();

			return 1;
		}
	}

	return 0;
}

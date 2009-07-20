/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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

#if !defined(__PARTY_H)
#define __PARTY_H

/* Party tabs */
enum {
	PARTY_TAB_LIST,
	PARTY_TAB_WHO,
	PARTY_TAB_LEAVE,
	PARTY_TAB_PASSWORD,
	PARTY_TABS
};

/** The main party GUI structure */
typedef struct gui_party_struct
{
	/** Command to run. Something like "list", "who", etc. */
	char command[MAX_BUF + 1];

	/** Linked list of lines */
	struct gui_party_line *start;

	/** Number of lines */
	int lines;

	/** Scroll bar position */
	int yoff;

	/** Selected row */
	int selected;

	/** Selected tab */
	int tab;
} _gui_party_struct;

/** Structure for the party GUI lines */
typedef struct gui_party_line
{
	/** The next line */
	struct gui_party_line *next;

	/** Line contents */
	char line[MAX_BUF + 1];
} _gui_party_line;

extern void switch_tabs();
extern _gui_party_struct *load_party_interface(char *data, int len);
extern void show_party();
extern void gui_party_interface_mouse(SDL_Event *e);
extern int console_party();
extern void clear_party_interface();

#endif
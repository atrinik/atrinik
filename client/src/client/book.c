/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Book GUI related code. */

#include <include.h>

/**
 * @defgroup BOOK_LINE_xxx BOOK_LINE_xxx
 *@{*/
/** Normal line. */
#define BOOK_LINE_NORMAL    0
/** The line is a title. */
#define BOOK_LINE_TITLE     1
/** The line is an icon. */
#define BOOK_LINE_ICON      2
/** The line is the book name. */
#define BOOK_LINE_NAME      4

/** Internal use. */
#define BOOK_LINE_PAGE      16
/*@}*/

/** Book data. */
_global_book_data global_book_data;

/**
 * Get a book's tag.
 * @param data Data.
 * @param pos Position to start in data.
 * @param tag Tag.
 * @return Book line on success, NULL otherwise. */
static _gui_book_line *get_book_tag(char *data, int *pos, int tag)
{
	char *buf, c;
	static _gui_book_line book_line;

	memset(&book_line, 0, sizeof(_gui_book_line));

	switch (tag)
	{
		case 't':
			book_line.mode = BOOK_LINE_TITLE;
			break;

		case 'p':
			book_line.mode = BOOK_LINE_PAGE;
			break;

		case 'b':
			book_line.mode = BOOK_LINE_NAME;
			break;

		default:
			return NULL;
	}

	(*pos)++;

	while ((c = *(data + *pos)) != '\0' && c != 0)
	{
		if (c == '>')
		{
			return &book_line;
		}

		(*pos)++;

		if (c <= ' ')
		{
			continue;
		}

		/* Check inside tags */
		switch (c)
		{
			case 't':
				if (!(buf = get_parameter_string(data, pos)))
				{
					return NULL;
				}

				if (tag == 'p')
				{
					book_line.mode |= BOOK_LINE_TITLE;
				}

				strncpy(book_line.line, buf, BOOK_LINES_CHAR);
				buf[BOOK_LINES_CHAR] = '\0';
				break;

			default:
				return NULL;
				break;
		}
	}

	return NULL;
}

/**
 * Check book's tag. Will use get_book_tag() to actually get the tags.
 * @param data Data.
 * @param len Length of the data.
 * @param pos Position.
 * @return Book line on success, NULL otherwise. */
static _gui_book_line *check_book_tag(char *data, int len, int *pos)
{
	int c;
	_gui_book_line *book_line;

	for (; len > *pos; (*pos)++)
	{
		c = *(data + *pos);

		if (c <= ' ')
		{
			continue;
		}

		book_line = get_book_tag(data, pos, c);

		return book_line;
	}

	return NULL;
}

/**
 * Link a page to the book GUI.
 *
 * Allocates the ::gui_interface_book pointer if it isn't already.
 * @param page Page to link. */
static void book_link_page(_gui_book_page *page)
{
	_gui_book_page *page_link;

	if (!gui_interface_book)
	{
		gui_interface_book = malloc(sizeof(_gui_book_struct));
		memset(gui_interface_book, 0, sizeof(_gui_book_struct));
	}

	if (!gui_interface_book->start)
	{
		gui_interface_book->start = page;
	}
	else
	{
		page_link = gui_interface_book->start;

		for (; page_link->next; page_link = page_link->next)
		{
		}

		page_link->next = page;
	}
}

/**
 * Post formatting and initializing of a loaded book.
 *
 * Sets the book's name, counts the pages in the book, etc.
 * @param name Book name to set. */
static void format_book(char *name)
{
	int pc = 0;
	_gui_book_page *page;

	if (!gui_interface_book)
	{
		return;
	}

	gui_interface_book->page_show = 0;
	strcpy(gui_interface_book->name, name);
	page = gui_interface_book->start;

	while (page)
	{
		pc++;
		page = page->next;
	}

	gui_interface_book->pages = pc;
}

/**
 * Free the @ref gui_interface_book "book GUI".  */
static void book_clear()
{
	int i;
	_gui_book_page *page_tmp, *page;

	if (!gui_interface_book)
	{
		return;
	}

	page = gui_interface_book->start;

	while (page)
	{
		page_tmp = page->next;

		for (i = 0; i < BOOK_PAGE_LINES; i++)
		{
			if (page->line[i])
			{
				free(page->line[i]);
			}
		}

		free(page);
		page = page_tmp;
	}

	free(gui_interface_book);
	gui_interface_book = NULL;
}

/**
 * Load book interface.
 * @param data Data.
 * @param len Length of data.
 * @return The loaded book interface.
 * @todo Get rid of the gotos. */
_gui_book_struct *book_gui_load(char *data, int len)
{
	_gui_book_line current_book_line, *book_line;
	int pos = 0, force_line;
	size_t lc = 0;
	_gui_book_page current_book_page;
	int plc = 0, plc_logic = 0;
	char c, name[256] = "";

	strncpy(name, "Book", sizeof(name));
	book_clear();
	memset(&current_book_page, 0, sizeof(_gui_book_page));
	memset(&current_book_line, 0, sizeof(_gui_book_line));

	for (pos = 0; len > pos; pos++)
	{
		c = *(data + pos);

		if (c == '\r')
		{
			continue;
		}

		/* If this character is "\" and the next is either < or >, skip
		 * it. */
		if (c == '\\' && (*(data + pos + 1) == '<' || *(data + pos + 1) == '>'))
		{
			continue;
		}

		/* Only allow tags that do not start with "\<" */
		if (c == '<' && *(data + pos - 1) != '\\')
		{
			pos++;
			book_line = check_book_tag(data, len, &pos);

			if (!book_line)
			{
				draw_info(data, COLOR_GREEN);
				draw_info("ERROR in book cmd!", COLOR_RED);
				return NULL;
			}

			if (book_line->mode & BOOK_LINE_NAME)
			{
				strcpy(name, book_line->line);
				memset(&current_book_line, 0, sizeof(_gui_book_line));
				continue;
			}

title_repeat_jump:
			if ((book_line->mode & BOOK_LINE_TITLE && plc_logic + 2 >= BOOK_PAGE_LINES) || book_line->mode & BOOK_LINE_PAGE)
			{
				_gui_book_page *page = malloc(sizeof(_gui_book_page));

				/* add the page & reset the current one */
				memcpy(page, &current_book_page, sizeof(_gui_book_page));
				book_link_page(page);
				memset(&current_book_page, 0, sizeof(_gui_book_page));
				plc = 0;
				plc_logic = 0;
			}

			if (book_line->mode & BOOK_LINE_TITLE)
			{
				int l_len;
				_gui_book_line *b_line = malloc(sizeof(_gui_book_line));

				memcpy(b_line, book_line, sizeof(_gui_book_line));
				b_line->mode = BOOK_LINE_TITLE;
				current_book_page.line[plc++] = b_line;
				plc_logic += 1;

				/* lets check we need to break the title line (because its to big) */
				if (StringWidthOffset(&BigFont, b_line->line, &l_len, 186))
				{
					int i = l_len;
					b_line->line[l_len] = 0;

					/* now lets go back to a ' ' if we don't find one, we cut the line hard */
					for (i = l_len; i >= 0; i--)
					{
						if (b_line->line[i] == ' ')
						{
							/* thats our real eof */
							b_line->line[i] = 0;
							break;
						}
					}

					/* lets see where our real eol is ... */
					if (i < 0)
						i = l_len;

					/* now lets remove all whitespaces.. if we hit EOL, jump back */
					for (; ; i++)
					{
						if (book_line->line[i] != ' ')
							break;
					}

					if (strlen(&book_line->line[i]))
					{
						memcpy(book_line->line, &book_line->line[i], strlen(&book_line->line[i]) + 1);
						goto title_repeat_jump;
					}
				}
			}
			continue;
		}

		if (c == '>' && *(data + pos - 1) != '\\')
		{
			draw_info(data, COLOR_GREEN);
			draw_info("ERROR in book cmd!", COLOR_RED);
			return NULL;
		}

		/* We have a line */
		if (c == '\0' || c == '\n')
		{
			int l_len;
			_gui_book_line *tmp_line;

			force_line = 0;
force_line_jump:
			current_book_line.line[lc] = '\0';

			book_line = malloc(sizeof(_gui_book_line));
			memcpy(book_line, &current_book_line,sizeof(_gui_book_line));
			current_book_page.line[plc] = book_line;
			tmp_line = current_book_page.line[plc++];
			plc_logic++;
			lc = 0;

			if (plc_logic >= BOOK_PAGE_LINES)
			{
				_gui_book_page *page = malloc(sizeof(_gui_book_page));

				/* Add the page and reset the current one */
				memcpy(page, &current_book_page, sizeof(_gui_book_page));
				book_link_page(page);
				memset(&current_book_page, 0, sizeof(_gui_book_page));
				plc = 0;
				plc_logic = 0;
			}

			/* Now check the last line - if the line is too long, adjust
			 * it. */
			if (StringWidthOffset((tmp_line->mode == BOOK_LINE_TITLE) ? &BigFont : &MediumFont, tmp_line->line, &l_len, 186))
			{
				int i, wspace_flag = 1;

				tmp_line->line[l_len] = '\0';

				for (i = l_len; i >= 0; i--)
				{
					if (tmp_line->line[i] == ' ')
					{
						/* End of file */
						tmp_line->line[i] = '\0';
						break;
					}
					else if (i > 0)
					{
						if (tmp_line->line[i] == '(' && tmp_line->line[i - 1] == ')')
						{
							tmp_line->line[i] = '\0';
							wspace_flag = 0;
							break;
						}
					}
				}

				if (i < 0)
				{
					i = l_len;
				}

				/* Remove all whitespace */
				if (wspace_flag)
				{
					for (; ; i++)
					{
						if (current_book_line.line[i] == 0)
						{
							/* End of line */
							if (!force_line)
							{
								/* Clear input line setting */
								memset(&current_book_line, 0, sizeof(_gui_book_line));
							}

							goto force_line_jump_out;
						}

						if (current_book_line.line[i] != ' ')
						{
							break;
						}
					}
				}

				memcpy(current_book_line.line, &current_book_line.line[i], strlen(&current_book_line.line[i]) + 1);
				lc = strlen(current_book_line.line);

				if (force_line)
				{
					if (StringWidth((tmp_line->mode == BOOK_LINE_TITLE) ? &BigFont : &MediumFont, current_book_line.line) < 186)
					{
						goto force_line_jump_out;
					}
				}

				goto force_line_jump;
			}

			memset(&current_book_line, 0, sizeof(_gui_book_line));
force_line_jump_out:
			continue;
		}

		current_book_line.line[lc++] = c;

		if (lc >= BOOK_LINES_CHAR - 2)
		{
			force_line = 1;
			goto force_line_jump;
		}
	}

	if (plc_logic)
	{
		_gui_book_page *page = malloc(sizeof(_gui_book_page));

		/* Add the page and reset the current one */
		memcpy(page, &current_book_page, sizeof(_gui_book_page));
		book_link_page(page);
	}

	format_book(name);
	return gui_interface_book;
}

/**
 * Show the book interface. */
void book_gui_show()
{
	char buf[128];
	SDL_Rect box;
	int i, ii, yoff, x, y, page_id;
	_gui_book_page *page;

	x = Screensize->x / 2 - Bitmaps[BITMAP_JOURNAL]->bitmap->w / 2;
	y = Screensize->y / 2 - Bitmaps[BITMAP_JOURNAL]->bitmap->h / 2;

	sprite_blt(Bitmaps[BITMAP_JOURNAL], x, y, NULL, NULL);
	global_book_data.x = x;
	global_book_data.y = y;
	global_book_data.xlen = Bitmaps[BITMAP_JOURNAL]->bitmap->w;
	global_book_data.ylen = Bitmaps[BITMAP_JOURNAL]->bitmap->h;

	if (!gui_interface_book)
	{
		return;
	}

	if (gui_interface_book->name)
	{
		StringBlt(ScreenSurface, &BigFont, gui_interface_book->name, x + global_book_data.xlen / 2 - get_string_pixel_length(gui_interface_book->name, &BigFont) / 2, y + 9, COLOR_WHITE, NULL, NULL);
	}

	StringBlt(ScreenSurface, &Font6x3Out, "PRESS ESC", x + global_book_data.xlen - 50, y + 25, COLOR_WHITE, NULL, NULL);

	/* Get the two pages we show */
	page = gui_interface_book->start;

	for (i = 0; i != gui_interface_book->page_show && page; i++, page = page->next)
	{
	}

	box.x = x + 47;
	box.y = y + 72;
	box.w = 200;
	box.h = 300;

	if (!page)
	{
		return;
	}

	for (page_id = 1; page_id <= 2; page_id++)
	{
		snprintf(buf, sizeof(buf), "Page %d of %d", gui_interface_book->page_show + page_id, gui_interface_book->pages);
		StringBlt(ScreenSurface, &Font6x3Out, buf, box.x + 70, box.y + 295, COLOR_WHITE, NULL, NULL);
		SDL_SetClipRect(ScreenSurface, &box);

		for (yoff = 0, i = 0, ii = 0; ii < BOOK_PAGE_LINES; ii++, yoff += 16)
		{
			if (!page->line[i])
			{
				break;
			}

			if (page->line[i]->mode == BOOK_LINE_NORMAL)
			{
				StringBlt(ScreenSurface, &MediumFont, page->line[i]->line , box.x + 2, box.y + 2 + yoff, COLOR_DBROWN, NULL, NULL);
			}
			else if (page->line[i]->mode == BOOK_LINE_TITLE)
			{
				StringBlt(ScreenSurface, &BigFont, page->line[i]->line, box.x + 2, box.y + 2 + yoff, COLOR_DBROWN, NULL, NULL);
			}

			i++;
		}

		SDL_SetClipRect(ScreenSurface, NULL);
		box.x = x + 280;

		if (!page->next)
		{
			break;
		}

		page = page->next;
	}

	if (gui_interface_book->pages)
	{
		snprintf(buf, sizeof(buf), "%c and %c to turn page", ASCII_RIGHT, ASCII_LEFT);
		StringBlt(ScreenSurface, &SystemFont, buf, box.x - 59, box.y + 300, COLOR_HGOLD, NULL, NULL);
	}
}

/**
 * Handle events when the mouse was clicked in the book GUI.
 *
 * This will attempt to find any keywords in the book, and call help menu
 * for them.
 * @param x Mouse X position.
 * @param y Mouse Y position. */
void book_gui_handle_mouse(int x, int y)
{
	int i, yoff, xoff = 50;
	_gui_book_page *page1, *page2;

	/* Get the 2 pages we show */
	page1 = gui_interface_book->start;

	/* Figure out what page we're in */
	for (i = 0; i != gui_interface_book->page_show && page1; i++, page1 = page1->next)
	{
	}

	page2 = page1->next;

	if (!page1)
	{
		return;
	}

	/* Loop through the max of book page lines */
	for (yoff = 0, i = 0; i < BOOK_PAGE_LINES; i++, yoff += 16)
	{
		if (page1->line[i] && page1->line[i]->line)
		{
			char *current_line = page1->line[i]->line, keyword[MAX_BUF];
			int in_keyword = 0;

			keyword[0] = '\0';

			/* Loop through the current line character, until we hit a null character */
			while (*current_line != '\0')
			{
				/* '^' marks either start of keyword, or an end. */
				if (*current_line == '^')
				{
					/* If we're in a keyword, it's an end. */
					if (in_keyword)
					{
						in_keyword = 0;
					}
					/* Start of keyword otherwise */
					else
					{
						in_keyword = 1;
					}
				}
				/* Otherwise we're in the keyword */
				else if (in_keyword)
				{
					char current_char[2];
					snprintf(current_char, sizeof(current_char), "%c", *current_line);
					strncat(keyword, current_char, sizeof(keyword) - strlen(keyword) - 1);
				}

				current_line++;
			}

			/* If we got a keyword */
			if (keyword[0] != '\0')
			{
				int line_len = StringWidth(&MediumFont, current_line), keyword_len = StringWidth(&MediumFont, keyword);

				/* Calculate if the click occurred on the keyword area */
				if (y < global_book_data.y + yoff + 85 && y > (global_book_data.y + yoff + 75) && x > global_book_data.x + xoff + line_len && x < global_book_data.x + xoff + line_len + keyword_len)
				{
					/* It did, show a help GUI for the keyword. */
					show_help(keyword);
					break;
				}
			}
		}

		/* If this is the end of page lines */
		if (i + 1 == BOOK_PAGE_LINES)
		{
			/* If second page doesn't exist, or we're already on it */
			if (!page2 || page1 == page2)
			{
				break;
			}

			/* Otherwise assign the page to second page, reset defaults,
			 * and assign xoff to right value for the second page. */
			page1 = page2;
			i = 0;
			yoff = 0;
			xoff = 280;
		}
	}
}

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
 * Generic lists implementation. */

#include <include.h>

/** Start of the visible lists. */
static list_struct *list_head = NULL;
/** End of the visible lists. */
static list_struct *list_tail = NULL;
/** Used to store scrollbar position. */
static int old_scrollbar_pos = 0;

static int list_handle_key(list_struct *list, SDLKey key);

/**
 * Draw a frame in which the rows will be drawn.
 * @param list List to draw the frame for. */
static void list_draw_frame(list_struct *list)
{
	draw_frame(list->surface, list->x + list->frame_offset, LIST_ROWS_START(list) + list->frame_offset, list->width, LIST_ROWS_HEIGHT(list));
}

/**
 * Colorize a row.
 * @param list List.
 * @param row Row number, 0-[max visible rows].
 * @param box Contains base x/y/width/height information to use. */
static void list_row_color(list_struct *list, int row, SDL_Rect box)
{
	(void) list;

	if (row & 1)
	{
		SDL_FillRect(list->surface, &box, sdl_gray2);
	}
	else
	{
		SDL_FillRect(list->surface, &box, sdl_gray1);
	}
}

/**
 * Highlight a row (due to mouse being over it).
 * @param list List.
 * @param box Contains base x/y/width/height information to use. */
static void list_row_highlight(list_struct *list, SDL_Rect box)
{
	(void) list;
	SDL_FillRect(list->surface, &box, sdl_dgreen);
}

/**
 * Color a selected row.
 * @param list List.
 * @param box Contains base x/y/width/height information to use. */
static void list_row_selected(list_struct *list, SDL_Rect box)
{
	(void) list;
	SDL_FillRect(list->surface, &box, sdl_blue1);
}

/**
 * Get the currently focused list.
 *
 * If there is no focused list but there is at least one visible list,
 * will set the first list as the focused list.
 * @return Focused list, or NULL if there are no visible lists at all. */
list_struct *list_get_focused()
{
	list_struct *tmp;

	/* Try to find a focused list. */
	for (tmp = list_head; tmp; tmp = tmp->next)
	{
		if (tmp->focus)
		{
			return tmp;
		}
	}

	/* Failsafe in case there are lists, but none with active focus. */
	if (list_head)
	{
		list_head->focus = 1;
		return list_head;
	}

	return NULL;
}

/**
 * Set currently focused list.
 * @param list List to focus. */
void list_set_focus(list_struct *list)
{
	list_struct *tmp;

	/* Already focused, nothing to do. */
	if (list->focus)
	{
		return;
	}

	/* Remove focus from previously focused list. */
	for (tmp = list_head; tmp; tmp = tmp->next)
	{
		if (tmp != list)
		{
			tmp->focus = 0;
		}
	}

	list->focus = 1;
}

/**
 * Create new list.
 * @param id ID of the list, one of @ref LIST_xxx.
 * @param x X position of the list.
 * @param y Y position of the list.
 * @param max_rows Maximum number of visible rows to show.
 * @param cols How many columns per row.
 * @param spacing Spacing between column names and the actual rows start.
 * @return The created list. */
list_struct *list_create(uint32 id, int x, int y, uint32 max_rows, uint32 cols, int spacing)
{
	list_struct *list = calloc(1, sizeof(list_struct));

	if (max_rows == 0)
	{
		LOG(llevBug, "list_create(): Attempted to create a list with 0 max rows, changing to 1.\n");
		max_rows = 1;
	}

	/* Store the values. */
	list->id = id;
	list->x = x;
	list->y = y;
	list->max_rows = max_rows;
	list->cols = cols;
	list->spacing = spacing;
	list->font = FONT_SANS10;
	list->surface = ScreenSurface;

	/* Initialize defaults. */
	list->frame_offset = -2;
	list->header_height = 12;
	list->row_selected = 1;
	list->repeat_key = -1;

	/* Generic functions. */
	list->draw_frame_func = list_draw_frame;
	list->row_color_func = list_row_color;
	list->row_highlight_func = list_row_highlight;
	list->row_selected_func = list_row_selected;

	/* Initialize column data. */
	list->col_widths = calloc(1, sizeof(*list->col_widths) * list->cols);
	list->col_spacings = calloc(1, sizeof(*list->col_spacings) * list->cols);
	list->col_names = calloc(1, sizeof(*list->col_names) * list->cols);
	list->col_centered = calloc(1, sizeof(*list->col_centered) * list->cols);

	/* First list. */
	if (!list_head)
	{
		/* As this is the first list, by default it will have the focus. */
		list->focus = 1;
		list_head = list;
		list->prev = NULL;
	}
	else
	{
		list_tail->next = list;
		list->prev = list_tail;
	}

	list_tail = list;
	list->next = NULL;

	return list;
}

/**
 * Add text to list.
 * @param list List to add to.
 * @param row Row ID to add to. If it doesn't exist yet, it will be
 * allocated.
 * @param col Column ID.
 * @param str Text to add. */
void list_add(list_struct *list, uint32 row, uint32 col, const char *str)
{
	if (col > list->cols)
	{
		LOG(llevBug, "list_add(): Attempted to add column #%u, but columns max is %u.\n", col, list->cols);
		return;
	}

	/* Add new rows. */
	if (row + 1 > list->rows)
	{
		uint32 i;

		/* Update rows count and resize the array of rows. */
		list->rows = row + 1;
		list->text = realloc(list->text, sizeof(*list->text) * list->rows);

		/* Allocate columns for the new row(s). */
		for (i = row; i < list->rows; i++)
		{
			list->text[i] = calloc(1, sizeof(**list->text) * list->cols);
		}
	}

	list->text[row][col] = strdup(str);
}

/**
 * Set options for one column.
 * @param list List.
 * @param col Column ID.
 * @param width The column's ID. -1 to leave default (0).
 * @param spacing Spacing between columns. -1 to leave default (0).
 * @param name Name of the column. NULL to leave default (no name shown).
 * @param centered Whether to center the drawn name/text in the column.
 * -1 to leave default (not centered). */
void list_set_column(list_struct *list, uint32 col, int width, int spacing, const char *name, int centered)
{
	if (col > list->cols)
	{
		LOG(llevBug, "list_set_column(): Attempted to change column #%u, but columns max is %u.\n", col, list->cols);
		return;
	}

	/* Set width. */
	if (width != -1)
	{
		list->col_widths[col] = width;
		list->width += width;
	}

	/* Set spacing. */
	if (spacing != -1)
	{
		list->col_spacings[col] = spacing;
		list->width += spacing;
	}

	/* Set the column's name. */
	if (name)
	{
		/* There shouldn't be one previously, but just in case. */
		if (list->col_names[col])
		{
			free(list->col_names[col]);
		}

		list->col_names[col] = strdup(name);
	}

	/* Is the column centered? */
	if (centered != -1)
	{
		list->col_centered[col] = centered;
	}
}

/**
 * Change list's font.
 * @param list Which list to change font for.
 * @param font Font to use. */
void list_set_font(list_struct *list, int font)
{
	list->font = font;
}

/**
 * Enable scrollbar.
 * @param list List to enable scrollbar on. */
void list_scrollbar_enable(list_struct *list)
{
	list->scrollbar = 1;
}

/**
 * Get scrollbar's size.
 * @param list List.
 * @param box Where to store the size of the scrollbar.
 * @return 1 on success, 0 on failure (no scrollbar active). */
static int list_scrollbar_get_size(list_struct *list, SDL_Rect *box)
{
	uint32 col;

	if (!list->scrollbar)
	{
		return 0;
	}

	box->x = list->x + list->frame_offset + 1;
	box->y = LIST_ROWS_START(list) + list->frame_offset;
	box->w = LIST_SCROLLBAR_WIDTH;
	box->h = LIST_ROW_HEIGHT(list) * list->max_rows;

	for (col = 0; col < list->cols; col++)
	{
		box->x += list->col_widths[col] + list->col_spacings[col];
	}

	return 1;
}

/**
 * Render scrollbar for a list.
 * @param list The list. */
static void list_scrollbar_render(list_struct *list)
{
	SDL_Rect scrollbar_box;
	int mx, my;

	if (!list_scrollbar_get_size(list, &scrollbar_box))
	{
		return;
	}

	SDL_GetMouseState(&mx, &my);

	draw_frame(list->surface, scrollbar_box.x, scrollbar_box.y, scrollbar_box.w, scrollbar_box.h);

	scrollbar_box.x += 1;
	scrollbar_box.y += 1;
	scrollbar_box.w -= 1;
	scrollbar_box.h -= 1;

	if (list->rows > list->max_rows)
	{
		int scroll;

		scroll = list->max_rows + list->row_offset;
		list->scrollbar_h = scrollbar_box.h * list->max_rows / list->rows;
		list->scrollbar_y = ((scroll - list->max_rows) * scrollbar_box.h) / list->rows;

		if (list->scrollbar_h < 1)
		{
			list->scrollbar_h = 1;
		}

		if (scroll - list->max_rows > 0 && list->scrollbar_y + list->scrollbar_h < scrollbar_box.h)
		{
			list->scrollbar_y++;
		}

		scrollbar_box.h = list->scrollbar_h;
		scrollbar_box.y += list->scrollbar_y;
	}

	if (mx >= scrollbar_box.x && mx < scrollbar_box.x + scrollbar_box.w && my >= scrollbar_box.y && my < scrollbar_box.y + scrollbar_box.h)
	{
		SDL_FillRect(list->surface, &scrollbar_box, SDL_MapRGBA(list->surface->format, 175, 154, 110, 255));
	}
	else
	{
		SDL_FillRect(list->surface, &scrollbar_box, SDL_MapRGBA(list->surface->format, 157, 139, 98, 255));
	}
}

/**
 * Show one list.
 * @param list List to show. */
void list_show(list_struct *list)
{
	uint32 row, col;
	int w = 0, extra_width = 0;
	SDL_Rect box;

	/* Keys needing repeat? */
	if (list->repeat_key != -1)
	{
		if (list->repeat_key_ticks + KEY_REPEAT_DELAY - 5 < LastTick)
		{
			while ((list->repeat_key_ticks += KEY_REPEAT_DELAY - 5) < LastTick)
			{
				list_handle_key(list, list->repeat_key);
			}
		}
	}

	/* Draw a frame, if needed. */
	if (list->draw_frame_func)
	{
		list->draw_frame_func(list);
	}

	/* Draw the column names. */
	for (col = 0; col < list->cols; col++)
	{
		extra_width = 0;

		/* Center it? */
		if (list->col_centered[col])
		{
			extra_width = list->col_widths[col] / 2 - string_get_width(list->font, list->col_names[col], 0) / 2;
		}

		/* Actually draw the column name. */
		if (list->col_names[col])
		{
			string_blt_shadow(list->surface, list->font, list->col_names[col], list->x + w + extra_width, list->y, COLOR_SIMPLE(list->focus ? COLOR_WHITE : COLOR_GREY), COLOR_SIMPLE(COLOR_BLACK), 0, NULL);
		}

		w += list->col_widths[col] + list->col_spacings[col];
	}

	/* Initialize default values for coloring rows. */
	box.x = list->x + list->frame_offset;
	box.w = list->width;
	box.h = LIST_ROW_HEIGHT(list);

	list_scrollbar_render(list);

	/* Doing coloring of each row? */
	if (list->row_color_func)
	{
		for (row = 0; row < list->max_rows; row++)
		{
			box.y = LIST_ROWS_START(list) + (row * LIST_ROW_HEIGHT(list)) + list->frame_offset;
			list->row_color_func(list, row, box);
		}
	}

	/* Start printing out rows from the offset to the maximum. */
	for (row = list->row_offset; row < list->rows; row++)
	{
		/* Stop if we reached maximum number of visible rows. */
		if (LIST_ROW_OFFSET(row, list) == list->max_rows)
		{
			break;
		}

		/* Color selected row. */
		if (list->row_selected_func && (row + 1) == list->row_selected)
		{
			box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list)) + list->frame_offset;
			list->row_selected_func(list, box);
		}
		/* Color highlighted row. */
		else if (list->row_highlight_func && (row + 1) == list->row_highlighted)
		{
			box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list)) + list->frame_offset;
			list->row_highlight_func(list, box);
		}

		w = 0;

		/* Show all the columns. */
		for (col = 0; col < list->cols; col++)
		{
			/* Is there any text to show? */
			if (list->text[row][col])
			{
				SDL_Rect box;

				extra_width = 0;

				/* Center it. */
				if (list->col_centered[col])
				{
					extra_width = list->col_widths[col] / 2 - string_get_width(list->font, list->text[row][col], TEXT_WORD_WRAP) / 2;
				}

				/* Add width limit on the string. */
				box.w = list->col_widths[col] + list->col_spacings[col];
				box.h = LIST_ROW_HEIGHT(list);
				/* Output the text. */
				string_blt_shadow(list->surface, list->font, list->text[row][col], list->x + w + extra_width, LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list)), COLOR_SIMPLE(list->focus ? COLOR_WHITE : COLOR_GREY), COLOR_SIMPLE(COLOR_BLACK), TEXT_WORD_WRAP, &box);
			}

			w += list->col_widths[col] + list->col_spacings[col];
		}
	}
}

/**
 * Remove the specified list from the linked list of visible lists and
 * deinitialize it.
 * @param list List to remove. */
void list_remove(list_struct *list)
{
	uint32 row, col;

	if (!list)
	{
		return;
	}

	/* Remove it from the list. */
	if (!list->prev)
	{
		list_head = list->next;
	}
	else
	{
		list->prev->next = list->next;
	}

	if (!list->next)
	{
		list_tail = list->prev;
	}
	else
	{
		list->next->prev = list->prev;
	}

	/* Free the texts. */
	for (row = 0; row < list->rows; row++)
	{
		for (col = 0; col < list->cols; col++)
		{
			if (list->text[row][col])
			{
				free(list->text[row][col]);
			}
		}

		free(list->text[row]);
	}

	free(list->text);
	free(list->col_widths);
	free(list->col_spacings);
	free(list->col_centered);

	/* Free column names. */
	for (col = 0; col < list->cols; col++)
	{
		if (list->col_names[col])
		{
			free(list->col_names[col]);
		}
	}

	free(list->col_names);
	free(list);
}

/**
 * Remove all visible lists. */
void list_remove_all()
{
	/* Loop until there is nothing left. */
	while (list_head)
	{
		list_remove(list_head);
	}
}

/**
 * Scroll the list in the specified direction by the specified amount.
 * @param list List to scroll.
 * @param up If 1, scroll the list upwards, otherwise downwards.
 * @param scroll Amount to scroll by. */
static void list_scroll(list_struct *list, int up, int scroll)
{
	/* The actual values are unsigned. Changing them to signed here
	 * makes it easier to check for overflows below. */
	sint32 row_selected = list->row_selected, row_offset = list->row_offset;
	sint32 max_rows, rows;

	/* Number of rows. */
	rows = list->rows;
	/* Number of visible rows. */
	max_rows = list->max_rows;

	/* Scrolling upward. */
	if (up)
	{
		row_selected -= scroll;

		/* Adjust row offset if needed. */
		if (row_offset > (row_selected - 1))
		{
			row_offset -= scroll;
		}
	}
	/* Downward otherwise. */
	else
	{
		row_selected += scroll;

		/* Adjust row offset if needed. */
		if (row_selected >= max_rows + row_offset)
		{
			row_offset += scroll;
		}
	}

	/* Make sure row offset is within bounds. */
	if (row_offset < 0 || rows < max_rows)
	{
		row_offset = 0;
	}
	else if (row_offset >= rows - max_rows)
	{
		row_offset = rows - max_rows;
	}

	/* Make sure selected row is within bounds. */
	if (row_selected < 1)
	{
		row_selected = 1;
	}
	else if (row_selected >= rows)
	{
		row_selected = list->rows;
	}

	/* Set the values. */
	list->row_selected = row_selected;
	list->row_offset = row_offset;
}

/**
 * Handle one key press.
 * @param list List to do the keypress for.
 * @param key The key.
 * @return 1 to allow key repeating, 0 otherwise. */
static int list_handle_key(list_struct *list, SDLKey key)
{
	if (list->key_event_func)
	{
		int ret = list->key_event_func(list, key);

		if (ret != -1)
		{
			return ret;
		}
	}

	switch (key)
	{
		/* Up arrow. */
		case SDLK_UP:
			list_scroll(list, 1, 1);
			break;

		/* Down arrow. */
		case SDLK_DOWN:
			list_scroll(list, 0, 1);
			break;

		/* Page up. */
		case SDLK_PAGEUP:
			list_scroll(list, 1, list->max_rows);
			break;

		/* Page down. */
		case SDLK_PAGEDOWN:
			list_scroll(list, 0, list->max_rows);
			break;

		/* Esc, let the list creator handle this if they want to. */
		case SDLK_ESCAPE:
			if (list->handle_esc_func)
			{
				list->handle_esc_func(list);
			}

			return 0;

		/* Enter. */
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (list->handle_enter_func)
			{
				list->handle_enter_func(list);
			}

			return 0;

		/* Unhandled key. */
		default:
			return 0;
	}

	return 1;
}

/**
 * Handle keyboard event.
 * @param event Event.
 * @return 1 if we handled the event, 0 otherwise. */
int lists_handle_keyboard(SDL_KeyboardEvent *event)
{
	list_struct *list = list_get_focused();

	/* No list exists. */
	if (!list)
	{
		return 0;
	}

	if (list->surface != ScreenSurface)
	{
		return 0;
	}

	if (event->type == SDL_KEYDOWN)
	{
		/* Rotate between lists using tab. */
		if (event->keysym.sym == SDLK_TAB)
		{
			/* Go backwards? */
			if (event->keysym.mod & KMOD_SHIFT)
			{
				/* Previous list. */
				if (list->prev)
				{
					list_set_focus(list->prev);
				}
				/* Last one. */
				else
				{
					list_set_focus(list_tail);
				}
			}
			else
			{
				/* Next list exists? */
				if (list->next)
				{
					list_set_focus(list->next);
				}
				/* First one otherwise. */
				else
				{
					list_set_focus(list_head);
				}
			}

			return 1;
		}

		/* Handle the key. */
		if (list_handle_key(list, event->keysym.sym))
		{
			/* Store the pressed key and ticks for repeating. */
			list->repeat_key = event->keysym.sym;
			list->repeat_key_ticks = LastTick + KEY_REPEAT_DELAY_INIT;
		}

		return 1;
	}
	/* Key was released. */
	else if (event->type == SDL_KEYUP)
	{
		/* If the key is the one we stored previously, reset it. */
		if (event->keysym.sym == (SDLKey) list->repeat_key)
		{
			list->repeat_key = -1;
		}
	}

	return 0;
}

/**
 * Handle mouse events for one list. Checking whether the mouse is over
 * the list should have been done before calling this.
 * @param list The list.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param event Event.
 * @return 1 if the event was handled, 0 otherwise. */
int list_handle_mouse(list_struct *list, int mx, int my, SDL_Event *event)
{
	uint32 row;

	if (!LIST_MOUSE_OVER(list, mx, my) && !list->scrollbar_dragging)
	{
		return 0;
	}

	/* Left mouse button was pressed, update focused list. */
	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		list_set_focus(list);

		if (event->button.button == SDL_BUTTON_LEFT)
		{
			SDL_Rect scrollbar_box;

			if (list_scrollbar_get_size(list, &scrollbar_box) && mx > scrollbar_box.x && mx < scrollbar_box.x + scrollbar_box.w && my > scrollbar_box.y + list->scrollbar_y && my < scrollbar_box.y + list->scrollbar_y + list->scrollbar_h)
			{
				old_scrollbar_pos = event->motion.y - list->scrollbar_y;
				list->scrollbar_dragging = 1;
				return 1;
			}
		}
	}
	else if (event->type == SDL_MOUSEBUTTONUP)
	{
		list->scrollbar_dragging = 0;
		return 1;
	}
	else if (event->type == SDL_MOUSEMOTION)
	{
		if (list->scrollbar_dragging)
		{
			SDL_Rect scrollbar_box;

			if (!list_scrollbar_get_size(list, &scrollbar_box))
			{
				return 0;
			}

			list->scrollbar_y = event->motion.y - old_scrollbar_pos;

			if (list->scrollbar_y > scrollbar_box.h - list->scrollbar_h - 1)
			{
				list->scrollbar_y = scrollbar_box.h - list->scrollbar_h - 1;
			}

			list->row_offset = MAX(0, list->scrollbar_y) * list->rows / (LIST_ROW_HEIGHT(list) * list->max_rows - 1);
			list->row_selected = list->max_rows + list->row_offset - 1;
			return 1;
		}
	}

	if (mx >= list->x + list->width)
	{
		return 1;
	}

	/* No row is highlighted now. Will be switched back on as needed
	 * below. */
	list->row_highlighted = 0;

	/* Handle mouse wheel for scrolling. */
	if (event->button.button == SDL_BUTTON_WHEELUP || event->button.button == SDL_BUTTON_WHEELDOWN)
	{
		list_scroll(list, event->button.button == SDL_BUTTON_WHEELUP, 1);
	}

	/* See which row the mouse is over. */
	for (row = list->row_offset; row < list->rows; row++)
	{
		/* Stop if we reached maximum number of visible rows. */
		if (LIST_ROW_OFFSET(row, list) == list->max_rows)
		{
			break;
		}

		/* Is the mouse over this row? */
		if ((uint32) my > (LIST_ROWS_START(list) + LIST_ROW_OFFSET(row, list) * LIST_ROW_HEIGHT(list)) + list->frame_offset && (uint32) my < LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) + 1) * LIST_ROW_HEIGHT(list))
		{
			/* Mouse click? */
			if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
			{
				/* See if we clicked on this row earlier, and whether this
				 * should be considered a double click. */
				if (SDL_GetTicks() - list->click_tick < DOUBLE_CLICK_DELAY)
				{
					/* Double click, handle it as if enter was used. */
					if (list->handle_enter_func)
					{
						list->handle_enter_func(list);
						list->click_tick = 0;
					}

					/* Update selected row (in case enter handling
					 * function did not actually jump to another GUI,
					 * thus removing the need for this list). */
					list->row_selected = row + 1;
				}
				/* Normal click. */
				else
				{
					/* Update selected row and click ticks for above
					 * double click calculation. */
					list->row_selected = row + 1;
					list->click_tick = SDL_GetTicks();
				}
			}
			/* Not a mouse click, so update highlighted row. */
			else
			{
				list->row_highlighted = row + 1;
			}

			break;
		}
	}

	return 1;
}

/**
 * Handle mouse events such as mouse motion and mouse click.
 * @param mx Mouse X.
 * @param my Mouse Y.
 * @param event The event.
 * @return 1 if we handled the event (mouse was inside the list), 0
 * otherwise. */
int lists_handle_mouse(int mx, int my, SDL_Event *event)
{
	list_struct *tmp;

	for (tmp = list_head; tmp; tmp = tmp->next)
	{
		if (tmp->surface == ScreenSurface && list_handle_mouse(tmp, mx, my, event))
		{
			return 1;
		}
	}

	return 0;
}

/**
 * Updated Y position of lists after a resize event.
 * @param y_offset Y offset to add to (or subtract from) list's Y. */
void lists_handle_resize(int y_offset)
{
	list_struct *tmp;

	for (tmp = list_head; tmp; tmp = tmp->next)
	{
		if (tmp->surface == ScreenSurface)
		{
			tmp->y += y_offset;
		}
	}
}

/**
 * Utility function: checks if list with the specified ID already exists.
 * @param id List ID. One of @ref LIST_xxx.
 * @return Pointer to the list if it exists, NULL otherwise. */
list_struct *list_exists(uint32 id)
{
	list_struct *tmp;

	for (tmp = list_head; tmp; tmp = tmp->next)
	{
		if (tmp->id == id)
		{
			return tmp;
		}
	}

	return NULL;
}

/**
 * Used for alphabetical sorting in list_sort().
 * @param a What to compare.
 * @param b What to compare against.
 * @return Return value of strcmp() against the two entries. */
static int list_compare_alpha(const void *a, const void *b)
{
	return strcmp(((char ***) a)[0][0], ((char ***) b)[0][0]);
}

/**
 * Sort a list's entries.
 * @param list List to sort.
 * @param type How to sort, one of @ref LIST_SORT_xxx.
 * @note Sorting is done by looking at the first column of each row. */
void list_sort(list_struct *list, int type)
{
	/* Alphabetical sort. */
	if (type == LIST_SORT_ALPHA)
	{
		qsort((void *) list->text, list->rows, sizeof(*list->text), (void *) (int (*)()) list_compare_alpha);
	}
}

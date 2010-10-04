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
 * Generic lists implementation. */

#include <include.h>

/** Start of the visible lists. */
static list_struct *list_head = NULL;
/** End of the visible lists. */
static list_struct *list_tail = NULL;

static int list_handle_key(list_struct *list, SDLKey key);

/**
 * Draw a frame in which the rows will be drawn.
 * @param list List to draw the frame for. */
static void list_draw_frame(list_struct *list)
{
	draw_frame(list->x + list->frame_offset, LIST_ROWS_START(list) + list->frame_offset, list->width, list->height);
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
		SDL_FillRect(ScreenSurface, &box, sdl_gray2);
	}
	else
	{
		SDL_FillRect(ScreenSurface, &box, sdl_gray1);
	}
}

/**
 * Highlight a row (due to mouse being over it).
 * @param list List.
 * @param box Contains base x/y/width/height information to use. */
static void list_row_highlight(list_struct *list, SDL_Rect box)
{
	(void) list;
	SDL_FillRect(ScreenSurface, &box, sdl_dgreen);
}

/**
 * Color a selected row.
 * @param list List.
 * @param box Contains base x/y/width/height information to use. */
static void list_row_selected(list_struct *list, SDL_Rect box)
{
	(void) list;
	SDL_FillRect(ScreenSurface, &box, sdl_blue1);
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
 * @param height Maximum height of the list rows.
 * @param cols How many columns per row.
 * @param spacing Spacing between column names and the actual rows start.
 * @return The created list. */
list_struct *list_create(uint32 id, int x, int y, int height, uint32 cols, int spacing)
{
	list_struct *list = calloc(1, sizeof(list_struct));

	/* Store the values. */
	list->id = id;
	list->x = x;
	list->y = y;
	list->height = height;
	list->cols = cols;
	list->spacing = spacing;

	/* Initialize defaults. */
	list->frame_offset = -2;
	list->row_height = 12;
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
 * Show one list.
 * @param list List to show. */
void list_show(list_struct *list)
{
	uint32 row, col, max_rows;
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
 			extra_width = list->col_widths[col] / 2 - get_string_pixel_length(list->col_names[col], &SystemFont) / 2;
		}

		/* Actually draw the column name. */
		string_blt_shadow(ScreenSurface, &SystemFont, list->col_names[col], list->x + w + extra_width, list->y, COLOR_WHITE, COLOR_BLACK, 0, NULL);
		w += list->col_widths[col] + list->col_spacings[col];
	}

	/* Maximum visible rows. */
	max_rows = LIST_ROWS_MAX(list);

	/* Initialize default values for coloring rows. */
	box.x = list->x + list->frame_offset;
	box.w = list->width;
	box.h = list->row_height;

	/* Doing coloring of each row? */
	if (list->row_color_func)
	{
		for (row = 0; row < max_rows; row++)
		{
			box.y = LIST_ROWS_START(list) + (row * list->row_height) + list->frame_offset;
			list->row_color_func(list, row, box);
		}
	}

	/* Start printing out rows from the offset to the maximum. */
	for (row = list->row_offset; row < list->rows; row++)
	{
		/* Stop if we reached maximum number of visible rows. */
		if (LIST_ROW_OFFSET(row, list) == max_rows)
		{
			break;
		}

		/* Color selected row. */
		if (list->row_selected_func && (row + 1) == list->row_selected)
		{
			box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * list->row_height) + list->frame_offset;
			list->row_selected_func(list, box);
		}
		/* Color highlighted row. */
		else if (list->row_highlight_func && (row + 1) == list->row_highlighted)
		{
			box.y = LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * list->row_height) + list->frame_offset;
			list->row_highlight_func(list, box);
		}

		w = 0;

		/* Show all the columns. */
		for (col = 0; col < list->cols; col++)
		{
			/* Is there any text to show? */
			if (list->text[row][col])
			{
				extra_width = 0;

				/* Center it. */
				if (list->col_centered[col])
				{
 					extra_width = list->col_widths[col] / 2 - get_string_pixel_length(list->text[row][col], &SystemFont) / 2;
				}

				/* Output the text. */
				string_blt_shadow(ScreenSurface, &SystemFont, list->text[row][col], list->x + w + extra_width, LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) * list->row_height), COLOR_WHITE, COLOR_BLACK, 0, NULL);
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
	max_rows = LIST_ROWS_MAX(list);

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
 * @return 1 if we handled the key, 0 otherwise. */
static int list_handle_key(list_struct *list, SDLKey key)
{
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
			list_scroll(list, 1, LIST_ROWS_MAX(list));
			break;

		/* Page down. */
		case SDLK_PAGEDOWN:
			list_scroll(list, 0, LIST_ROWS_MAX(list));
			break;

		/* Esc, let the list creator handle this if they want to. */
		case SDLK_ESCAPE:
			if (list->handle_esc_func)
			{
				list->handle_esc_func(list);
			}

			break;

		/* Enter. */
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			if (list->handle_enter_func)
			{
				list->handle_enter_func(list);
			}

			break;

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

	if (event->type == SDL_KEYDOWN)
	{
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
 * @param event Event. */
static void list_handle_mouse(list_struct *list, int mx, int my, SDL_Event *event)
{
	uint32 row, rows_max;

	(void) mx;

	/* No row is highlighted now. Will be switched back on as needed
	 * below. */
	list->row_highlighted = 0;
	/* Get the maximum number of rows displayed. */
	rows_max = LIST_ROWS_MAX(list);

	/* Handle mouse wheel for scrolling. */
	if (event->button.button == SDL_BUTTON_WHEELUP || event->button.button == SDL_BUTTON_WHEELDOWN)
	{
		list_scroll(list, event->button.button == SDL_BUTTON_WHEELUP, 1);
	}

	/* See which row the mouse is over. */
	for (row = list->row_offset; row < rows_max + list->row_offset; row++)
	{
		/* Is the mouse over this row? */
		if ((uint32) my > (LIST_ROWS_START(list) + LIST_ROW_OFFSET(row, list) * list->row_height) + list->frame_offset && (uint32) my < LIST_ROWS_START(list) + (LIST_ROW_OFFSET(row, list) + 1) * list->row_height)
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
		/* Check whether the mouse is inside the list. */
		if (mx > tmp->x && mx < tmp->x + tmp->width && my > tmp->y && my < tmp->y + LIST_HEIGHT_FULL(tmp))
		{
			/* Left mouse button was pressed, update focused list. */
			if (event->type == SDL_MOUSEBUTTONDOWN)
			{
				list_set_focus(tmp);
			}

			/* Handle the actual mouse for this list. */
			list_handle_mouse(tmp, mx, my, event);
			return 1;
		}
	}

	return 0;
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

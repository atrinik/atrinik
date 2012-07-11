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
 * Header file for generic lists implementation. */

#ifndef LIST_H
#define LIST_H

/** One list. */
typedef struct list_struct
{
	/** X position of the list. */
	int x;

	/** Y position of the list. */
	int y;

	/**
	 * Parent X position, ie, X position of the surface the list is being
	 * drawn on. */
	int px;

	/**
	 * Parent Y position, ie, Y position of the surface the list is being
	 * drawn on. */
	int py;

	/** List's maximum width. */
	int width;

	/** Maximum number of visible rows. */
	uint32 max_rows;

	/** Number of rows. */
	uint32 rows;

	/** Number of columns in a row. */
	uint32 cols;

	/** Spacing between column names and the actual rows start. */
	int spacing;

	/** An array of the column widths. */
	uint32 *col_widths;

	/** An array of the column spacings. */
	int *col_spacings;

	/** An array of pointers to the column names. */
	char **col_names;

	/** An array of which columns are centered. */
	uint8 *col_centered;

	/**
	 * Array of arrays of pointers to the text. In other words:
	 *
	 * row -> col -> text. */
	char ***text;

	/** How many pixels to adjust the height of a row by. */
	sint16 row_height_adjust;

	/**
	 * Frame offset (used when drawing the frame around the rows and when
	 * coloring the row entries). */
	sint16 frame_offset;

	/** Height of the header with column names. */
	uint16 header_height;

	/**
	 * Currently highlighted row ID + 1, therefore, 0 means no
	 * highlighted row. */
	uint32 row_highlighted;

	/**
	 * Currently selected row ID + 1, therefore, 0 means no selected
	 * row. */
	uint32 row_selected;

	/**
	 * Row offset used for scrolling.
	 *
	 * - 0 = Row #0 is shown first in the list.
	 * - 10 = Row #10 is shown first in the list. */
	uint32 row_offset;

	/**
	 * Used for figuring out whether a double click occurred (keeps last
	 * ticks value). */
	uint32 click_tick;

	/** If 1, this list has the active focus. */
	uint8 focus;

	/** Does the list use scrollbars? */
	uint8 scrollbar_enabled;

	/** The scrollbar. */
	scrollbar_struct scrollbar;

	/** Font used, one of @ref FONT_xxx. Default is @ref FONT_SANS10. */
	int font;

	/** Surface used to draw the list on. */
	SDL_Surface *surface;

	/** Additional text API @ref TEXT_xxx "flags". */
	uint64 text_flags;

	/**
	 * Pointer to some custom data. If non-NULL, will be freed when list
	 * is destroyed. */
	void *data;

	/**
	 * Function that will draw frame (and/or other effects) right before
	 * the column names and the actual rows.
	 * @param list List. */
	void (*draw_frame_func)(struct list_struct *list);

	/**
	 * Function that will color the specified row.
	 * @param list List.
	 * @param row Row number, 0-[max visible rows].
	 * @param box Contains base x/y/width/height information to use. */
	void (*row_color_func)(struct list_struct *list, int row, SDL_Rect box);

	/**
	 * Function to highlight a row (due to mouse being over it).
	 * @param list List.
	 * @param box Contains base x/y/width/height information to use. */
	void (*row_highlight_func)(struct list_struct *list, SDL_Rect box);

	/**
	 * Function to color a selected row.
	 * @param list List.
	 * @param box Contains base x/y/width/height information to use. */
	void (*row_selected_func)(struct list_struct *list, SDL_Rect box);

	/**
	 * Function to handle ESC key being pressed while the list had focus.
	 * @param list List. */
	void (*handle_esc_func)(struct list_struct *list);

	/**
	 * Function to handle enter key being pressed on a selected row, or
	 * a row being double clicked.
	 * @param list List.
	 * @param Event Event that triggered this. */
	void (*handle_enter_func)(struct list_struct *list, SDL_Event *event);

	/**
	 * Custom function to call for handling keyboard events.
	 * @param list List.
	 * @param key Key ID.
	 * @retval -1 Did not handle the event, but should still attempt to
	 * handle generic list events (eg, scrolling with arrow keys).
	 * @retval 0 Did not handle the event.
	 * @retval 1 Handled the event. */
	int (*key_event_func)(struct list_struct *list, SDLKey key);

	/**
	 * Hook to use for setting text color based on row/column.
	 * @param list List.
	 * @param row Text row.
	 * @param col Column.
	 * @param[out] color What color to use.
	 * @param[out] color_shadow What color to use for the text's shadow,
	 * NULL to disable shadow. */
	void (*text_color_hook)(struct list_struct *list, uint32 row, uint32 col, const char **color, const char **color_shadow);

	/**
	 * Callback function to call after drawing one column in a list.
	 * @param list The list.
	 * @param row The row of the column that was drawn.
	 * @param col The column. */
	void (*post_column_func)(struct list_struct *list, uint32 row, uint32 col);

	/**
	 * Callback function to call when a mouse has been detected to be
	 * located over a list row.
	 * @param list The list.
	 * @param row The row in the list the mouse is over.
	 * @param event Event that triggered this - can be used to figure out
	 * whether the event was a click, a motion, etc. */
	void (*handle_mouse_row_func)(struct list_struct *list, uint32 row, SDL_Event *event);
} list_struct;

/** Calculate list's row height. */
#define LIST_ROW_HEIGHT(list) (((list)->font != -1 ? FONT_HEIGHT((list)->font) : 0) + (list)->row_height_adjust)
/** Figure out Y position where rows should actually start. */
#define LIST_ROWS_START(list) ((list)->y + (list)->header_height + (list)->spacing + (list)->frame_offset)
/** Figure out maximum visible rows. */
#define LIST_ROWS_MAX(list) ((uint32) ((list)->height + (list)->spacing) / LIST_ROW_HEIGHT((list)))
/** Calculate the height of the rows. */
#define LIST_ROWS_HEIGHT(list) (LIST_ROW_HEIGHT((list)) * (list)->max_rows)
/**
 * Adjust row ID by the row offset, thus transforming row ID to
 * 0-[max visible rows]. */
#define LIST_ROW_OFFSET(row, list) ((row) - (list)->row_offset)
/**
 * Figure out full height of the list, including its header. */
#define LIST_HEIGHT_FULL(list) ((int) LIST_ROWS_HEIGHT((list)) + (list)->spacing + (list)->header_height)
/**
 * Figure out the full width of the list, including its scrollbar, if it
 * has one. */
#define LIST_WIDTH_FULL(list) ((list)->width + ((list)->scrollbar_enabled ? (list)->scrollbar.background.w : 0))
/** Calculate whether mouse is over the specified list. */
#define LIST_MOUSE_OVER(list, mx, my) ((mx) > (list)->x && (mx) < (list)->x + LIST_WIDTH_FULL((list)) && (my) > (list)->y && (my) < (list)->y + LIST_HEIGHT_FULL((list)))

/**
 * @defgroup LIST_SORT_xxx List sort types
 * List sort types.
 *@{*/
/** Alphabetical sort. */
#define LIST_SORT_ALPHA 1
/*@}*/

/** Double click delay in ticks. */
#define DOUBLE_CLICK_DELAY 300

#endif

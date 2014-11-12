/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Text window header file. */

#ifndef TEXTWIN_H
#define TEXTWIN_H

#include <button.h>

#define TEXTWIN_TAB_NAME(_tab) ((_tab)->name ? (_tab)->name : textwin_tab_names[(_tab)->type - 1])

typedef struct textwin_tab_struct
{
    uint8 type;

    char *name;

    char *entries;

    size_t entries_size;

    /** Scroll offset. */
    uint32 scroll_offset;

    /** Number of lines. */
    uint32 num_lines;

    button_struct button;

    char *charnames;

    text_input_struct text_input;

    text_input_history_struct *text_input_history;
} textwin_tab_struct;

/** Custom attributes for text window widgets. */
typedef struct textwin_struct
{
    /** Font used. */
    font_struct *font;

    /** The scrollbar. */
    scrollbar_struct scrollbar;

    /** Whether there is anything in selection_start yet. */
    uint8 selection_started;

    /** Start of selection. */
    sint64 selection_start;

    /** End of selection. */
    sint64 selection_end;

    struct textwin_tab_struct *tabs;

    size_t tabs_num;

    size_t tab_selected;

    uint8 timestamps;
} textwin_struct;

#define TEXTWIN_TAB_HEIGHT 20

/**
 * @defgroup TEXTWIN_TEXT_xxx Textwin text coordinates
 * Coordinates used for the text in text window widgets.
 *@{*/
/** Text starting X position. */
#define TEXTWIN_TEXT_STARTX(_widget) (3)
/** Text starting Y position. */
#define TEXTWIN_TEXT_STARTY(_widget) (1)
/** Maximum width of the text in the widget. */
#define TEXTWIN_TEXT_WIDTH(_widget) ((_widget)->w - scrollbar_get_width(&TEXTWIN((_widget))->scrollbar) - (TEXTWIN_TEXT_STARTX((_widget)) * 2))
/** Maximum height of the text in the widget. */
#define TEXTWIN_TEXT_HEIGHT(_widget) ((_widget)->h - (TEXTWIN_TEXT_STARTY((_widget)) * 2) - textwin_tabs_height((_widget)) - (TEXTWIN((_widget))->tabs_num != 0 && textwin_tab_commands[TEXTWIN((_widget))->tabs[TEXTWIN((_widget))->tab_selected].type - 1] ? TEXTWIN((_widget))->tabs[TEXTWIN((_widget))->tab_selected].text_input.coords.h : 0))
/*@}*/

#define TEXTWIN_TEXT_INPUT_STARTX(_widget) (1)
#define TEXTWIN_TEXT_INPUT_STARTY(_widget) (TEXTWIN_TEXT_STARTY((_widget)) + TEXTWIN_TEXT_HEIGHT((_widget)))
#define TEXTWIN_TEXT_INPUT_WIDTH(_widget) ((_widget)->w - TEXTWIN_TEXT_INPUT_STARTX((_widget)) * 2 - TEXTWIN_SCROLLBAR_WIDTH((_widget)))

#define TEXTWIN_SCROLLBAR_WIDTH(_widget) (9)
#define TEXTWIN_SCROLLBAR_HEIGHT(_widget) ((_widget)->h - (TEXTWIN_TEXT_STARTY((_widget)) * 2) - textwin_tabs_height((_widget)))

/** Get the maximum number of visible rows. */
#define TEXTWIN_ROWS_VISIBLE(widget) (TEXTWIN_TEXT_HEIGHT((widget)) / FONT_HEIGHT(TEXTWIN((widget))->font))
/** Get the base flags depending on the text window. */
#define TEXTWIN_TEXT_FLAGS(widget) (TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_NO_FONT_CHANGE)

#endif

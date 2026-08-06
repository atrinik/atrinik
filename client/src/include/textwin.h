/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * Text window header file.
 */

#ifndef TEXTWIN_H
#define TEXTWIN_H

#include <button.h>

#define TEXTWIN_TAB_NAME(_tab) ((_tab)->name ? (_tab)->name : textwin_tab_names[(_tab)->type - 1])

typedef struct textwin_tab_struct {
    uint8_t type;

    char *name;

    char *entries;

    size_t entries_size;

    /** Scroll offset. */
    uint32_t scroll_offset;

    /** Number of lines. */
    uint32_t num_lines;

    button_struct button;

    char *charnames;

    text_input_struct text_input;

    text_input_history_struct *text_input_history;

    unsigned int unread : 1;
} textwin_tab_struct;

/** Custom attributes for text window widgets. */
typedef struct textwin_struct {
    /** Font used. */
    font_struct *font;

    /** The scrollbar. */
    scrollbar_struct scrollbar;

    /** Whether there is anything in selection_start yet. */
    uint8_t selection_started;

    /** Start of selection. */
    int64_t selection_start;

    /** End of selection. */
    int64_t selection_end;

    struct textwin_tab_struct *tabs;

    size_t tabs_num;

    size_t tab_selected;

    uint8_t timestamps;
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
#define TEXTWIN_TEXT_WIDTH(_widget)                                       \
    ((_widget)->w - scrollbar_get_width(&TEXTWIN((_widget))->scrollbar) - \
     (TEXTWIN_TEXT_STARTX((_widget)) * 2))
/** Maximum height of the text in the widget. */
#define TEXTWIN_TEXT_HEIGHT(_widget)                                                        \
    ((_widget)->h - (TEXTWIN_TEXT_STARTY((_widget)) * 2) - textwin_tabs_height((_widget)) - \
     (TEXTWIN((_widget))->tabs_num != 0 &&                                                  \
              textwin_tab_commands                                                          \
                  [TEXTWIN((_widget))->tabs[TEXTWIN((_widget))->tab_selected].type - 1]     \
          ? TEXTWIN((_widget))->tabs[TEXTWIN((_widget))->tab_selected].text_input.coords.h  \
          : 0))
/*@}*/

#define TEXTWIN_TEXT_INPUT_STARTX(_widget) (1)
#define TEXTWIN_TEXT_INPUT_STARTY(_widget) \
    (TEXTWIN_TEXT_STARTY((_widget)) + TEXTWIN_TEXT_HEIGHT((_widget)))
#define TEXTWIN_TEXT_INPUT_WIDTH(_widget) \
    ((_widget)->w - TEXTWIN_TEXT_INPUT_STARTX((_widget)) * 2 - TEXTWIN_SCROLLBAR_WIDTH((_widget)))

#define TEXTWIN_SCROLLBAR_WIDTH(_widget) (9)
#define TEXTWIN_SCROLLBAR_HEIGHT(_widget) \
    ((_widget)->h - (TEXTWIN_TEXT_STARTY((_widget)) * 2) - textwin_tabs_height((_widget)))

/** Get the maximum number of visible rows. */
#define TEXTWIN_ROWS_VISIBLE(widget) \
    (TEXTWIN_TEXT_HEIGHT((widget)) / FONT_HEIGHT(TEXTWIN((widget))->font))
/** Get the base flags depending on the text window. */
#define TEXTWIN_TEXT_FLAGS(widget) (TEXT_WORD_WRAP | TEXT_MARKUP | TEXT_NO_FONT_CHANGE)

/** Public API implemented in src/gui/widgets/textwin.c. */

extern const char *const textwin_tab_names[];

extern const char *const textwin_tab_commands[];

extern void textwin_readjust(widgetdata *widget);

extern size_t textwin_tab_name_to_id(const char *name);

extern void textwin_tab_free(textwin_tab_struct *tab);

extern void textwin_tab_remove(widgetdata *widget, const char *name);

extern void textwin_tab_add(widgetdata *widget, const char *name);

extern int textwin_tab_find(widgetdata *widget, uint8_t type, const char *name, size_t *id);

extern void textwin_tab_open(widgetdata *widget, const char *name);

extern void draw_info_tab(size_t type, const char *color, const char *str);

extern void draw_info_format(const char *color, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

extern void draw_info(const char *color, const char *str);

extern void textwin_handle_copy(widgetdata *widget);

extern void textwin_show(SDL_Surface *surface, int x, int y, int w, int h);

extern int textwin_tabs_height(widgetdata *widget);

extern void textwin_create_scrollbar(widgetdata *widget);

extern void widget_textwin_init(widgetdata *widget);

extern void widget_xp_tracker_init(widgetdata *widget);

extern void widget_textwin_handle_console(const char *text);

#endif

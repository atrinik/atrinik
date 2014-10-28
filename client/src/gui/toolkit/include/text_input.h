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
 * Header file for text input API.
 *
 * @author Alex Tokar */

#ifndef TEXT_INPUT_H
#define TEXT_INPUT_H

typedef struct text_input_history_struct
{
    /**
     * The history. */
    UT_array *history;

    /**
     * Position in the text input history -- used when browsing through the
     * history. */
    size_t pos;
} text_input_history_struct;

/**
 * Text input structure. */
typedef struct text_input_struct
{
    /**
     * The text input string. */
    char str[HUGE_BUF];

    /**
     * Text input string being edited. */
    char str_editing[HUGE_BUF];

    /**
     * Position of the cursor in the input string. */
    size_t pos;

    /**
     * Number of charactes in the input string. */
    size_t num;

    /**
     * Maximum number of allowed characters in the input string. */
    size_t max;

    /**
     * History. */
    text_input_history_struct *history;

    /**
     * Font to use. */
    font_struct *font;

    /**
     * Text flags. */
    int text_flags;

    /**
     * Coordinates of the text input.
     * @warning Don't change this directly - use text_input_set_dimensions(). */
    SDL_Rect coords;

    /**
     * Parent X. */
    int px;

    /**
     * Parent Y. */
    int py;

    /**
     * If 1, the text input has focus. */
    uint8 focus;

    int (*character_check_func)(struct text_input_struct *text_input, char c);

    void (*show_edit_func)(struct text_input_struct *text_input);
} text_input_struct;

#define TEXT_INPUT_PADDING 2
#define TEXT_INPUT_BORDER 1

#endif

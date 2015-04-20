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
 * Interface header file. */

#ifndef INTERFACE_H
#define INTERFACE_H

/**
 * Interface data. */
typedef struct interface_struct {
    /** Message contents. */
    char *message;

    /** Title text. */
    char *title;

    /** Icon name. */
    char *icon;

    /** Text to prepend to the text input string when sending it to NPC. */
    char *text_input_prepend;

    /** Font used. */
    font_struct *font;

    /** Array of the shortcut-supporting links. */
    UT_array *links;

    /** Whether the interface should be destroyed. */
    uint8_t destroy;

    /** Scroll offset. */
    uint32_t scroll_offset;

    /** Number of lines. */
    uint32_t num_lines;

    /** Scrollbar. */
    scrollbar_struct scrollbar;

    /** Whether the user has progressed through the dialog. */
    uint8_t progressed;

    /**
     * If progressed, how long until another progression may happen
     * (unless a new dialog has been opened of course, in which case this
     * is reset). */
    uint32_t progressed_ticks;

    /** Whether to allow entering tabs. */
    uint8_t allow_tab;

    /** If 1, disable cleaning up text input string. */
    uint8_t input_cleanup_disable;

    /** If 1, allow sending empty text input string. */
    uint8_t input_allow_empty;

    /** If 1, enable text input. */
    uint8_t text_input;

    /**
     * Text to prefix for autocompleting text. If NULL, autocompletion
     * will be disabled. */
    char *text_autocomplete;

    /**
     * Animated object.
     */
    object *anim;

    /**
     * Ticks of the last animation.
     */
    uint32_t last_anim;
} interface_struct;

/**
 * @defgroup INTERFACE_ICON_xxx Interface icon coords
 * Interface icon coordinates.
 *@{*/
/** X position of the icon. */
#define INTERFACE_ICON_STARTX 8
/** Y position of the icon. */
#define INTERFACE_ICON_STARTY 8
/** Width of the icon. */
#define INTERFACE_ICON_WIDTH 55
/** Height of the icon. */
#define INTERFACE_ICON_HEIGHT 55
/*@}*/

/**
 * @defgroup INTERFACE_TEXT_xxx Interface text coords
 * Interface text coordinates.
 *@{*/
/** X position of the text. */
#define INTERFACE_TEXT_STARTX 10
/** Y position of the text. */
#define INTERFACE_TEXT_STARTY 73
/** Maximum width of the text. */
#define INTERFACE_TEXT_WIDTH 412
/** Maximum height of the text. */
#define INTERFACE_TEXT_HEIGHT 430
/*@}*/

/**
 * @defgroup INTERFACE_TITLE_xxx Interface title coords
 * Interface title coordinates.
 *@{*/
/** X position of the title. */
#define INTERFACE_TITLE_STARTX 80
/** Y position of the title. */
#define INTERFACE_TITLE_STARTY 38
/** Maximum width of the title. */
#define INTERFACE_TITLE_WIDTH 350
/** Maximum height of the title. */
#define INTERFACE_TITLE_HEIGHT 22
/*@}*/

/**
 * @defgroup INTERFACE_BUTTON_xxx Interface button coords
 * Interface button coordinates.
 *@{*/
/** X position of the 'hello' button. */
#define INTERFACE_BUTTON_HELLO_STARTX 7
/** Y position of the 'hello' button. */
#define INTERFACE_BUTTON_HELLO_STARTY 512

/** X position of the 'close' button. */
#define INTERFACE_BUTTON_CLOSE_STARTX 337
/** Y position of the 'close' button. */
#define INTERFACE_BUTTON_CLOSE_STARTY 512
/*@}*/

/**
 * How many ticks must pass before the user may use the hello button
 * again or click a link again in the same dialog. */
#define INTERFACE_PROGRESSED_TICKS 125

#endif

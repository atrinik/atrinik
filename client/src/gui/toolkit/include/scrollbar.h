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
 * Scrollbar header file.
 */

#ifndef SCROLLBAR_H
#define SCROLLBAR_H

/**
 * Scrollbar element.
 */
typedef struct scrollbar_element {
    /** X position. */
    int x;

    /** Y position. */
    int y;

    /** Width. */
    int w;

    /** Height. */
    int h;

    /** Highlight status; if 1, the mouse is over this element. */
    uint8_t highlight;

    /**
     * Rendering function of this element.
     * @param surface
 * Surface to render on.
     * @param box
 * Where to draw.
     * @param elem
 * The element.
     * @param horizontal
 * Whether the scrollbar is horizontal.
 */
    void (*render_func)(SDL_Surface *surface, SDL_Rect *box, struct scrollbar_element *element, uint8_t horizontal);
} scrollbar_element;

/**
 * Holds scrollbar information.
 */
typedef struct scrollbar_struct {
    /** Pointer to the scroll offset. */
    uint32_t *scroll_offset;

    /** Pointer to number of lines. */
    uint32_t *num_lines;

    /** Maximum number of lines. */
    uint32_t max_lines;

    /** How much the arrows will adjust scroll offset. */
    int arrow_adjust;

    /**
     * Pointer that will be updated if redraw should be done due to
     * scroll offset change. Can be NULL, in which case it will not be
     * updated.
 */
    uint8_t *redraw;

    /** X position of the scrollbar. */
    int x;

    /** Y position of the scrollbar. */
    int y;

    /** Parent X position. */
    int px;

    /** Parent Y position. */
    int py;

    /** Slider position. */
    int old_slider_pos;

    /** Whether the slider is being dragged. */
    uint8_t dragging;

    /** Used to keep track of when to repeat a click. */
    uint32_t click_ticks;

    /** How many ticks must pass before click repeat can occur. */
    uint32_t click_repeat_ticks;

    /** Scrolling direction, one of @ref SCROLL_DIRECTION_xxx. */
    int scroll_direction;

    /** The background. */
    scrollbar_element background;

    /** Up arrow. */
    scrollbar_element arrow_up;

    /** Down arrow. */
    scrollbar_element arrow_down;

    /** The slider. */
    scrollbar_element slider;
} scrollbar_struct;

/**
 * Optional structure that can be used for storing scrollbar
 * information.
 */
typedef struct scrollbar_info_struct {
    /** @copydoc scrollbar_struct::scroll_offset */
    uint32_t scroll_offset;

    /** @copydoc scrollbar_struct::num_lines */
    uint32_t num_lines;

    /** @copydoc scrollbar_struct::redraw */
    uint8_t redraw;
} scrollbar_info_struct;

/**
 * @defgroup SCROLL_DIRECTION_xxx Scroll directions
 * Scroll directions.
 *@{
 */
/** No direction. */
#define SCROLL_DIRECTION_NONE 0
/** Scrolling up. */
#define SCROLL_DIRECTION_UP 1
/** Scrolling down. */
#define SCROLL_DIRECTION_DOWN 2
/*@}*/

/** Get the scroll offset needed to reach the bottom of the scrollbar. */
#define SCROLL_BOTTOM(_scrollbar) ((uint32_t) (MAX(0, (int) *(_scrollbar)->num_lines - (int) (_scrollbar)->max_lines)))
/** Get the scroll offset needed to reach the top of the scrollbar. */
#define SCROLL_TOP(_scrollbar) (0)

#endif

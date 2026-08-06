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
 * Color picker API header file.
 *
 * @author Zoey Rose
 */

#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H

enum {
    /**
     * The color chooser.
     */
    COLOR_PICKER_ELEM_COLOR,
    /**
     * Hue chooser.
     */
    COLOR_PICKER_ELEM_HUE,

    /**
     * Number of elements.
     */
    COLOR_PICKER_ELEM_NUM
};

/**
 * One color picker element.
 */
typedef struct color_picker_element_struct {
    /**
     * Dimensions.
     */
    SDL_Rect coords;

    /**
     * If 1, this element is being dragged.
     */
    uint8_t dragging;
} color_picker_element_struct;

/**
 * Color picker structure.
 */
typedef struct color_picker_struct {
    /**
     * X position of the color picker.
     */
    int x;

    /**
     * Y position of the color picker.
     */
    int y;

    /**
     * X position of color picker's parent.
     */
    int px;

    /**
     * Y position of color picker's parent.
     */
    int py;

    /**
     * The elements.
     */
    color_picker_element_struct elements[COLOR_PICKER_ELEM_NUM];

    /**
     * Thickness of the border; 0 for none.
     */
    uint8_t border_thickness;

    /**
     * Which color is currently selected, in HSV (hue,saturation,value)
     * colorspace.
     */
    double hsv[3];

    void (*callback_func)(struct color_picker_struct *color_picker);
} color_picker_struct;

/** Public API implemented in src/gui/popups/color_chooser.c. */

extern color_picker_struct *color_chooser_open(void);

/** Public API implemented in src/gui/toolkit/color_picker.c. */

extern void color_picker_create(color_picker_struct *color_picker, int size);

extern void color_picker_set_parent(color_picker_struct *color_picker, int px, int py);

extern void color_picker_set_notation(color_picker_struct *color_picker,
                                      const char *color_notation);

extern void
color_picker_get_rgb(color_picker_struct *color_picker, uint8_t *r, uint8_t *g, uint8_t *b);

extern void color_picker_show(SDL_Surface *surface, color_picker_struct *color_picker);

extern int color_picker_event(color_picker_struct *color_picker, SDL_Event *event);

extern int color_picker_mouse_over(color_picker_struct *color_picker, int mx, int my);

#endif

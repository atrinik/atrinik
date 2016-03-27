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
 * Color picker API.
 *
 * @author Alex Tokar
 */

#include <global.h>

/**
 * Create a new color picker.
 * @param color_picker
 * Color picker that will be initialized.
 * @param size
 * Size of the color picker. For best results, use a nice
 * round number such as 50, 100, 150, etc.
 */
void color_picker_create(color_picker_struct *color_picker, int size)
{
    size_t i;

    memset(color_picker, 0, sizeof(*color_picker));

    color_picker->border_thickness = 1;

    for (i = 0; i < COLOR_PICKER_ELEM_NUM; i++) {
        color_picker->elements[i].coords.w = color_picker->border_thickness * 2;
        color_picker->elements[i].coords.h = color_picker->border_thickness * 2;
    }

    color_picker->elements[0].coords.x = 0;
    color_picker->elements[0].coords.y = 0;
    color_picker->elements[0].coords.w += size + 1;
    color_picker->elements[0].coords.h += size + 1;

    color_picker->elements[1].coords.x = color_picker->elements[0].coords.w;
    color_picker->elements[1].coords.y = 0;
    color_picker->elements[1].coords.w += size / 10 + 1;
    color_picker->elements[1].coords.h += size + 1;
}

/**
 * Set the color picker's parent x/y positions.
 * @param color_picker
 * Color picker.
 * @param px
 * Parent X.
 * @param py
 * Parent Y.
 */
void color_picker_set_parent(color_picker_struct *color_picker, int px, int py)
{
    color_picker->px = px;
    color_picker->py = py;
}

/**
 * Set color picker's color, using HTML color notation.
 * @param color_picker
 * Color picker.
 * @param color_notation
 * The notation to set.
 */
void color_picker_set_notation(color_picker_struct *color_picker, const char *color_notation)
{
    SDL_Color color;
    double rgb[3];

    if (!text_color_parse(color_notation, &color)) {
        LOG(BUG, "Invalid color: %s", color_notation);
        return;
    }

    rgb[0] = 1.0 * ((double) color.r / 255.0);
    rgb[1] = 1.0 * ((double) color.g / 255.0);
    rgb[2] = 1.0 * ((double) color.b / 255.0);
    colorspace_rgb2hsv(rgb, color_picker->hsv);
}

/**
 * Get color picker's currently selected color as RGB.
 * @param color_picker
 * Color picker.
 * @param[out] r Will contain the red value.
 * @param[out] g Will contain the green value.
 * @param[out] b Will contain the blue value.
 */
void color_picker_get_rgb(color_picker_struct *color_picker, uint8_t *r, uint8_t *g, uint8_t *b)
{
    double rgb[3];

    colorspace_hsv2rgb(color_picker->hsv, rgb);

    *r = 255 * rgb[0];
    *g = 255 * rgb[1];
    *b = 255 * rgb[2];
}

/**
 * Show one color picker element.
 * @param surface
 * Surface to use. Can be NULL in the case that 'event' is
 * non-NULL.
 * @param color_picker
 * The color picker.
 * @param type
 * Which element to show.
 * @param event
 * If non-NULL, will check for mouse events inside the color
 * picker element.
 * @return
 * 1 if event is non-NULL and a mouse event was handled, 0
 * otherwise.
 */
static int color_picker_element_show(SDL_Surface *surface, color_picker_struct *color_picker, size_t type, SDL_Event *event)
{
    SDL_Rect box;
    int x, y, mx, my;
    double hsv[3], rgb[3];
    SDL_Rect dest;
    uint8_t r, g, b;

    box.x = color_picker->x + color_picker->elements[type].coords.x;
    box.y = color_picker->y + color_picker->elements[type].coords.y;
    box.w = color_picker->elements[type].coords.w;
    box.h = color_picker->elements[type].coords.h;

    mx = my = 0;

    /* Create the border, if applicable. */
    if (color_picker->border_thickness) {
        if (surface) {
            border_create_color(surface, &box, color_picker->border_thickness, "303030");
        }

        box.x += color_picker->border_thickness;
        box.y += color_picker->border_thickness;
        box.w -= color_picker->border_thickness * 2;
        box.h -= color_picker->border_thickness * 2;
    }

    /* Check for events. */
    if (event) {
        mx = event->motion.x - color_picker->px;
        my = event->motion.y - color_picker->py;

        /* If the element is being dragged, clamp the mouse x/y positions
         * to the element's dimensions. */
        if (color_picker->elements[type].dragging && event->type == SDL_MOUSEMOTION) {
            if (mx < box.x) {
                mx = box.x;
            }

            if (my < box.y) {
                my = box.y;
            }

            if (mx >= box.x + box.w) {
                mx = box.x + box.w - 1;
            }

            if (my >= box.y + box.h) {
                my = box.y + box.h - 1;
            }
        } else if (event->type == SDL_MOUSEMOTION) {
            return 0;
        }
    } else if (color_picker->elements[type].dragging && SDL_GetMouseState(NULL, NULL) != SDL_BUTTON_LEFT) {
        /* If the element says it's being dragged, but the mouse state says
         * otherwise, stop dragging. */
        color_picker->elements[type].dragging = 0;
    }

    if (type == COLOR_PICKER_ELEM_COLOR) {
        int selx, sely;
        uint32_t pixel;

        selx = 0;
        sely = 0;
        hsv[0] = color_picker->hsv[0];

        for (x = 0; x < box.w; x++) {
            hsv[2] = 1.0 * ((double) x / (box.w - 1));

            /* If this is the currently selected value, store the X coordinate.
             * */
            if (color_picker->hsv[2] >= hsv[2] && color_picker->hsv[2] < hsv[2] + (1.0 * (1.0 / (double) box.w))) {
                selx = x;
            }

            for (y = 0; y < box.h; y++) {
                hsv[1] = fabs(1.0 * ((double) y / (box.h - 1)) - 1.0);

                /* If this is the currently selected saturation, store the Y
                 * coordinate. */
                if (color_picker->hsv[1] >= hsv[1] && color_picker->hsv[1] < hsv[1] + (1.0 * (1.0 / (double) box.h))) {
                    sely = y;
                }

                colorspace_hsv2rgb(hsv, rgb);

                dest.x = box.x + x;
                dest.y = box.y + y;
                dest.w = 1;
                dest.h = 1;

                if (event) {
                    if (mx >= dest.x && my >= dest.y && mx < dest.x + dest.w && my < dest.y + dest.h) {
                        color_picker->hsv[1] = hsv[1];
                        color_picker->hsv[2] = hsv[2];
                        return 1;
                    }

                    continue;
                }

                putpixel(surface, dest.x, dest.y, SDL_MapRGB(surface->format, 255 * rgb[0], 255 * rgb[1], 255 * rgb[2]));
            }
        }

        /* Draw the crosshair for saturation/value. */
        if (surface) {
            for (x = 0; x < box.w; x++) {
                pixel = getpixel(surface, box.x + x, box.y + sely);
                SDL_GetRGB(pixel, surface->format, &r, &g, &b);
                putpixel(surface, box.x + x, box.y + sely, SDL_MapRGB(surface->format, 255 - r, 255 - g, 255 - b));
            }

            for (y = 0; y < box.h; y++) {
                pixel = getpixel(surface, box.x + selx, box.y + y);
                SDL_GetRGB(pixel, surface->format, &r, &g, &b);
                putpixel(surface, box.x + selx, box.y + y, SDL_MapRGB(surface->format, 255 - r, 255 - g, 255 - b));
            }
        }
    } else if (type == COLOR_PICKER_ELEM_HUE) {
        hsv[1] = hsv[2] = 1.0;

        for (y = 0; y < box.h; y++) {
            hsv[0] = fabs(1.0 * ((double) y / (double) (box.h - 1)) - 1.0);
            colorspace_hsv2rgb(hsv, rgb);

            dest.x = box.x;
            dest.y = box.y + y;
            dest.w = box.w;
            dest.h = 1;

            if (event) {
                if (mx >= dest.x && my >= dest.y && mx < dest.x + dest.w && my < dest.y + dest.h) {
                    color_picker->hsv[0] = hsv[0];
                    return 1;
                }

                continue;
            }

            r = 255 * rgb[0];
            g = 255 * rgb[1];
            b = 255 * rgb[2];

            /* If this is the currently selected hue, invert the colors. */
            if (color_picker->hsv[0] >= hsv[0] && color_picker->hsv[0] < hsv[0] + (1.0 * (1.0 / (double) box.h))) {
                r = 255 - r;
                g = 255 - g;
                b = 255 - b;
            }

            SDL_FillRect(surface, &dest, SDL_MapRGB(surface->format, r, g, b));
        }
    }

    return 0;
}

/**
 * Show the specified color picker.
 * @param surface
 * Surface to use.
 * @param color_picker
 * Color picker to show.
 */
void color_picker_show(SDL_Surface *surface, color_picker_struct *color_picker)
{
    size_t i;

    for (i = 0; i < COLOR_PICKER_ELEM_NUM; i++) {
        color_picker_element_show(surface, color_picker, i, NULL);
    }
}

/**
 * Handle an event inside the specified color picker.
 * @param color_picker
 * Color picker.
 * @param event
 * Event to handle.
 * @return
 * 1 if the event was handled, 0 otherwise.
 */
int color_picker_event(color_picker_struct *color_picker, SDL_Event *event)
{
    if ((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEMOTION) && event->button.button == SDL_BUTTON_LEFT) {
        size_t i;

        for (i = 0; i < COLOR_PICKER_ELEM_NUM; i++) {
            if (color_picker_element_show(NULL, color_picker, i, event)) {
                if (color_picker->callback_func) {
                    color_picker->callback_func(color_picker);
                }

                color_picker->elements[i].dragging = 1;
                return 1;
            }
        }
    }

    return 0;
}

/**
 * Check if mouse is over the specified color picker.
 * @param color_picker
 * Color picker.
 * @param mx
 * Mouse X.
 * @param my
 * Mouse Y.
 * @return
 * 1 if mx,my is over the color picker, 0 otherwise.
 */
int color_picker_mouse_over(color_picker_struct *color_picker, int mx, int my)
{
    size_t i;
    int x, y;

    mx -= color_picker->px;
    my -= color_picker->py;

    for (i = 0; i < COLOR_PICKER_ELEM_NUM; i++) {
        x = color_picker->x + color_picker->elements[i].coords.x;
        y = color_picker->y + color_picker->elements[i].coords.y;

        if (mx >= x && my >= y && mx < x + color_picker->elements[i].coords.w && my < y + color_picker->elements[i].coords.h) {
            return 1;
        }
    }

    return 0;
}

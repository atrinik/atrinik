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
 * Color chooser popup.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc popup_struct::draw_func */
static int popup_draw(popup_struct *popup)
{
    color_picker_struct *color_picker;
    double rgb[3];
    SDL_Rect box;

    color_picker = popup->custom_data;
    color_picker_set_parent(color_picker, popup->x, popup->y);
    color_picker->x = 10;
    color_picker->y = 20;
    color_picker_show(popup->surface, color_picker);

    colorspace_hsv2rgb(color_picker->hsv, rgb);
    box.w = 100;
    box.h = 200;
    text_show_format(popup->surface, FONT_ARIAL11, color_picker->x + 180, color_picker->y, COLOR_WHITE, TEXT_MARKUP | TEXT_WORD_WRAP, &box, "Preview:\n[bar=#%.2X%.2X%.2X 80 20][border=#303030 80 20]\n\nChoose your desired color, then dismiss this popup.", (int) (255 * rgb[0]), (int) (255 * rgb[1]), (int) (255 * rgb[2]));

    return 1;
}

/** @copydoc popup_struct::event_func */
static int popup_event(popup_struct *popup, SDL_Event *event)
{
    color_picker_struct *color_picker;

    color_picker = popup->custom_data;

    if (color_picker_event(color_picker, event)) {
        return 1;
    }

    return -1;
}

/**
 * Open a color chooser popup.
 * @return Allocated color picker. */
color_picker_struct *color_chooser_open(void)
{
    popup_struct *popup;
    color_picker_struct *color_picker;

    color_picker = malloc(sizeof(*color_picker));
    color_picker_create(color_picker, 150);

    popup = popup_create(texture_get(TEXTURE_TYPE_SOFTWARE, "rectangle:300,200;[bar=widget_bg][border=widget_border -1 -1 2]"));
    popup->draw_func = popup_draw;
    popup->event_func = popup_event;
    popup->custom_data = color_picker;
    popup->modal = 0;
    popup->destroy_on_switch = 1;
    popup->button_left.button.texture = NULL;
    popup->button_right.button.texture = texture_get(TEXTURE_TYPE_CLIENT, "button_round");
    popup->button_right.button.texture_over = texture_get(TEXTURE_TYPE_CLIENT, "button_round_over");
    popup->button_right.button.texture_pressed = texture_get(TEXTURE_TYPE_CLIENT, "button_round_down");
    popup->button_right.x += 7;

    return color_picker;
}

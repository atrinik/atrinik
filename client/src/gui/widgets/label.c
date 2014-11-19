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
 * Implements label type widgets.
 *
 * @author Alex Tokar
 * @author Daniel Liptrot */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    _widget_label *label;

    label = widget->subwidget;

    if (label->text) {
        text_show(ScreenSurface, label->font, label->text, widget->x, widget->y, label->color, TEXT_MARKUP, NULL);
    }
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
    _widget_label *label;

    label = widget->subwidget;

    if (label->text) {
        efree(label->text);
    }
}

/**
 * Initialize one label widget. */
void widget_label_init(widgetdata *widget)
{
    _widget_label *label;

    label = ecalloc(1, sizeof(*label));
    label->font = font_get("arial", 10);
    label->color = COLOR_WHITE;

    widget->draw_func = widget_draw;
    widget->deinit_func = widget_deinit;
    widget->subwidget = label;
}

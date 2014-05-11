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
 * Implements texture type widgets.
 *
 * @author Alex Tokar
 * @author Daniel Liptrot */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    _widget_texture *texture;

    texture = widget->subwidget;

    if (texture->texture) {
        surface_show(ScreenSurface, widget->x, widget->y, NULL, texture_surface(texture->texture));
    }
}

/**
 * Initialize one texture widget. */
void widget_texture_init(widgetdata *widget)
{
    _widget_texture *texture;

    texture = ecalloc(1, sizeof(*texture));

    widget->draw_func = widget_draw;
    widget->subwidget = texture;
}

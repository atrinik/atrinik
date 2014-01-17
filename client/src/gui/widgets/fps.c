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
 * Implements FPS type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/**
 * FPS widget data. */
typedef struct widget_fps_struct
{
    /**
     * Last time the FPS was calculated. */
    uint32 lasttime;

    /**
     * Current FPS. */
    uint32 current;

    /**
     * Number of frames drawn since last calculation. */
    uint32 frames;
} widget_fps_struct;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    widget_fps_struct *tmp;

    tmp = (widget_fps_struct *) widget->subwidget;

    if (widget->redraw) {
        char buf[MAX_BUF];

        snprintf(buf, sizeof(buf), "%d", tmp->current);
        text_show(widget->surface, FONT_ARIAL11, "fps:", 5, 4, COLOR_WHITE, 0, NULL);
        text_show(widget->surface, FONT_ARIAL11, buf, widget->w - 5 - text_get_width(FONT_ARIAL11, buf, 0), 4, COLOR_WHITE, 0, NULL);
    }
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
    widget_fps_struct *tmp;

    tmp = (widget_fps_struct *) widget->subwidget;
    tmp->frames++;

    if (tmp->lasttime < SDL_GetTicks() - 1000) {
        if (tmp->current != tmp->frames) {
            widget->redraw = 1;
        }

        tmp->lasttime = SDL_GetTicks();
        tmp->current = tmp->frames;
        tmp->frames = 0;
    }
}

/**
 * Initialize one FPS widget. */
void widget_fps_init(widgetdata *widget)
{
    widget_fps_struct *tmp;

    widget->draw_func = widget_draw;
    widget->background_func = widget_background;

    widget->subwidget = tmp = calloc(1, sizeof(*tmp));
    tmp->lasttime = SDL_GetTicks();
}

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
 * Implements map name type widgets.
 *
 * @author Alex Tokar
 */

#include <global.h>

typedef struct widget_mapname_struct {
    int alpha;
} widget_mapname_struct;

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    widget_mapname_struct *widget_mapname;
    SDL_Rect box;

    widget_mapname = widget->subwidget;

    box.w = widget->w;
    box.h = 0;
    text_show_format(ScreenSurface, MAP_NAME_FONT, widget->x, widget->y, COLOR_HGOLD, TEXT_MARKUP, &box, "[alpha=%d]%s[/alpha]", widget_mapname->alpha, MapData.name);
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget, int draw)
{
    widget_mapname_struct *widget_mapname;
    int alpha;

    widget_mapname = widget->subwidget;
    alpha = widget_mapname->alpha;

    if (MapData.name_fadeout_start || MapData.name[0] == '\0') {
        uint32_t time_passed;

        time_passed = SDL_GetTicks() - MapData.name_fadeout_start;

        if (time_passed > MAP_NAME_FADEOUT || MapData.name[0] == '\0') {
            if (MapData.name[0] != '\0') {
                alpha = MIN(255, 255 * ((double) (time_passed - MAP_NAME_FADEOUT) / MAP_NAME_FADEOUT));

                if (alpha == 255) {
                    MapData.name_fadeout_start = 0;
                }
            }

            if (MapData.name_new[0] != '\0') {
                strncpy(MapData.name, MapData.name_new, sizeof(MapData.name) - 1);
                MapData.name[sizeof(MapData.name) - 1] = '\0';

                resize_widget(widget, RESIZE_RIGHT, text_get_width(MAP_NAME_FONT, MapData.name, TEXT_MARKUP));
                resize_widget(widget, RESIZE_BOTTOM, text_get_height(MAP_NAME_FONT, MapData.name, TEXT_MARKUP));

                MapData.name_new[0] = '\0';
                widget->redraw = 1;
            }
        } else {
            alpha = 255 * (1.0 - (double) time_passed / MAP_NAME_FADEOUT);
        }
    } else if (MapData.name_new[0] != '\0') {
        if (strcmp(MapData.name_new, MapData.name) != 0) {
            MapData.name_fadeout_start = SDL_GetTicks();
            widget->redraw = 1;
        } else {
            MapData.name_new[0] = '\0';
        }
    }

    if (alpha != widget_mapname->alpha) {
        widget_mapname->alpha = alpha;
        widget->redraw = 1;
    }
}

/**
 * Initialize one mapname widget.
 */
void widget_mapname_init(widgetdata *widget)
{
    widget_mapname_struct *widget_mapname;

    widget_mapname = emalloc(sizeof(*widget_mapname));
    widget_mapname->alpha = 255;

    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->subwidget = widget_mapname;

}

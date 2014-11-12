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
 * Implements player info type widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect box;
    char buf[MAX_BUF];

    if (!widget->redraw) {
        return;
    }

    box.w = widget->w;
    box.h = widget->h;

    text_show_format(widget->surface, FONT_SERIF14, 4, 0, COLOR_WHITE, TEXT_VALIGN_CENTER | TEXT_OUTLINE, &box, "%1.2f", cpl.action_timer);
    text_show_format(widget->surface, FONT_SERIF14, 0, 0, COLOR_HGOLD, TEXT_ALIGN_CENTER | TEXT_VALIGN_CENTER | TEXT_OUTLINE | TEXT_MARKUP, &box, "[b]%s[/b]", cpl.name);

    snprintf(buf, sizeof(buf), "[b]%d[/b]", cpl.stats.level);
    text_show(widget->surface, FONT_SERIF14, buf, widget->w - 4 - text_get_width(FONT_SERIF14, buf, TEXT_MARKUP), 0, cpl.stats.level == s_settings->max_level ? COLOR_HGOLD : COLOR_WHITE, TEXT_MARKUP | TEXT_OUTLINE | TEXT_VALIGN_CENTER, &box);
}

/** @copydoc widgetdata::background_func */
static void widget_background(widgetdata *widget)
{
    static uint32 action_tick = 0;

    /* Pre-emptively tick down the skill delay timer */
    if (cpl.action_timer > 0) {
        if (LastTick - action_tick > 125) {
            cpl.action_timer -= (float) (LastTick - action_tick) / 1000.0f;

            if (cpl.action_timer < 0) {
                cpl.action_timer = 0;
            }

            action_tick = LastTick;
            WIDGET_REDRAW(widget);
        }
    } else {
        action_tick = LastTick;
    }
}

/**
 * Initialize one player info widget. */
void widget_playerinfo_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
}

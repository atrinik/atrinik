/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2026 Atrinik Development Team                    *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 ************************************************************************/

/** @file Experience-per-hour widget. */

#include <global.h>
#include <toolkit/datetime.h>
#include <toolkit/string.h>

static void widget_draw(widgetdata *widget) {
    if (!widget->redraw) {
        return;
    }

    SDL_Rect box = {.w = widget->w, .h = widget->h};
    char hourly[64];
    snprintf(VS(hourly), "%s", string_format_number_comma(telemetry_exp_per_hour()));

    text_show_format(widget->surface,
                     FONT_ARIAL11,
                     4,
                     0,
                     COLOR_WHITE,
                     TEXT_VALIGN_CENTER | TEXT_OUTLINE,
                     &box,
                     "XP/h: %s",
                     hourly);
}

static void widget_background(widgetdata *widget, int draw) {
    static uint64_t last_redraw_second;
    uint64_t now = datetime_monotonic_us() / 1000000;
    if (now != last_redraw_second) {
        last_redraw_second = now;
        WIDGET_REDRAW_ALL(XP_TRACKER_ID);
    }
}

static int widget_event(widgetdata *widget, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        char gained[64];
        snprintf(VS(gained), "%s", string_format_number_comma(telemetry_exp_gained()));
        uint64_t elapsed = telemetry_exp_elapsed_seconds();
        char buf[256];
        snprintf(VS(buf),
                 "Experience gained: %s\nElapsed: %02" PRIu64 ":%02" PRIu64 ":%02" PRIu64
                 "\nRight-click to reset.",
                 gained,
                 elapsed / 3600,
                 elapsed / 60 % 60,
                 elapsed % 60);
        tooltip_create(event_mouse_x(event), event_mouse_y(event), FONT_ARIAL11, buf);
        tooltip_multiline(220);
        tooltip_enable_delay(100);
        return 1;
    }

    return 0;
}

static void menu_reset(widgetdata *widget, widgetdata *menuitem, SDL_Event *event) {
    telemetry_exp_tracker_reset();
}

static int widget_menu_handle(widgetdata *widget, SDL_Event *event) {
    widgetdata *menu = create_menu(event_mouse_x(event), event_mouse_y(event), widget);
    widget_menu_standard_items(widget, menu);
    add_menuitem(menu, "Reset XP tracker", &menu_reset, MENU_NORMAL, 0);
    menu_finalize(menu);
    return 1;
}

void widget_xp_tracker_init(widgetdata *widget) {
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
    widget->menu_handle_func = widget_menu_handle;
}

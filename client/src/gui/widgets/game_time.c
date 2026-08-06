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

/** @file Synchronized in-game clock widget. */

#include <global.h>

static void widget_draw(widgetdata *widget) {
    if (!widget->redraw) {
        return;
    }

    SDL_Rect box = {.w = widget->w, .h = widget->h};
    uint64_t game_minutes;
    uint32_t millis_per_game_minute;
    char text[32] = "Game time: --:--";

    if (telemetry_game_time_get(&game_minutes, &millis_per_game_minute)) {
        unsigned hour = game_minutes / 60 % 24;
        unsigned minute = game_minutes % 60;
        snprintf(VS(text), "Game time: %02u:%02u", hour, minute);
    }

    text_show(widget->surface,
              FONT_ARIAL11,
              text,
              4,
              0,
              COLOR_WHITE,
              TEXT_VALIGN_CENTER | TEXT_OUTLINE,
              &box);
}

static void widget_background(widgetdata *widget, int draw) {
    static uint64_t displayed_minute = UINT64_MAX;
    uint64_t game_minutes;
    uint32_t millis_per_game_minute;
    if (telemetry_game_time_get(&game_minutes, &millis_per_game_minute) &&
        game_minutes != displayed_minute) {
        displayed_minute = game_minutes;
        WIDGET_REDRAW_ALL(GAME_TIME_ID);
    }
}

static int widget_event(widgetdata *widget, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        uint64_t game_minutes;
        uint32_t millis_per_game_minute;
        if (telemetry_game_time_get(&game_minutes, &millis_per_game_minute)) {
            char buf[160];
            snprintf(VS(buf),
                     "Day %" PRIu64 ", %02" PRIu64 ":%02" PRIu64
                     "\nOne game minute passes every %.2f real seconds.",
                     game_minutes / (UINT64_C(24) * 60) + 1,
                     game_minutes / 60 % 24,
                     game_minutes % 60,
                     millis_per_game_minute / 1000.0);
            tooltip_create(event_mouse_x(event), event_mouse_y(event), FONT_ARIAL11, buf);
            tooltip_multiline(260);
            tooltip_enable_delay(100);
        }
        return 1;
    }

    return 0;
}

void widget_game_time_init(widgetdata *widget) {
    widget->draw_func = widget_draw;
    widget->background_func = widget_background;
    widget->event_func = widget_event;
}

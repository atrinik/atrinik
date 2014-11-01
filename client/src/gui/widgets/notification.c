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
 * Implements notification type widgets.
 *
 * Similar to tooltips, but instead triggered by player actions. Such a
 * notification can even define an action to execute when the notification
 * is clicked, or if the notification has a keybinding shortcut assigned
 * to it, when the shortcut key is pressed (thus overriding normal
 * behavior of that particular shortcut).
 *
 * @author Alex Tokar */

#include <global.h>
#include <notification.h>

/**
 * The notification data. */
static notification_struct *notification = NULL;

/**
 * Destroy notification data. */
void notification_destroy(void)
{
    if (!notification) {
        return;
    }

    if (notification->action) {
        efree(notification->action);
    }

    if (notification->shortcut) {
        efree(notification->shortcut);
    }

    efree(notification);
    notification = NULL;
    cur_widget[NOTIFICATION_ID]->show = 0;
}

/**
 * Process notification's action, if any. */
static void notification_action_do(void)
{
    if (notification && notification->action) {
        /* Macro or command? */
        if (*notification->action == '?') {
            keybind_process_command(notification->action);
        }
        else {
            send_command_check(notification->action);
        }

        /* Done the action, destroy it... */
        notification_destroy();
    }
}

/**
 * Check whether notification should handle keybinding macro.
 * @param cmd Macro to check.
 * @return 1 if the notification handled the keybinding, 0 otherwise. */
int notification_keybind_check(const char *cmd)
{
    if (notification && notification->action && notification->shortcut && !strcmp(notification->shortcut, cmd)) {
        notification_action_do();
        return 1;
    }

    return 0;
}

/** @copydoc socket_command_struct::handle_func */
void socket_command_notification(uint8 *data, size_t len, size_t pos)
{
    int wd, ht;
    char type, *cp;
    SDL_Rect box;
    StringBuffer *sb;
    SDL_Color color;

    /* Destroy previous notification, if any. */
    notification_destroy();
    /* Show the widget... */
    cur_widget[NOTIFICATION_ID]->show = 1;
    SetPriorityWidget(cur_widget[NOTIFICATION_ID]);
    /* Create the data structure and initialize default values. */
    notification = ecalloc(1, sizeof(*notification));
    notification->start_ticks = SDL_GetTicks();
    notification->alpha = 255;
    notification->delay = NOTIFICATION_DEFAULT_DELAY;
    sb = stringbuffer_new();

    /* Parse the data. */
    while (pos < len) {
        type = packet_to_uint8(data, len, &pos);

        switch (type) {
            case CMD_NOTIFICATION_TEXT:
            {
                char message[HUGE_BUF];

                packet_to_string(data, len, &pos, message, sizeof(message));
                stringbuffer_append_string(sb, message);
                break;
            }

            case CMD_NOTIFICATION_ACTION:
            {
                char action[HUGE_BUF];

                packet_to_string(data, len, &pos, action, sizeof(action));
                notification->action = estrdup(action);
                break;
            }

            case CMD_NOTIFICATION_SHORTCUT:
            {
                char shortcut[HUGE_BUF];

                packet_to_string(data, len, &pos, shortcut, sizeof(shortcut));
                notification->shortcut = estrdup(shortcut);
                break;
            }

            case CMD_NOTIFICATION_DELAY:
                notification->delay = MAX(NOTIFICATION_DEFAULT_FADEOUT, packet_to_uint32(data, len, &pos));
                break;

            default:
                break;
        }
    }

    /* Shortcut specified, add the shortcut name to the notification
     * message. */
    if (notification->shortcut) {
        keybind_struct *keybind = keybind_find_by_command(notification->shortcut);

        if (keybind) {
            char key_buf[MAX_BUF];

            keybind_get_key_shortcut(keybind->key, keybind->mod, key_buf, sizeof(key_buf));
            string_toupper(key_buf);
            stringbuffer_append_printf(sb, " (click or [b]%s[/b])", key_buf);
        }
    }
    /* No shortcut, clicking is the best one can do... */
    else if (notification->action) {
        stringbuffer_append_string(sb, " (click)");
    }

    cp = stringbuffer_finish(sb);

    /* Calculate the maximum height the text will need. */
    box.x = 0;
    box.y = 0;
    box.w = NOTIFICATION_DEFAULT_WIDTH;
    box.h = 0;
    text_show(NULL, NOTIFICATION_DEFAULT_FONT, cp, 0, 0, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP | TEXT_HEIGHT, &box);
    ht = box.h;

    /* Calculate the maximum text width. */
    box.h = 0;
    text_show(NULL, NOTIFICATION_DEFAULT_FONT, cp, 0, 0, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP | TEXT_MAX_WIDTH, &box);
    wd = box.w;

    box.x = 0;
    box.y = 0;
    box.w = wd + 6;
    box.h = ht + 6;

    /* Update the notification widget width/height. */
    resize_widget(cur_widget[NOTIFICATION_ID], RESIZE_RIGHT, box.w);
    resize_widget(cur_widget[NOTIFICATION_ID], RESIZE_BOTTOM, box.h);

    if (cur_widget[NOTIFICATION_ID]->surface) {
        SDL_FreeSurface(cur_widget[NOTIFICATION_ID]->surface);
    }

    /* Create a new surface. */
    cur_widget[NOTIFICATION_ID]->surface = SDL_CreateRGBSurface(get_video_flags(), box.w, box.h, video_get_bpp(), 0, 0, 0, 0);

    /* Fill the surface with the background color. */
    if (text_color_parse("e6e796", &color)) {
        SDL_FillRect(cur_widget[NOTIFICATION_ID]->surface, &box, SDL_MapRGB(cur_widget[NOTIFICATION_ID]->surface->format, color.r, color.g, color.b));
    }

    /* Create a border. */
    border_create_color(cur_widget[NOTIFICATION_ID]->surface, &box, 1, "606060");

    /* Render the text. */
    box.w = wd;
    box.h = ht;
    text_show(cur_widget[NOTIFICATION_ID]->surface, NOTIFICATION_DEFAULT_FONT, cp, 3, 3, COLOR_BLACK, TEXT_MARKUP | TEXT_WORD_WRAP, &box);

    efree(cp);
}

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
{
    SDL_Rect dst;

    /* Nothing to render... */
    if (!notification) {
        return;
    }

    /* Update the widget's position to below map name. */
    widget->x = cur_widget[MAPNAME_ID]->x;
    widget->y = cur_widget[MAPNAME_ID]->y + cur_widget[MAPNAME_ID]->h;

    /* Check whether we should do fade out. */
    if (SDL_GetTicks() - notification->start_ticks > notification->delay - NOTIFICATION_DEFAULT_FADEOUT) {
        int fade;

        /* Calculate how far into the fading animation we are. */
        fade = SDL_GetTicks() - notification->start_ticks - (notification->delay - NOTIFICATION_DEFAULT_FADEOUT);

        /* Completed the fading animation? */
        if (fade > NOTIFICATION_DEFAULT_FADEOUT) {
            notification_destroy();
            return;
        }

        /* Adjust the alpha value... */
        notification->alpha = 255 * ((NOTIFICATION_DEFAULT_FADEOUT - fade) / (double) NOTIFICATION_DEFAULT_FADEOUT);
    }

    dst.x = widget->x;
    dst.y = widget->y;
    SDL_SetAlpha(widget->surface, SDL_SRCALPHA, notification->alpha);
    SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &dst);

    /* Do highlight. */
    if (widget_mouse_event.owner == widget && notification->action) {
        filledRectAlpha(ScreenSurface, dst.x, dst.y, dst.x + widget->w, dst.y + widget->h, 0xffffff3c);
    }
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        notification_action_do();
        return 1;
    }

    return 0;
}

/**
 * Initialize one notification widget. */
void widget_notification_init(widgetdata *widget)
{
    widget->draw_func = widget_draw;
    widget->event_func = widget_event;
}

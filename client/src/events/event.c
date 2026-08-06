/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Zoey Rose and Atrinik Development Team      *
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
 * This file controls various event functions, like character mouse movement,
 * parsing macro keys etc.
 */

#include <global.h>
#include <video.h>

/** @copydoc event_drag_cb_fnc */
static event_drag_cb_fnc event_drag_cb = NULL;

static int dragging_old_mx = -1, dragging_old_my = -1;

int event_dragging_check(void) {
    int mx, my;

    if (!cpl.dragging_tag) {
        return 0;
    }

    mouse_get_state(&mx, &my);

    if (abs(cpl.dragging_startx - mx) < 3 && abs(cpl.dragging_starty - my) < 3) {
        return 0;
    }

    return 1;
}

int event_dragging_need_redraw(void) {
    int mx, my;

    if (!event_dragging_check()) {
        return 0;
    }

    mouse_get_state(&mx, &my);

    if (mx != dragging_old_mx || my != dragging_old_my) {
        dragging_old_mx = mx;
        dragging_old_my = my;

        return 1;
    }

    return 0;
}

void event_dragging_start(tag_t tag, int mx, int my) {
    dragging_old_mx = -1;
    dragging_old_my = -1;

    cpl.dragging_tag = tag;
    cpl.dragging_startx = mx;
    cpl.dragging_starty = my;

    event_dragging_set_callback(NULL);
}

void event_dragging_set_callback(event_drag_cb_fnc fnc) {
    event_drag_cb = fnc;
}

void event_dragging_stop(void) {
    cpl.dragging_tag = 0;
}

static void event_dragging_stop_internal(void) {
    if (event_dragging_check() && event_drag_cb != NULL) {
        event_drag_cb();
    }

    event_dragging_stop();
}

/**
 * Sets new width/height of the screen, storing the size in options.
 *
 * Does not actually do the resizing.
 * @param width
 * Width to set.
 * @param height
 * Height to set.
 */
void resize_window(int width, int height) {
    setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X, width);
    setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y, height);

    if (!setting_get_int(OPT_CAT_CLIENT, OPT_OFFSCREEN_WIDGETS) && width > 100 && height > 100) {
        widgets_ensure_onscreen();
    }
}

/**
 * Poll input device like mouse, keys, etc.
 * @return
 * 1 if the the quit key was pressed, 0 otherwise
 */
int Event_PollInputDevice(void) {
    SDL_Event event;
    int x, y, done = 0;
    static Uint32 Ticks = 0;

    /* Execute mouse actions, even if mouse button is being held. */
    if ((SDL_GetTicks() - Ticks > 125) || !Ticks) {
        if (cpl.state >= ST_PLAY) {
            /* Mouse gesture: hold right+left buttons or middle button
             * to fire. */
            if (widget_mouse_event.owner == cur_widget[MAP_ID]) {
                if (map_mouse_fire()) {
                    Ticks = SDL_GetTicks();
                }
            }
        }
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            x = (int)event.wheel.mouse_x;
            y = (int)event.wheel.mouse_y;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION ||
                   event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                   event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            x = (int)event.motion.x;
            y = (int)event.motion.y;
        }

        if (event.type == SDL_EVENT_KEY_DOWN) {
            keys[event.key.scancode].repeated = event.key.repeat;
            keys[event.key.scancode].pressed = 1;
        } else if (event.type == SDL_EVENT_KEY_UP) {
            keys[event.key.scancode].pressed = 0;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            tooltip_dismiss();
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_PRINTSCREEN) {
            screenshot_create(ScreenSurface);
            continue;
        }

        switch (event.type) {
                /* Screen has been resized, update screen size. */
            case SDL_EVENT_WINDOW_RESIZED:
                ScreenSurface = SDL_GetWindowSurface(ScreenWindow);

                if (!ScreenSurface) {
                    LOG(ERROR, "Unable to grab surface after resize event: %s", SDL_GetError());
                    exit(1);
                }

                /* Set resolution to custom. */
                setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION, 0);
                resize_window(event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_WHEEL:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_TEXT_INPUT:
            case SDL_EVENT_TEXT_EDITING:

                if (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_WHEEL) {
                    cursor_x = x;
                    cursor_y = y;
                    cursor_texture = texture_get(TEXTURE_TYPE_CLIENT, "cursor_default");
                }

                if (popup_handle_event(&event)) {
                    break;
                }

                if (event_dragging_check() && event.type != SDL_EVENT_MOUSE_BUTTON_UP) {
                    break;
                }

                if (cpl.state <= ST_WAITFORPLAY && intro_event(&event)) {
                    break;
                } else if (cpl.state == ST_PLAY && widgets_event(&event)) {
                    break;
                }

                if (cpl.state == ST_PLAY &&
                    (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
                    key_handle_event(&event.key);
                    break;
                }

                break;

            case SDL_EVENT_QUIT:
                done = 1;
                break;

            case SDL_EVENT_USER:
                if (event.user.code == EVENT_SOUND_MUSIC_FINISHED) {
                    sound_music_finished_handle();
                }
                break;

            default:
                break;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            event_dragging_stop_internal();
        }
    }

    return done;
}

void event_push_key(SDL_EventType type, SDL_Keycode key, SDL_Keymod mod) {
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.which = 0;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_GetScancodeFromKey(key, NULL);
    event.key.key = key;
    event.key.mod = mod;
    SDL_PushEvent(&event);
}

void event_push_key_once(SDL_Keycode key, SDL_Keymod mod) {
    event_push_key(SDL_EVENT_KEY_DOWN, key, mod);
    event_push_key(SDL_EVENT_KEY_UP, key, mod);
}

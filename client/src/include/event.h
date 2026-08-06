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
 * Event related header file.
 */

#ifndef EVENT_H
#define EVENT_H

enum {
    DRAG_GET_STATUS = -1,
    DRAG_NONE,
    DRAG_QUICKSLOT,
    DRAG_QUICKSLOT_SPELL
};

/** SDL user-event code posted when SDL_mixer finishes a music track. */
#define EVENT_SOUND_MUSIC_FINISHED 1

/**
 * Called when dragged object is not handled, and a handler was specified.
 */
typedef void (*event_drag_cb_fnc)(void);

/**
 * Key information.
 */
typedef struct key_struct {
    /** If 1, the key is pressed. */
    uint8_t pressed;

    /** Last repeat time. */
    uint32_t time;

    /** Whether the key is being repeated. */
    uint8_t repeated;
} key_struct;

#define EVENT_IS_MOUSE(_event)                                                       \
    ((_event)->type == SDL_MOUSEBUTTONDOWN || (_event)->type == SDL_MOUSEBUTTONUP || \
     (_event)->type == SDL_MOUSEMOTION)
#define EVENT_IS_KEY(_event) ((_event)->type == SDL_KEYDOWN || (_event)->type == SDL_KEYUP)

/** Public API implemented in src/events/event.c. */

extern int event_dragging_check(void);

extern int event_dragging_need_redraw(void);

extern void event_dragging_start(tag_t tag, int mx, int my);

extern void event_dragging_set_callback(event_drag_cb_fnc fnc);

extern void event_dragging_stop(void);

extern void resize_window(int width, int height);

extern int Event_PollInputDevice(void);

extern void event_push_key(SDL_EventType type, SDLKey key, SDLMod mod);

extern void event_push_key_once(SDLKey key, SDLMod mod);

/** Public API implemented in src/events/keys.c. */

extern key_struct keys[SDLK_LAST];

extern void init_keys(void);

extern void key_handle_event(SDL_KeyboardEvent *event);

/** Public API implemented in src/events/move.c. */

extern void client_send_fire(int num, tag_t tag);

extern void move_keys(int num);

extern int dir_from_tile_coords(int tx, int ty);

#endif

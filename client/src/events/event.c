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
 * This file controls various event functions, like character mouse movement,
 * parsing macro keys etc. */

#include <global.h>

int old_mouse_y = 0;

int event_dragging_check(void)
{
	int mx, my;

	if (!cpl.dragging_tag)
	{
		return 0;
	}

	if (SDL_GetMouseState(&mx, &my) != SDL_BUTTON_LEFT)
	{
		return 0;
	}

	if (abs(cpl.dragging_startx - mx) < 3 && abs(cpl.dragging_starty - my) < 3)
	{
		return 0;
	}

	return 1;
}

void event_dragging_start(int tag, int mx, int my)
{
	cpl.dragging_tag = tag;
	cpl.dragging_startx = mx;
	cpl.dragging_starty = my;
}

void event_dragging_stop(void)
{
	cpl.dragging_tag = 0;
}

/**
 * Sets new width/height of the screen, storing the size in options.
 *
 * Does not actually do the resizing.
 * @param width Width to set.
 * @param height Height to set. */
void resize_window(int width, int height)
{
	setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_X, width);
	setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION_Y, height);

	if (!setting_get_int(OPT_CAT_CLIENT, OPT_OFFSCREEN_WIDGETS) && width > 100 && height > 100)
	{
		widgets_ensure_onscreen();
	}
}

/**
 * Poll input device like mouse, keys, etc.
 * @return 1 if the the quit key was pressed, 0 otherwise */
int Event_PollInputDevice(void)
{
	SDL_Event event;
	int x, y, done = 0;
	static Uint32 Ticks = 0;
	int tx, ty;

	/* Execute mouse actions, even if mouse button is being held. */
	if ((SDL_GetTicks() - Ticks > 125) || !Ticks)
	{
		if (cpl.state >= ST_PLAY)
		{
			/* Mouse gesture: hold right+left buttons or middle button
			 * to fire. */
			if (widget_mouse_event.owner == cur_widget[MAP_ID])
			{
				int state = SDL_GetMouseState(&x, &y);

				if ((state == (SDL_BUTTON(SDL_BUTTON_RIGHT) | SDL_BUTTON(SDL_BUTTON_LEFT)) || state == SDL_BUTTON(SDL_BUTTON_MIDDLE)) && mouse_to_tile_coords(x, y, &tx, &ty))
				{
					Ticks = SDL_GetTicks();
					cpl.fire_on = 1;
					move_keys(dir_from_tile_coords(tx, ty));
					cpl.fire_on = 0;
				}
			}
		}
	}

	while (SDL_PollEvent(&event))
	{
		x = event.motion.x;
		y = event.motion.y;

		if (event.type == SDL_KEYDOWN)
		{
			if (!keys[event.key.keysym.sym].pressed)
			{
				keys[event.key.keysym.sym].repeated = 0;
				keys[event.key.keysym.sym].pressed = 1;
				keys[event.key.keysym.sym].time = LastTick + KEY_REPEAT_TIME_INIT;
			}
		}
		else if (event.type == SDL_KEYUP)
		{
			keys[event.key.keysym.sym].pressed = 0;
		}
		else if (event.type == SDL_MOUSEMOTION)
		{
			tooltip_dismiss();
		}

		if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_PRINT)
		{
			screenshot_create(ScreenSurface);
			continue;
		}

		if (event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION || event.type == SDL_KEYUP || event.type == SDL_KEYDOWN)
		{
			if (popup_handle_event(&event))
			{
				continue;
			}
			else if (cpl.state <= ST_WAITFORPLAY && main_screen_event(&event))
			{
				continue;
			}
		}

		switch (event.type)
		{
			/* Screen has been resized, update screen size. */
			case SDL_VIDEORESIZE:
				ScreenSurface = SDL_SetVideoMode(event.resize.w, event.resize.h, video_get_bpp(), get_video_flags());

				if (!ScreenSurface)
				{
					logger_print(LOG(ERROR), "Unable to grab surface after resize event: %s", SDL_GetError());
					exit(1);
				}

				/* Set resolution to custom. */
				setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION, 0);
				resize_window(event.resize.w, event.resize.h);
				break;

			case SDL_MOUSEBUTTONUP:
				if (cpl.state < ST_PLAY)
				{
					break;
				}

				if (widget_event_mouseup(x, y, &event))
				{
					event_dragging_stop();
					break;
				}

				event_dragging_stop();

				break;

			case SDL_MOUSEMOTION:
			{
				if (cpl.state < ST_PLAY)
				{
					break;
				}

				x_custom_cursor = x;
				y_custom_cursor = y;

				if (!event_dragging_check() && widget_event_mousemv(x, y, &event))
				{
					break;
				}

				break;
			}

			case SDL_MOUSEBUTTONDOWN:
			{
				if (cpl.state < ST_PLAY)
				{
					break;
				}

				if (!event_dragging_check() && widget_event_mousedn(x, y, &event))
				{
					break;
				}

				break;
			}

			case SDL_KEYUP:
			case SDL_KEYDOWN:
				if (widget_input_handle_key(cur_widget[IN_NUMBER_ID], &event))
				{
					break;
				}
				else if (widget_input_handle_key(cur_widget[IN_CONSOLE_ID], &event))
				{
					break;
				}

				key_handle_event(&event.key);
				break;

			case SDL_QUIT:
				done = 1;
				break;

			default:
				break;
		}

		old_mouse_y = y;
	}

#if 0
	if (!text_input_string_flag)
	{
		size_t i;

		for (i = 0; i < SDLK_LAST; i++)
		{
			/* Ignore modifier keys. */
			if (KEY_IS_MODIFIER(i))
			{
				continue;
			}

			if (keys[i].pressed && keys[i].time + KEY_REPEAT_TIME - 5 < LastTick)
			{
				while ((keys[i].time += KEY_REPEAT_TIME - 5) < LastTick)
				{
					keys[i].repeated = 1;

					event.type = SDL_KEYDOWN;
					event.key.which = 0;
					event.key.state = SDL_PRESSED;
					event.key.keysym.unicode = 0;
					event.key.keysym.mod = SDL_GetModState();
					event.key.keysym.sym = i;
					SDL_PushEvent(&event);
				}
			}
		}
	}
#endif

	return done;
}

void event_push_key(SDL_EventType type, SDLKey key, SDLMod mod)
{
	SDL_Event event;

	event.type = type;
	event.key.which = 0;
	event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.unicode = key;
	event.key.keysym.sym = key;
	event.key.keysym.mod = mod;
	SDL_PushEvent(&event);
}

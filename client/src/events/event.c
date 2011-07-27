/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2011 Alex Tokar and Atrinik Development Team    *
*                                                                       *
* Fork from Daimonin (Massive Multiplayer Online Role Playing Game)     *
* and Crossfire (Multiplayer game for X-windows).                       *
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

/* src:  (if != DRAG_GET_STATUS) set actual dragging source.
 * item: (if != NULL) set actual dragging item.
 * ret:  the actual dragging source. */
int draggingInvItem(int src)
{
	static int drag_src = DRAG_NONE;

	if (src != DRAG_GET_STATUS)
		drag_src = src;

	return drag_src;
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
int Event_PollInputDevice()
{
	SDL_Event event;
	int x, y, done = 0;
	static Uint32 Ticks = 0;
	int tx, ty;

	/* Execute mouse actions, even if mouse button is being held. */
	if ((SDL_GetTicks() - Ticks > 125) || !Ticks)
	{
		if (GameStatus >= GAME_STATUS_PLAY)
		{
			if (text_input_string_flag && cpl.input_mode == INPUT_MODE_NUMBER)
			{
				Ticks = SDL_GetTicks();
				mouse_InputNumber();
			}
			/* Mouse gesture: hold right+left buttons or middle button
			 * to fire. */
			else if (!cpl.action_timer && cpl.menustatus == MENU_NO && widget_mouse_event.owner == cur_widget[MAP_ID])
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

		if ((event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEMOTION || event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) && popup_handle_event(&event))
		{
			continue;
		}

		switch (event.type)
		{
			/* Screen has been resized, update screen size. */
			case SDL_VIDEORESIZE:
				ScreenSurface = SDL_SetVideoMode(event.resize.w, event.resize.h, video_get_bpp(), get_video_flags());

				if (!ScreenSurface)
				{
					LOG(llevError, "Unable to grab surface after resize event: %s\n", SDL_GetError());
				}

				/* Set resolution to custom. */
				setting_set_int(OPT_CAT_CLIENT, OPT_RESOLUTION, 0);
				resize_window(event.resize.w, event.resize.h);
				break;

			case SDL_MOUSEBUTTONUP:
				if (lists_handle_mouse(x, y, &event))
				{
					break;
				}

				if (GameStatus < GAME_STATUS_PLAY)
				{
					break;
				}

				/* Widget has higher priority than anything below, except menus
				 * so break if we had a widget event */
				if (widget_event_mouseup(x,y, &event))
				{
					/* NOTE: Place here special handlings that have to be done, even if a widget owns it */

					/* Sanity handling */
					draggingInvItem(DRAG_NONE);
					break;
				}

				if (text_input_string_flag && cpl.input_mode == INPUT_MODE_NUMBER)
					break;

				/* Only drop to ground has to be handled, the rest do the widget handlers */
				if (draggingInvItem(DRAG_GET_STATUS) > DRAG_IWIN_BELOW)
				{
					/* KEYFUNC_APPLY and KEYFUNC_DROP works only if cpl.inventory_win = IWIN_INV. The tag must
					 * be placed in cpl.win_inv_tag. So we do this and after DnD we restore the old values. */
					int old_inv_win = cpl.inventory_win;
					int old_inv_tag = cpl.win_inv_tag;
					cpl.inventory_win = IWIN_INV;

					/* Drop to ground */
					if (mouse_to_tile_coords(x, y, NULL, NULL))
					{
						if (draggingInvItem(DRAG_GET_STATUS) != DRAG_QUICKSLOT_SPELL)
						{
							keybind_process_command("?DROP");
						}
					}

					cpl.inventory_win = old_inv_win;
					cpl.win_inv_tag = old_inv_tag;
				}

				draggingInvItem(DRAG_NONE);
				break;

			case SDL_MOUSEMOTION:
			{
				if (lists_handle_mouse(x, y, &event))
				{
					break;
				}

				if (GameStatus < GAME_STATUS_PLAY)
				{
					break;
				}

				x_custom_cursor = x;
				y_custom_cursor = y;

				/* We have to break now when menu is active - menu is higher priority than any widget! */
				if (cpl.menustatus != MENU_NO)
				{
					break;
				}

				if (widget_event_mousemv(x, y, &event))
				{
					/* NOTE: place here special handlings that have to be done, even if a widget owns it */

					break;
				}

				break;
			}

			case SDL_MOUSEBUTTONDOWN:
			{
				if (lists_handle_mouse(x, y, &event))
				{
					break;
				}

				if (GameStatus == GAME_STATUS_WAITLOOP)
				{
					break;
				}

				if (GameStatus < GAME_STATUS_PLAY)
				{
					break;
				}

				/* Beyond here only when no menu is active. */
				if (cpl.menustatus != MENU_NO)
				{
					break;
				}

				/* Widget System */
				if (widget_event_mousedn(x, y, &event))
				{
					/* NOTE: Place here special handlings that have to be done, even if a widget owns it */

					break;
				}

				break;
			}

			case SDL_KEYUP:
			case SDL_KEYDOWN:
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

	return done;
}

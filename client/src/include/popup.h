/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2010 Alex Tokar and Atrinik Development Team    *
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
 * Header file for popup API. */

#ifndef POPUP_H
#define POPUP_H

/** A single popup. */
typedef struct popup_struct
{
	/**
	 * Surface the popup uses for blitting. This surface is then copied
	 * to ::ScreenSurface. */
	SDL_Surface *surface;

	/** Bitmap ID to blit on the surface. */
	int bitmap_id;

	/**
	 * Overlay image to draw before popup_struct::surface over the
	 * ::ScreenSurface. */
	SDL_Surface *overlay;

	/** Custom data. */
	void *custom_data;

	/** Optional character pointer. */
	char *buf;

	/**
	 * Function used for drawing on the popup's surface.
	 * @param popup The popup. */
	void (*draw_func)(struct popup_struct *popup);

	/**
	 * Function used for handling mouse/key events when popup is visible.
	 * @param event SDL event.
	 * @retval -1 Did not handle the event.
	 * @retval 0 Did not handle the event, but allow other keyboard
	 * events.
	 * @retval 1 Handled the event. */
	int (*event_func)(SDL_Event *event);
} popup_struct;

#endif

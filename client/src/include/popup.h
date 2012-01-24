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
 * Header file for popup API. */

#ifndef POPUP_H
#define POPUP_H

/**
 * A single "generic" button in a popup. */
typedef struct popup_button
{
	/** X position in the popup of the button. */
	int x;

	/** Y position in the popup of the button. */
	int y;

	/** Text to show in the button. */
	char *text;

	/** The button. */
	button_struct button;

	/**
	 * Callback function to call when the button is clicked.
	 * @param button The clicked button.
	 * @retval 1 Handled the event, should not do generic handling.
	 * @retval 0 Did not handle the event. */
	int (*event_func)(struct popup_button *button);
} popup_button;

/** A single popup. */
typedef struct popup_struct
{
	/**
	 * Surface the popup uses for blitting. This surface is then copied
	 * to ::ScreenSurface. */
	SDL_Surface *surface;

	/** Bitmap ID to blit on the surface. */
	int bitmap_id;

	/** Disable automatically blitting the bitmap on the popup surface? */
	uint8 disable_bitmap_blit;

	/** Custom data. */
	void *custom_data;

	/** Optional character pointer. */
	char *buf;

	/** Optional integers. */
	sint64 i[3];

	/** X position of the popup. */
	int x;

	/** Y position of the popup. */
	int y;

	/** The left button, generally the help button. */
	popup_button button_left;

	/** The right button, generally the close button. */
	popup_button button_right;

	/** Next popup in a doubly-linked list. */
	struct popup_struct *next;

	/** Previous popup in a doubly-linked list. */
	struct popup_struct *prev;

	/** Start of selection. */
	sint64 selection_start;

	/** End of selection. */
	sint64 selection_end;

	/** Whether the selection has started. */
	uint8 selection_started;

	/** Whether redrawing is in order. */
	uint8 redraw;

	/**
	 * Function used for drawing on the popup's surface.
	 * @param popup The popup.
	 * @return 0 to destroy the popup, 1 otherwise. */
	int (*draw_func)(struct popup_struct *popup);

	/**
	 * Function used for drawing after blitting the popup's surface on
	 * the main surface.
	 * @param popup The popup.
	 * @return 0 to destroy the popup, 1 otherwise. */
	int (*draw_func_post)(struct popup_struct *popup);

	/**
	 * Function used for handling mouse/key events when popup is visible.
	 * @param event SDL event.
	 * @retval -1 Did not handle the event.
	 * @retval 0 Did not handle the event, but allow other keyboard
	 * events.
	 * @retval 1 Handled the event. */
	int (*event_func)(struct popup_struct *popup, SDL_Event *event);

	/**
	 * Function used right before the visible popup is destroyed using
	 * popup_destroy_visible().
	 * @param popup The popup.
	 * @return 1 to proceed with the destruction of the popup, 0
	 * otherwise. */
	int (*destroy_callback_func)(struct popup_struct *popup);

	/**
	 * Function used to get contents for clipboard copy operation.
	 * @param popup Popup.
	 * @return Contents to copy. */
	const char *(*clipboard_copy_func)(struct popup_struct *popup);
} popup_struct;

#endif

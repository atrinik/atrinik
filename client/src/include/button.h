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
 * Button header file. */

#ifndef BUTTON_H
#define BUTTON_H

/** Determine whether the x,y position is over the specified button. */
#define BUTTON_MOUSE_OVER(button, mx, my, sprite) ((mx) >= (button)->x && (mx) < (button)->x + (sprite)->bitmap->w && (my) >= (button)->y && (my) < (button)->y + (sprite)->bitmap->h)
/** Delay in milliseconds for the tooltip to appear (if any). */
#define BUTTON_TOOLTIP_DELAY 750

/** Button structure. */
typedef struct button_struct
{
	/** X position. */
	int x;

	/** Y position. */
	int y;

	/** Bitmap to normally use for the button. */
	int bitmap;

	/**
	 * Bitmap to use if the mouse is over the button, -1 to use regular
	 * one. */
	int bitmap_over;

	/**
	 * Bitmap to use if the button is being pressed, -1 to use regular
	 * one. */
	int bitmap_pressed;

	/** Font used for the text. */
	int font;

	/** Text flags. */
	uint64 flags;

	/** Color of the text. */
	const char *color;

	/** Color of the text's shadow. */
	const char *color_shadow;

	/** Color of the text if the mouse is over the button. */
	const char *color_over;

	/** Color of the text's shadow if the mouse is over the button. */
	const char *color_over_shadow;

	/** 1 if the mouse is over the button. */
	int mouse_over;

	/**
	 * 1 if the button is being pressed.
	 * @private */
	int pressed;

	/** 1 if the button should be forced to be pressed. */
	int pressed_forced;

	/** If 1, the button is in disabled state and cannot be pressed. */
	int disabled;

	/** When the button was pressed. */
	uint32 pressed_ticks;

	/** When the mouse started hovering over the button. */
	uint32 hover_ticks;

	/** Ticks needed to trigger a repeat. */
	uint32 pressed_repeat_ticks;

	/**
	 * Function called on button repeat
	 * @param button The button. */
	void (*repeat_func)(struct button_struct *button);
} button_struct;

#endif

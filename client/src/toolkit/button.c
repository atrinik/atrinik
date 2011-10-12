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
 * Button API.
 *
 * @author Alex Tokar */

#include <global.h>

static button_struct button_mousestate;
/**
 * Last clicked ticks to prevent single button click from triggering many
 * actions at once. */
static uint32 ticks = 0;
/**
 * How many milliseconds must past before a button repeat is triggered. */
static uint32 ticks_delay;

void button_init()
{
	button_create(&button_mousestate);
}

/**
 * Show a button.
 * @param bitmap_id Bitmap ID to use for the button.
 * @param bitmap_id_over Bitmap ID to use for the button when the mouse
 * is over the button. -1 to use 'bitmap_id'.
 * @param bitmap_id_clicked Bitmap ID to use for the button when left
 * mouse button is down. -1 to use 'bitmap_id'.
 * @param x X position of the button.
 * @param y Y position of the button.
 * @param text Text to display in the middle of the button. Can be NULL
 * for no text.
 * @param font Font to use for the text. One of @ref FONT_xxx.
 * @param color Color to use for the text.
 * @param color_shadow Shadow color of the text.
 * @param color_over Color to use when the mouse is over the button. -1
 * to use 'color'.
 * @param color_over_shadow Shadow color to use when the mouse is over
 * the button. -1 to use 'color_shadow'.
 * @param flags Text @ref TEXT_xxx "flags".
 * @return 1 if left mouse button is being held over the button, 0
 * otherwise. */
int button_show(int bitmap_id, int bitmap_id_over, int bitmap_id_clicked, int x, int y, const char *text, int font, const char *color, const char *color_shadow, const char *color_over, const char *color_over_shadow, uint64 flags, uint8 focus)
{
	_Sprite *sprite = Bitmaps[bitmap_id];
	int mx, my, ret = 0, state;
	const char *use_color = color, *use_color_shadow = color_shadow;

	/* Get state of the mouse and the x/y. */
	state = SDL_GetMouseState(&mx, &my);

	/* Is the mouse inside the button? */
	if (focus && mx > x && mx < x + sprite->bitmap->w && my > y && my < y + sprite->bitmap->h)
	{
		/* Change color. */
		use_color = color_over;
		use_color_shadow = color_over_shadow;

		/* Left button clicked? */
		if (state == SDL_BUTTON_LEFT)
		{
			/* Change bitmap. */
			if (bitmap_id_clicked != -1)
			{
				sprite = Bitmaps[bitmap_id_clicked];
			}

			if (!ticks || SDL_GetTicks() - ticks > ticks_delay)
			{
				ticks_delay = ticks ? 125 : 700;
				ticks = SDL_GetTicks();
				ret = 1;
			}
		}
		else
		{
			if (bitmap_id_over != -1)
			{
				sprite = Bitmaps[bitmap_id_over];
			}

			ticks = 0;
		}
	}

	/* Draw the bitmap. */
	sprite_blt(sprite, x, y, NULL, NULL);

	/* If text was passed, draw it as well. */
	if (text)
	{
		string_blt_shadow(ScreenSurface, font, text, x + sprite->bitmap->w / 2 - string_get_width(font, text, flags) / 2, y + sprite->bitmap->h / 2 - FONT_HEIGHT(font) / 2, use_color, use_color_shadow, flags, NULL);
	}

	return ret;
}

/**
 * Determine button's sprite, based on its bitmap settings and whether it
 * is currently pressed, or mouse is over it.
 * @param button Button.
 * @return Sprite to use. */
static _Sprite *button_determine_sprite(button_struct *button)
{
	if (button->pressed && button->bitmap_pressed != -1)
	{
		return Bitmaps[button->bitmap_pressed];
	}
	else if (button->mouse_over && button->bitmap_over != -1)
	{
		return Bitmaps[button->bitmap_over];
	}
	else
	{
		return Bitmaps[button->bitmap];
	}
}

/**
 * Initialize a button's default values.
 * @param button Button. */
void button_create(button_struct *button)
{
	/* Initialize default values. */
	button->x = button->y = 0;
	button->bitmap = BITMAP_BUTTON;
	button->bitmap_over = -1;
	button->bitmap_pressed = BITMAP_BUTTON_DOWN;
	button->bitmap_over = BITMAP_BUTTON_HOVER;
	button->font = FONT_ARIAL10;
	button->flags = 0;
	button->color = COLOR_WHITE;
	button->color_shadow = COLOR_BLACK;
	button->color_over = COLOR_HGOLD;
	button->color_over_shadow = COLOR_BLACK;
	button->mouse_over = button->pressed = button->pressed_forced = button->disabled = 0;
	button->pressed_ticks = button->hover_ticks = button->pressed_repeat_ticks = 0;
	button->repeat_func = NULL;
}

/**
 * Render a button.
 * @param button Button to render.
 * @param text Optional text to render. */
void button_render(button_struct *button, const char *text)
{
	_Sprite *sprite;

	/* Make sure the mouse is still over the button. */
	if (button->mouse_over || button->pressed)
	{
		int state, mx, my, mover;

		state = SDL_GetMouseState(&mx, &my);
		mover = BUTTON_MOUSE_OVER(button, mx, my, Bitmaps[button->bitmap]);

		if (!mover)
		{
			button->mouse_over = 0;
		}

		if (!mover || state != SDL_BUTTON_LEFT)
		{
			button->pressed = 0;
		}
	}

	if (button->pressed_forced)
	{
		button->pressed = 1;
	}

	sprite = button_determine_sprite(button);
	sprite_blt(sprite, button->x, button->y, NULL, NULL);

	if (text)
	{
		const char *color, *color_shadow;

		if (button->mouse_over)
		{
			color = button->color_over;
			color_shadow = button->color_over_shadow;
		}
		else
		{
			color = button->color;
			color_shadow = button->color_shadow;
		}

		string_blt_shadow(ScreenSurface, button->font, text, button->x + sprite->bitmap->w / 2 - string_get_width(button->font, text, button->flags) / 2, button->y + sprite->bitmap->h / 2 - FONT_HEIGHT(button->font) / 2, color, color_shadow, button->flags, NULL);
	}

	if (button->repeat_func && button->pressed && SDL_GetTicks() - button->pressed_ticks > button->pressed_repeat_ticks)
	{
		button->repeat_func(button);
		button->pressed_ticks = SDL_GetTicks();
		button->pressed_repeat_ticks = 150;
	}
}

/**
 * Handle SDL event for a button.
 * @param button Button to handle.
 * @param event The event.
 * @return 1 if the event makes the button pressed, 0 otherwise. */
int button_event(button_struct *button, SDL_Event *event)
{
	_Sprite *sprite;
	int old_mouse_over;

	if (event->type != SDL_MOUSEBUTTONUP && event->type != SDL_MOUSEBUTTONDOWN && event->type != SDL_MOUSEMOTION)
	{
		return 0;
	}

	/* Mouse button is released, the button is no longer being pressed. */
	if (event->type == SDL_MOUSEBUTTONUP)
	{
		button->pressed = 0;
		return 0;
	}

	old_mouse_over = button->mouse_over;
	/* Always reset this. */
	button->mouse_over = 0;

	/* The button is disabled, we don't care about the mouse. */
	if (button->disabled)
	{
		return 0;
	}

	sprite = button_determine_sprite(button);

	if (BUTTON_MOUSE_OVER(button, event->motion.x, event->motion.y, sprite))
	{
		/* Left mouse click, the button has been pressed. */
		if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
		{
			button->pressed = 1;
			button->pressed_ticks = SDL_GetTicks();
			button->pressed_repeat_ticks = 750;
			return 1;
		}
		else
		{
			button->mouse_over = 1;

			/* Do not reset hover ticks if the previous state was already
			 * in highlight mode. */
			if (!old_mouse_over)
			{
				button->hover_ticks = SDL_GetTicks();
			}
		}
	}

	return 0;
}

/**
 * Render a tooltip, if possible.
 * @param button Button.
 * @param font Font to use for the tooltip text.
 * @param text Tooltip text. */
void button_tooltip(button_struct *button, int font, const char *text)
{
	/* Sanity check. */
	if (!button || !text)
	{
		return;
	}

	/* Render the tooltip if the mouse is over the button, it's not
	 * pressed, and enough time has passed since the button was
	 * highlighted. */
	if (button->mouse_over && !button->pressed && SDL_GetTicks() - button->hover_ticks > BUTTON_TOOLTIP_DELAY)
	{
		int mx, my;

		SDL_GetMouseState(&mx, &my);

		tooltip_create(mx, my, font, text);
	}
}

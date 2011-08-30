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
 * Popup API.
 *
 * Popup is basically a specified bitmap that appears in the middle of
 * the screen, graying out the background and disabling mouse clicks and
 * keyboard actions on the background.
 *
 * Graying out the background is managed by using an overlay image, which
 * is an SDL surface created when the popup is created. The pixels from
 * ::ScreenSurface are copied to this surface and grayed out, and the
 * surface is then copied over the ::ScreenSurface before doing any
 * actual popup drawing. When the screen size changes, the overlay is
 * re-created. */

#include <global.h>

/**
 * Doubly-linked list of the visible popups. */
static popup_struct *popup_head = NULL;
/**
 * Grayed out copy of ::ScreenSurface. */
static SDL_Surface *popup_overlay = NULL;
/**
 * Whether the popup overlay needs redrawing. */
static uint8 popup_overlay_need_redraw = 0;

/**
 * Create an overlay to be used as popup background. */
static void popup_create_overlay(void)
{
	popup_struct *popup;
	int j, k;
	uint8 r, g, b, a;

	DL_FOREACH_REVERSE(popup_head, popup)
	{
		if (popup == popup_head)
		{
			break;
		}

		popup_render(popup);
	}

	/* Already exists? Free it. */
	if (popup_overlay)
	{
		SDL_FreeSurface(popup_overlay);
	}

	/* Create SDL surface with the same size as the ScreenSurface. */
	popup_overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, ScreenSurface->w, ScreenSurface->h, 32, 0, 0, 0, 0);

	/* Copy pixels from ScreenSurface. */
	for (k = 0; k < ScreenSurface->h; k++)
	{
		for (j = 0; j < ScreenSurface->w; j++)
		{
			SDL_GetRGBA(getpixel(ScreenSurface, j, k), ScreenSurface->format, &r, &g, &b, &a);
			/* Gray out the pixel. */
			r = g = b = (r + g + b) / 3;
			putpixel(popup_overlay, j, k, SDL_MapRGBA(popup_overlay->format, r, g, b, a));
		}
	}

	popup_overlay_need_redraw = 0;
}

/**
 * Create a new popup.
 * @param bitmap_id Bitmap ID to use for the popup's background.
 * @return The created popup. */
popup_struct *popup_create(int bitmap_id)
{
	popup_struct *popup;
	int mx, my;

	popup = calloc(1, sizeof(popup_struct));
	/* Create the surface used by the popup. */
	popup->surface = SDL_ConvertSurface(Bitmaps[bitmap_id]->bitmap, Bitmaps[bitmap_id]->bitmap->format, Bitmaps[bitmap_id]->bitmap->flags);
	/* Store the bitmap used. */
	popup->bitmap_id = bitmap_id;
	DL_PREPEND(popup_head, popup);

	popup_create_overlay();

	SDL_GetMouseState(&mx, &my);
	/* Make sure the mouse is no longer moving any widget. */
	widget_event_move_stop(mx, my);

	button_create(&popup->button_left.button);
	button_create(&popup->button_right.button);

	popup->button_left.x = 6;
	popup->button_left.y = 6;

	popup->button_right.x = 468;
	popup->button_right.y = 6;
	popup->button_right.text = strdup("X");

	popup->button_left.button.bitmap = popup->button_right.button.bitmap = BITMAP_BUTTON_ROUND_LARGE;
	popup->button_left.button.bitmap_pressed = popup->button_right.button.bitmap_pressed = BITMAP_BUTTON_ROUND_LARGE_DOWN;
	popup->button_left.button.bitmap_over = popup->button_right.button.bitmap_over = BITMAP_BUTTON_ROUND_LARGE_HOVER;

	return popup;
}

/**
 * Free the data used by a popup button.
 * @param button The button. */
static void popup_button_free(popup_button *button)
{
	if (button->text)
	{
		free(button->text);
	}
}

/**
 * Destroy the visible popup, freeing it. */
void popup_destroy(popup_struct *popup)
{
	if (popup->destroy_callback_func && !popup->destroy_callback_func(popup))
	{
		return;
	}

	DL_DELETE(popup_head, popup);
	SDL_FreeSurface(popup->surface);

	if (popup->buf)
	{
		free(popup->buf);
	}

	popup_button_free(&popup->button_right);
	popup_button_free(&popup->button_left);

	free(popup);

	/* Free the overlay; will need to be re-created. */
	if (popup_overlay)
	{
		SDL_FreeSurface(popup_overlay);
		popup_overlay = NULL;
	}
}

/**
 * Destroy all visible popups. */
void popup_destroy_all(void)
{
	popup_struct *popup, *tmp;

	DL_FOREACH_SAFE(popup_head, popup, tmp)
	{
		popup_destroy(popup);
	}
}

/**
 * See if popup needs an overlay update due to screen resize or some
 * other reason.
 * @return Whether the overlay needs to be updated or not. */
int popup_overlay_need_update(void)
{
	return !popup_overlay || popup_overlay->w != ScreenSurface->w || popup_overlay->h != ScreenSurface->h || popup_overlay_need_redraw;
}

/**
 * Render a single popup button.
 * @param popup Popup.
 * @param button The button to render. */
static void popup_button_render(popup_struct *popup, popup_button *button)
{
	if (button->button.bitmap != -1)
	{
		button->button.x = popup->x + button->x;
		button->button.y = popup->y + button->y;
		button_render(&button->button, button->text ? button->text : "");
	}
}

/**
 * Draw popup, if there is a visible one. */
void popup_render(popup_struct *popup)
{
	SDL_Rect box;

	/* Draw the overlay. */
	box.x = 0;
	box.y = 0;
	SDL_BlitSurface(popup_overlay, NULL, ScreenSurface, &box);

	if (!popup->disable_bitmap_blit)
	{
		_BLTFX bltfx;

		/* Draw the background of the popup. */
		bltfx.surface = popup->surface;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		sprite_blt(Bitmaps[popup->bitmap_id], 0, 0, NULL, &bltfx);
	}

	/* Calculate the popup's X/Y positions. */
	popup->x = ScreenSurface->w / 2 - popup->surface->w / 2;
	popup->y = ScreenSurface->h / 2 - popup->surface->h / 2;

	/* Handle drawing inside the popup. */
	if (popup->draw_func)
	{
		if (!popup->draw_func(popup))
		{
			popup_destroy(popup);
			return;
		}
	}

	/* Show the popup in the middle of the screen. */
	box.x = popup->x;
	box.y = popup->y;
	SDL_BlitSurface(popup->surface, NULL, ScreenSurface, &box);

	popup_button_render(popup, &popup->button_left);
	popup_button_render(popup, &popup->button_right);

	if (popup->draw_func_post)
	{
		if (!popup->draw_func_post(popup))
		{
			popup_destroy(popup);
			return;
		}
	}
}

/**
 * Render the first popup. */
void popup_render_head(void)
{
	if (!popup_head)
	{
		return;
	}

	if (popup_overlay_need_update())
	{
		popup_create_overlay();
	}

	popup_render(popup_head);
}

/**
 * Handle popup button event.
 * @param button The button.
 * @param event The event.
 * @retval 1 Handled the event.
 * @retval -1 Handled the event and the button was handled by callback
 * function.
 * @retval 0 Did not handle the event. */
static int popup_button_handle_event(popup_button *button, SDL_Event *event)
{
	if (button->text && button_event(&button->button, event))
	{
		if (button->event_func && button->event_func(button))
		{
			return -1;
		}

		return 1;
	}

	return 0;
}

/**
 * Handle mouse and keyboard events when a popup is active.
 * @param event Event.
 * @return 1 to disable any other mouse/keyboard actions, 0 otherwise. */
int popup_handle_event(SDL_Event *event)
{
	int ret;

	/* No popup is visible. */
	if (!popup_head)
	{
		return 0;
	}

	/* Handle custom events? */
	if (popup_head->event_func)
	{
		ret = popup_head->event_func(popup_head, event);

		if (ret != -1)
		{
			return ret;
		}
	}

	/* Key is down. */
	if (event->type == SDL_KEYDOWN)
	{
		/* Escape, destroy the popup. */
		if (event->key.keysym.sym == SDLK_ESCAPE)
		{
			popup_destroy(popup_head);
		}
	}
	else if ((ret = popup_button_handle_event(&popup_head->button_left, event)))
	{
		return 1;
	}
	else if ((ret = popup_button_handle_event(&popup_head->button_right, event)))
	{
		if (ret == 1)
		{
			popup_destroy(popup_head);
		}

		return 1;
	}

	return 1;
}

/**
 * Get the currently visible popup.
 * @return The visible popup, or NULL if there isn't any. */
popup_struct *popup_get_head(void)
{
	return popup_head;
}

/**
 * Mark popup overlay as needing an update. */
void popup_overlay_redraw(void)
{
	popup_overlay_need_redraw = 1;
}

/**
 * Set the text of a generic popup button.
 * @param button The button.
 * @param text Text to set. */
void popup_button_set_text(popup_button *button, const char *text)
{
	if (button->text)
	{
		free(button->text);
	}

	button->text = text ? strdup(text) : NULL;
}

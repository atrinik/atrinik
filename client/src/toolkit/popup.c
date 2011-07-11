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

/** The currently visible popup. NULL if there is no visible popup. */
static popup_struct *popup_visible = NULL;

/**
 * Create an overlay to be used by popup.
 * @param popup The popup. */
static void popup_create_overlay(popup_struct *popup)
{
	int j, k;
	uint8 r, g, b, a;

	/* Already exists? Free it. */
	if (popup->overlay)
	{
		SDL_FreeSurface(popup->overlay);
	}

	/* Create SDL surface with the same size as the ScreenSurface. */
	popup->overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, ScreenSurface->w, ScreenSurface->h, 32, 0, 0, 0, 0);

	/* Copy pixels from ScreenSurface. */
	for (k = 0; k < ScreenSurface->h; k++)
	{
		for (j = 0; j < ScreenSurface->w; j++)
		{
			SDL_GetRGBA(getpixel(ScreenSurface, j, k), ScreenSurface->format, &r, &g, &b, &a);
			/* Gray out the pixel. */
			r = g = b = (r + g + b) / 3;
			putpixel(popup->overlay, j, k, SDL_MapRGBA(popup->overlay->format, r, g, b, a));
		}
	}
}

/**
 * Free popup structure and the popup itself.
 * @param popup Popup to free. */
static void popup_free(popup_struct *popup)
{
	SDL_FreeSurface(popup->surface);
	SDL_FreeSurface(popup->overlay);

	if (popup->buf)
	{
		free(popup->buf);
	}

	free(popup);
}

/**
 * Create a new popup.
 * @param bitmap_id Bitmap ID to use for the popup's background.
 * @return The created popup. */
popup_struct *popup_create(int bitmap_id)
{
	popup_struct *popup = calloc(1, sizeof(popup_struct));

	/* Create the surface used by the popup. */
	popup->surface = SDL_ConvertSurface(Bitmaps[bitmap_id]->bitmap, Bitmaps[bitmap_id]->bitmap->format, Bitmaps[bitmap_id]->bitmap->flags);
	/* Store the bitmap used. */
	popup->bitmap_id = bitmap_id;
	/* Create overlay. */
	popup_create_overlay(popup);
	popup_visible = popup;

	return popup;
}

/**
 * Destroys the currently visible popup, freeing it.
 *
 * If there is no visible popup, nothing is done. */
void popup_destroy_visible()
{
	if (popup_visible)
	{
		if (popup_visible->destroy_callback_func && !popup_visible->destroy_callback_func(popup_visible))
		{
			return;
		}

		popup_free(popup_visible);
		popup_visible = NULL;
	}
}

/**
 * See if popup needs an overlay update due to screen resize.
 * @param popup Popup.
 * @return Whether the overlay needs to be updated or not. */
int popup_overlay_need_update(popup_struct *popup)
{
	return popup->overlay->w != ScreenSurface->w || popup->overlay->h != ScreenSurface->h;
}

/**
 * Draw popup, if there is a visible one. */
void popup_draw()
{
	SDL_Rect box;
	_BLTFX bltfx;

	/* No visible popup, nothing to do. */
	if (!popup_visible)
	{
		return;
	}

	/* Update the overlay if surface size was changed. */
	if (popup_overlay_need_update(popup_visible))
	{
		popup_create_overlay(popup_visible);
	}

	/* Draw the overlay. */
	box.x = 0;
	box.y = 0;
	SDL_BlitSurface(popup_visible->overlay, NULL, ScreenSurface, &box);

	/* Draw the background of the popup. */
	bltfx.surface = popup_visible->surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;
	sprite_blt(Bitmaps[popup_visible->bitmap_id], 0, 0, NULL, &bltfx);

	/* Handle drawing inside the popup. */
	if (popup_visible->draw_func)
	{
		popup_visible->draw_func(popup_visible);
	}

	if (!popup_visible)
	{
		return;
	}

	/* Show the popup in the middle of the screen. */
	box.x = ScreenSurface->w / 2 - popup_visible->surface->w / 2;
	box.y = ScreenSurface->h / 2 - popup_visible->surface->h / 2;
	SDL_BlitSurface(popup_visible->surface, NULL, ScreenSurface, &box);

	popup_visible->x = box.x;
	popup_visible->y = box.y;

	if (popup_visible->draw_func_post)
	{
		popup_visible->draw_func_post(popup_visible);
	}

	if (!popup_visible)
	{
		return;
	}

	/* Show close button. */
	if (button_show(BITMAP_BUTTON_ROUND, -1, BITMAP_BUTTON_ROUND_DOWN, box.x + popup_visible->surface->w - Bitmaps[BITMAP_BUTTON_ROUND_DOWN]->bitmap->w - 10, box.y + 12, "X", FONT_ARIAL10, COLOR_WHITE, COLOR_BLACK, COLOR_HGOLD, COLOR_BLACK, 0))
	{
		popup_destroy_visible();
	}
}

/**
 * Handle mouse and keyboard events when a popup is active.
 * @param event Event.
 * @return 1 to disable any other mouse/keyboard actions, 0 otherwise. */
int popup_handle_event(SDL_Event *event)
{
	/* No popup is visible. */
	if (!popup_visible)
	{
		return 0;
	}

	/* Handle custom events? */
	if (popup_visible->event_func)
	{
		int ret = popup_visible->event_func(popup_visible, event);

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
			popup_destroy_visible();
		}
	}

	return 1;
}

/**
 * Get the currently visible popup.
 * @return The visible popup, or NULL if there isn't any. */
popup_struct *popup_get_visible()
{
	return popup_visible;
}

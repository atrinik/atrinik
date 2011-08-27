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
 * The scroll buttons API.
 *
 * Scroll buttons are basically simplified scrollbars which are often
 * more user friendly than scrollbars. They allow the user to keep
 * holding them in order to scroll, instead of having to drag a
 * scrollbar, which is often imprecise.
 *
 * Scroll buttons consist of 4 buttons:
 *
 * - ^x: Scrolls a custom number of lines up per click (often the whole
 *       visible page).
 * - ^: Scrolls one line up per click.
 * - v: Scrolls one line down per click.
 * - vX: Scrolls a custom number of lines down per click (often the whole
 *       visible page). */

#include <global.h>

static SDL_Color scrollbar_color_bg;
static SDL_Color scrollbar_color_fg;
static SDL_Color scrollbar_color_highlight;

/**
 * Initialize the scrollbar API. */
void scrollbar_init()
{
	text_color_parse("000000", &scrollbar_color_bg);
	text_color_parse("b5a584", &scrollbar_color_fg);
	text_color_parse("ffffff", &scrollbar_color_highlight);
}

static void scrollbar_element_init(scrollbar_element *elem, int x, int y, int w, int h)
{
	elem->x = x;
	elem->y = y;
	elem->w = w;
	elem->h = h;
}

static void scrollbar_element_render_background(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem)
{
	(void) elem;
	SDL_FillRect(surface, box, SDL_MapRGB(surface->format, scrollbar_color_bg.r, scrollbar_color_bg.g, scrollbar_color_bg.b));
	border_create_sdl_color(surface, box, &scrollbar_color_fg);
}

static void scrollbar_element_render_arrow_up(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem)
{
	SDL_Color *color = &scrollbar_color_fg;

	if (elem->highlight)
	{
		color = &scrollbar_color_highlight;
	}

	border_create_sdl_color(surface, box, color);
	lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + box->w / 2, box->y + box->h - 3, color->r, color->g, color->b, 255);
	lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + 2, box->y + box->h / 2, color->r, color->g, color->b, 255);
	lineRGBA(surface, box->x + box->w / 2, box->y + 2, box->x + box->w - 2 - 1, box->y + box->h / 2, color->r, color->g, color->b, 255);
}

static void scrollbar_element_render_arrow_down(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem)
{
	SDL_Color *color = &scrollbar_color_fg;

	if (elem->highlight)
	{
		color = &scrollbar_color_highlight;
	}

	border_create_sdl_color(surface, box, color);
	lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + box->w / 2, box->y + 2, color->r, color->g, color->b, 255);
	lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + 2, box->y + box->h / 2, color->r, color->g, color->b, 255);
	lineRGBA(surface, box->x + box->w / 2, box->y + box->h - 3, box->x + box->w - 2 - 1, box->y + box->h / 2, color->r, color->g, color->b, 255);
}

static void scrollbar_element_render_slider(SDL_Surface *surface, SDL_Rect *box, scrollbar_element *elem)
{
	SDL_FillRect(surface, box, SDL_MapRGB(surface->format, scrollbar_color_fg.r, scrollbar_color_fg.g, scrollbar_color_fg.b));

	if (elem->highlight)
	{
		border_create_sdl_color(surface, box, &scrollbar_color_highlight);
	}
}

static int scrollbar_element_highlight_check(scrollbar_struct *scrollbar, int mx, int my, scrollbar_element *elem)
{
	SDL_Rect box;

	box.x = scrollbar->x + elem->x;
	box.y = scrollbar->y + elem->y;
	box.w = elem->w;
	box.h = elem->h;

	if (mx >= box.x && mx < box.x + box.w && my >= box.y && my < box.y + box.h)
	{
		elem->highlight = 1;
		return 1;
	}
	else
	{
		elem->highlight = 0;
		return 0;
	}
}

static void scrollbar_element_render(scrollbar_struct *scrollbar, SDL_Surface *surface, scrollbar_element *elem)
{
	SDL_Rect box;

	box.x = scrollbar->x + elem->x;
	box.y = scrollbar->y + elem->y;
	box.w = elem->w;
	box.h = elem->h;

	if (elem->highlight)
	{
		int mx, my;

		SDL_GetMouseState(&mx, &my);

		mx -= scrollbar->px;
		my -= scrollbar->py;

		scrollbar_element_highlight_check(scrollbar, mx, my, elem);
	}

	elem->render_func(surface, &box, elem);
}

/**
 * Initialize a single scrollbar structure.
 * @param scrollbar Structure to initialize.
 * @param w Width of the scrollbar. Should be an odd number, otherwise
 * the arrow calculations will not work correctly.
 * @param h Height of the scrollbar. */
void scrollbar_create(scrollbar_struct *scrollbar, int w, int h, uint32 *scroll_offset, uint32 *num_lines, uint32 max_lines)
{
	memset(scrollbar, 0, sizeof(*scrollbar));

	scrollbar->scroll_offset = scroll_offset;
	scrollbar->num_lines = num_lines;
	scrollbar->max_lines = max_lines;

	scrollbar_element_init(&scrollbar->background, 0, 0, w, h);
	scrollbar_element_init(&scrollbar->arrow_up, 0, 0, w, w);
	scrollbar_element_init(&scrollbar->arrow_down, 0, h - w, w, w);
	scrollbar_element_init(&scrollbar->slider, 2, SLIDER_YPOS_START(scrollbar), w - 2 * 2, SLIDER_HEIGHT_FULL(scrollbar));

	scrollbar->background.render_func = scrollbar_element_render_background;
	scrollbar->arrow_up.render_func = scrollbar_element_render_arrow_up;
	scrollbar->arrow_down.render_func = scrollbar_element_render_arrow_down;
	scrollbar->slider.render_func = scrollbar_element_render_slider;
}

void scrollbar_scroll_adjust(scrollbar_struct *scrollbar, int adjust)
{
	int scroll;

	/* Adjust the scroll offset. */
	scroll = *scrollbar->scroll_offset + adjust;

	/* Make sure the scroll offset is in a valid range. */
	if (scroll < 0)
	{
		scroll = 0;
	}
	else if ((uint32) scroll > *scrollbar->num_lines - scrollbar->max_lines)
	{
		scroll = *scrollbar->num_lines - scrollbar->max_lines;
	}

	/* If the scroll offset changed, update it and set the redraw flag,
	 * if possible. */
	if ((uint32) scroll != *scrollbar->scroll_offset)
	{
		*scrollbar->scroll_offset = scroll;

		if (scrollbar->redraw)
		{
			*scrollbar->redraw = 1;
		}
	}
}

static int scrollbar_click_scroll(scrollbar_struct *scrollbar)
{
	if (scrollbar->dragging)
	{
		return 0;
	}

	if (scrollbar->arrow_up.highlight)
	{
		scrollbar_scroll_adjust(scrollbar, -1);
		return 1;
	}
	else if (scrollbar->arrow_down.highlight)
	{
		scrollbar_scroll_adjust(scrollbar, 1);
		return 1;
	}
	else if (scrollbar->background.highlight && scrollbar->scroll_direction != SCROLL_DIRECTION_NONE)
	{
		if (scrollbar->scroll_direction == SCROLL_DIRECTION_UP)
		{
			scrollbar_scroll_adjust(scrollbar, -scrollbar->max_lines);
			return 1;
		}
		else if (scrollbar->scroll_direction == SCROLL_DIRECTION_DOWN)
		{
			scrollbar_scroll_adjust(scrollbar, scrollbar->max_lines);
			return 1;
		}
	}

	return 0;
}

void scrollbar_render(scrollbar_struct *scrollbar, SDL_Surface *surface, int x, int y)
{
	int mx, my;

	scrollbar->x = x;
	scrollbar->y = y;
	scrollbar->surface = surface;

	if (scrollbar->scroll_direction != SCROLL_DIRECTION_NONE && SDL_GetMouseState(NULL, NULL) != SDL_BUTTON_LEFT)
	{
		scrollbar->scroll_direction = SCROLL_DIRECTION_NONE;
	}

	if (SDL_GetMouseState(&mx, &my) == SDL_BUTTON_LEFT && SDL_GetTicks() - scrollbar->click_ticks > scrollbar->click_repeat_ticks)
	{
		if (scrollbar_click_scroll(scrollbar))
		{
			scrollbar->click_ticks = SDL_GetTicks();
			scrollbar->click_repeat_ticks = 35;
		}
	}

	if (*scrollbar->num_lines > scrollbar->max_lines)
	{
		int scroll;

		scroll = scrollbar->max_lines + *scrollbar->scroll_offset;

		scrollbar->slider.h = SLIDER_HEIGHT_FULL(scrollbar) * scrollbar->max_lines / *scrollbar->num_lines;
		scrollbar->slider.y = ((scroll - scrollbar->max_lines) * SLIDER_HEIGHT_FULL(scrollbar)) / *scrollbar->num_lines;

		if (scrollbar->slider.h < 1)
		{
			scrollbar->slider.h = 1;
		}

		if (scroll - scrollbar->max_lines > 0 && scrollbar->slider.y + scrollbar->slider.h < SLIDER_HEIGHT_FULL(scrollbar))
		{
			scrollbar->slider.y++;
		}
	}
	else
	{
		scrollbar->slider.h = SLIDER_HEIGHT_FULL(scrollbar);
		scrollbar->slider.y = 0;
	}

	scrollbar->slider.y += SLIDER_YPOS_START(scrollbar);

	scrollbar_element_render(scrollbar, surface, &scrollbar->background);
	scrollbar_element_render(scrollbar, surface, &scrollbar->arrow_up);
	scrollbar_element_render(scrollbar, surface, &scrollbar->arrow_down);
	scrollbar_element_render(scrollbar, surface, &scrollbar->slider);
}

int scrollbar_event(scrollbar_struct *scrollbar, SDL_Event *event)
{
	if (event->type == SDL_MOUSEMOTION)
	{
		if (scrollbar->dragging && !(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LEFT))
		{
			scrollbar->dragging = 0;
			return 0;
		}

		if (scrollbar->dragging)
		{
			int slider_y;
			uint32 scroll_offset;

			slider_y = event->motion.y - scrollbar->old_slider_pos;

			if (slider_y > SLIDER_HEIGHT_FULL(scrollbar) - scrollbar->slider.h)
			{
				slider_y = SLIDER_HEIGHT_FULL(scrollbar) - scrollbar->slider.h;
			}
			else if (slider_y < 0)
			{
				slider_y = 0;
			}

			scroll_offset = MIN(*scrollbar->num_lines - scrollbar->max_lines, MAX(0, slider_y) * *scrollbar->num_lines / SLIDER_HEIGHT_FULL(scrollbar));

			if (scroll_offset != *scrollbar->scroll_offset)
			{
				*scrollbar->scroll_offset = scroll_offset;

				if (scrollbar->redraw)
				{
					*scrollbar->redraw = 1;
				}
			}

			return 1;
		}
		else if (scrollbar_element_highlight_check(scrollbar, event->motion.x, event->motion.y, &scrollbar->arrow_up))
		{
			return 1;
		}
		else if (scrollbar_element_highlight_check(scrollbar, event->motion.x, event->motion.y, &scrollbar->arrow_down))
		{
			return 1;
		}
		else if (scrollbar_element_highlight_check(scrollbar, event->motion.x, event->motion.y, &scrollbar->slider))
		{
			return 1;
		}
		else if (scrollbar_element_highlight_check(scrollbar, event->motion.x, event->motion.y, &scrollbar->background))
		{
			return 1;
		}
	}
	else if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (scrollbar->slider.highlight)
		{
			scrollbar->old_slider_pos = event->motion.y - scrollbar->slider.y + SLIDER_YPOS_START(scrollbar);
			scrollbar->dragging = 1;
			return 1;
		}
		else if (scrollbar->background.highlight)
		{
			if (event->motion.y < scrollbar->y + scrollbar->slider.y)
			{
				scrollbar->scroll_direction = SCROLL_DIRECTION_UP;
			}
			else if (event->motion.y > scrollbar->y + scrollbar->slider.y + scrollbar->slider.h)
			{
				scrollbar->scroll_direction = SCROLL_DIRECTION_DOWN;
			}
		}

		if (scrollbar_click_scroll(scrollbar))
		{
			scrollbar->click_ticks = SDL_GetTicks();
			scrollbar->click_repeat_ticks = 400;
			return 1;
		}
	}

	return 0;
}

/**
 * Show one scroll button.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param pos Pointer to integer storing the current position.
 * @param max_pos Maximum possible value for 'pos'.
 * @param advance How many rows to advance when using the custom-scroll
 * buttons.
 * @param box Positions for the box. */
static void scroll_buttons_show_one(SDL_Surface *surface, int x, int y, int *pos, int max_pos, int advance, SDL_Rect *box)
{
	int state, mx, my;

	/* Draw the button's borders. */
	draw_frame(surface, box->x - 1, box->y - 1, box->w + 1, box->h + 1);

	state = SDL_GetMouseState(&mx, &my);

	/* Visual feedback so the user knows whether there is anything more
	 * further up/down so they don't have to bother clicking the buttons
	 * to find out. */
	if (*pos + MAX(-1, MIN(1, advance)) < 0 || *pos + MAX(-1, MIN(1, advance)) > max_pos)
	{
		SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 123, 115, 115));
	}
	/* Mouse over the button? */
	else if (mx > x && mx < x + box->w && my > y && my < y + box->h)
	{
		/* Clicked? */
		if (state == SDL_BUTTON(SDL_BUTTON_LEFT))
		{
			/* Advance the position pointer. */
			*pos += advance;
			SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 119, 106, 77));
		}
		else
		{
			SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 147, 133, 103));
		}
	}
	else
	{
		SDL_FillRect(surface, box, SDL_MapRGB(surface->format, 181, 165, 132));
	}
}

/**
 * Show scroll buttons.
 * @param surface Surface to use.
 * @param x X position.
 * @param y Y position.
 * @param pos Pointer to integer storing the current position.
 * @param max_pos Maximum possible value for 'pos'.
 * @param advance How many rows to advance when using the custom-scroll
 * buttons.
 * @param box Box containing coordinates, usually same as x/y.
 * @return 1 if the scroll value changed, 0 otherwise. */
int scroll_buttons_show(SDL_Surface *surface, int x, int y, int *pos, int max_pos, int advance, SDL_Rect *box)
{
	_BLTFX bltfx;
	int old_pos;

	old_pos = *pos;
	bltfx.surface = surface;
	bltfx.flags = 0;
	bltfx.alpha = 0;

	box->h = 20;
	box->w = 20;

	/* Show each of the buttons, color them depending on the mouse's
	 * status, and show the arrow's bitmap. */
	scroll_buttons_show_one(surface, x, y, pos, max_pos, -advance, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_UP2], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_UP2]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_UP2]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, -1, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_UP], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_UP]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_UP]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, 1, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_DOWN], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_DOWN]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_DOWN]->bitmap->h / 2, NULL, &bltfx);
	y += 30;
	box->y += 30;
	scroll_buttons_show_one(surface, x, y, pos, max_pos, advance, box);
	sprite_blt(Bitmaps[BITMAP_ARROW_DOWN2], box->x + box->w / 2 - Bitmaps[BITMAP_ARROW_DOWN2]->bitmap->w / 2, box->y + box->h / 2 - Bitmaps[BITMAP_ARROW_DOWN2]->bitmap->h / 2, NULL, &bltfx);

	/* Check out of bounds values. */
	if (*pos > max_pos)
	{
		*pos = max_pos;
	}

	if (*pos < 0)
	{
		*pos = 0;
	}

	return *pos != old_pos;
}

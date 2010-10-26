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
 * This file handles all text window related functions */

#include <include.h>

int textwin_flags;

static int old_slider_pos = 0;
SDL_Surface *txtwinbg = NULL;
int old_txtwin_alpha = -1;

/**
 * Makes sure textwin's scroll value is a sane number.
 * @param widget Text window's widget. */
static void textwin_scroll_adjust(widgetdata *widget)
{
	_textwin *textwin = TEXTWIN(widget);

	if (textwin->scroll < TEXTWIN_ROWS_VISIBLE(widget))
	{
		textwin->scroll = TEXTWIN_ROWS_VISIBLE(widget);
	}

	if (textwin->scroll > (int) textwin->num_entries)
	{
		textwin->scroll = textwin->num_entries;
	}
}

/**
 * Readjust text window's scroll/entries counts due to a font size
 * change.
 * @param widget Text window's widget. */
static void textwin_readjust(widgetdata *widget)
{
	_textwin *textwin = TEXTWIN(widget);
	SDL_Rect box;
	int scroll;

	if (!textwin->entries)
	{
		return;
	}

	box.w = widget->wd - Bitmaps[BITMAP_SLIDER]->bitmap->w - 12 - 1;
	box.h = 0;
	box.x = 0;
	box.y = 0;
	string_blt(NULL, textwin->font, textwin->entries, 0, 0, COLOR_SIMPLE(COLOR_WHITE), TEXTWIN_TEXT_FLAGS(widget) | TEXT_HEIGHT, &box);
	scroll = box.h / FONT_HEIGHT(textwin->font);

	/* Adjust the counts. */
	textwin->scroll = scroll;
	textwin->num_entries = scroll;
}

/**
 * Draw info with format arguments.
 * @param flags Various flags, like color.
 * @param format Format arguments. */
void draw_info_format(int flags, char *format, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	draw_info(buf, flags);
}

/**
 * Add string to the text window.
 * @param str The string.
 * @param flags Various flags, like color. */
void draw_info(const char *str, int flags)
{
	widgetdata *widget;
	_textwin *textwin;
	size_t len, scroll;
	SDL_Rect box;

	if (flags & NDI_PLAYER)
	{
		widget = cur_widget[CHATWIN_ID];
	}
	else
	{
		widget = cur_widget[MSGWIN_ID];
	}

	textwin = TEXTWIN(widget);
	WIDGET_REDRAW(widget);

	box.w = widget->wd - Bitmaps[BITMAP_SLIDER]->bitmap->w - 12 - 1;
	box.h = 0;

	/* Have the entries gone over maximum allowed lines? */
	if (textwin->entries && textwin->num_entries >= (size_t) options.chat_max_lines)
	{
		char *cp = strchr(textwin->entries, '\n');

		/* Found the first newline. */
		if (cp)
		{
			size_t pos = cp - textwin->entries + 1;
			char *buf = malloc(pos + 1);

			/* Copy the string together with the newline to a temporary
			 * buffer. */
			memcpy(buf, textwin->entries, pos);
			buf[pos] = '\0';
			/* Get the string's height. */
			string_blt(NULL, textwin->font, buf, 0, 0, COLOR_SIMPLE(COLOR_WHITE), TEXTWIN_TEXT_FLAGS(widget) | TEXT_HEIGHT, &box);
			scroll = box.h / FONT_HEIGHT(textwin->font);
			box.h = 0;
			free(buf);

			/* Move the string after the found newline to the beginning,
			 * effectively erasing the previous line. */
			textwin->entries_size -= pos;
			memcpy(textwin->entries, textwin->entries + pos, textwin->entries_size);
			textwin->entries[textwin->entries_size] = '\0';

			/* Adjust the counts. */
			textwin->scroll -= scroll;
			textwin->num_entries -= scroll;
		}
	}

	len = strlen(str);
	/* Resize the characters array as needed. */
	textwin->entries = realloc(textwin->entries, textwin->entries_size + len + 4);
	/* Tells the text module what color to use. */
	textwin->entries[textwin->entries_size] = '\r';
	textwin->entries[textwin->entries_size + 1] = (flags & 0xff) + 1;
	textwin->entries_size += 2;
	/* Add the string, newline and terminating \0. */
	strcpy(textwin->entries + textwin->entries_size, str);
	textwin->entries[textwin->entries_size + len] = '\n';
	textwin->entries[textwin->entries_size + len + 1] = '\0';
	textwin->entries_size += len + 1;

	/* Get the string's height. */
	string_blt(NULL, textwin->font, str, 0, 0, COLOR_SIMPLE(COLOR_WHITE), TEXTWIN_TEXT_FLAGS(widget) | TEXT_HEIGHT, &box);
	scroll = box.h / FONT_HEIGHT(textwin->font) + 1;

	/* Adjust the counts. */
	textwin->scroll += scroll;
	textwin->num_entries += scroll;
}

/**
 * Draw a text window.
 * @param actWin The text window ID.
 * @param x X position of the text window.
 * @param y Y position of the text window.
 * @param bltfx The surface. */
static void show_window(widgetdata *widget, int x, int y, _BLTFX *bltfx)
{
	_textwin *textwin = TEXTWIN(widget);
	SDL_Rect box;
	int temp;

	textwin->x = x;
	textwin->y = y;
	x = y = 0;

	/* Show the text entries, if any. */
	if (textwin->entries)
	{
		box.w = widget->wd - Bitmaps[BITMAP_SLIDER]->bitmap->w - 12 - 1;
		box.h = widget->ht - 1;
		box.y = MAX(0, textwin->scroll - TEXTWIN_ROWS_VISIBLE(widget));
		string_blt(bltfx->surface, textwin->font, textwin->entries, x + 1, y + 1, COLOR_SIMPLE(COLOR_WHITE), TEXTWIN_TEXT_FLAGS(widget) | TEXT_HEIGHT, &box);
	}

	/* Only draw scrollbar if needed */
	if (textwin->num_entries > (size_t) TEXTWIN_ROWS_VISIBLE(widget))
	{
		SDL_Rect box;

		box.x = box.y = 0;
		box.w = Bitmaps[BITMAP_SLIDER]->bitmap->w;
		box.h = textwin->size * 10 + 1;

		/* No textinput-line */
		temp = -9;
		sprite_blt(Bitmaps[BITMAP_SLIDER_UP], x + widget->wd - 11, y + 2, NULL, bltfx);
		sprite_blt(Bitmaps[BITMAP_SLIDER_DOWN], x + widget->wd - 11, y + 13 + temp + textwin->size * 10, NULL, bltfx);
		sprite_blt(Bitmaps[BITMAP_SLIDER], x + widget->wd - 11, y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2 + temp, &box, bltfx);
		box.h += temp - 2;
		box.w -= 2;

		/* Between 0.0 <-> 1.0 */
		textwin->slider_h = box.h * TEXTWIN_ROWS_VISIBLE(widget) / textwin->num_entries;
		textwin->slider_y = ((textwin->scroll - TEXTWIN_ROWS_VISIBLE(widget)) * box.h) / textwin->num_entries;

		if (textwin->slider_h < 20)
		{
			textwin->slider_h = 20;
		}

		if (textwin->scroll - TEXTWIN_ROWS_VISIBLE(widget) > 0 && textwin->slider_y + textwin->slider_h < box.h)
		{
			textwin->slider_y++;
		}

		box.h = textwin->slider_h;
		sprite_blt(Bitmaps[BITMAP_TWIN_SCROLL], x + widget->wd - 9, y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + textwin->slider_y, &box, bltfx);

		if (textwin->highlight == TW_HL_UP)
		{
			box.x = x + widget->wd - 11;
			box.y = y + 2;
			box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
			box.w = 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			box.h = 1;
			box.x = x + widget->wd - 11;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.y += Bitmaps[BITMAP_SLIDER_UP]->bitmap->h - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
		}
		else if (textwin->highlight == TW_ABOVE)
		{
			box.x = x + widget->wd - 9;
			box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2;
			box.h = textwin->slider_y + 1;
			box.w = 5;
			SDL_FillRect(bltfx->surface, &box, 0);
		}
		else if (textwin->highlight == TW_HL_SLIDER)
		{
			box.x = x + widget->wd - 9;
			box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + textwin->slider_y;
			box.w = 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x += 4;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x -= 4;
			box.h = 1;
			box.w = 4;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.y += textwin->slider_h - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
		}
		else if (textwin->highlight == TW_UNDER)
		{
			box.x = x + widget->wd - 9;
			box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + textwin->slider_y + box.h;
			box.h = textwin->size * 10 - textwin->slider_y - textwin->slider_h - 10;
			box.w = 5;
			SDL_FillRect(bltfx->surface, &box, 0);
		}
		else if (textwin->highlight == TW_HL_DOWN)
		{
			box.x = x + widget->wd - 11;
			box.y = y + textwin->size * 10 + 4;
			box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
			box.w = 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			box.h = 1;
			box.x = x + widget->wd - 11;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.y += Bitmaps[BITMAP_SLIDER_UP]->bitmap->h - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
		}
	}
	else
	{
		textwin->slider_h = 0;
	}
}

/**
 * Display the message text window, without handling scrollbar/mouse
 * actions.
 * @param x X position.
 * @param y Y position.
 * @param w Maximum width.
 * @param h Maximum height. */
void textwin_show(int x, int y, int w, int h)
{
	widgetdata *widget = cur_widget[MSGWIN_ID];
	_textwin *textwin = TEXTWIN(widget);
	SDL_Rect box;

	box.x = x;
	box.y = y;
	box.w = w;
	box.h = h;
	SDL_FillRect(ScreenSurface, &box, 0);
	draw_frame(ScreenSurface, x, y, box.w, box.h);

	box.w = w - 1;
	box.h = h - 1;
	box.y = MAX(0, (int) textwin->num_entries - (h / FONT_HEIGHT(textwin->font)));

	string_blt(ScreenSurface, textwin->font, textwin->entries, x + 1, y + 1, COLOR_SIMPLE(COLOR_WHITE), TEXTWIN_TEXT_FLAGS(widget) | TEXT_HEIGHT, &box);
}

/**
 * Display widget text windows.
 * @param widget The widget object. */
void widget_textwin_show(widgetdata *widget)
{
	int len;
	SDL_Rect box, box2;
	_BLTFX bltfx;
	_textwin *textwin = TEXTWIN(widget);
	int x = widget->x1;
	int y = widget->y1;

	/* sanity check */
	if (!textwin)
		return;

	box.x = box.y = 0;
	box.w = widget->wd;
	box.h = len = textwin->size * 10 + 13;

	/* If we don't have a backbuffer, create it */
	if (!widget->widgetSF)
	{
		/* Need to do this, or the foreground could be semi-transparent too. */
		SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, 255);
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->format, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->flags);
		SDL_SetColorKey(widget->widgetSF, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(widget->widgetSF->format, 0, 0, 0));
	}

	if (old_txtwin_alpha != options.textwin_alpha)
	{
		if (txtwinbg)
		{
			SDL_FreeSurface(txtwinbg);
		}

		SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, options.textwin_alpha);
		txtwinbg = SDL_DisplayFormatAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap);
		SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, 255);
		old_txtwin_alpha = options.textwin_alpha;

		WIDGET_REDRAW(widget);
	}

	box2.x = x;
	box2.y = y;
	SDL_BlitSurface(txtwinbg, &box, ScreenSurface, &box2);

	/* Let's draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		widget->redraw = 0;

		SDL_FillRect(widget->widgetSF, NULL, SDL_MapRGBA(widget->widgetSF->format, 0, 0, 0, options.textwin_alpha));

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;
		box.x = 0;
		box.y = 0;
		box.h = 1;
		box.w = widget->wd;
		SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
		box.y = len;
		box.h = 1;
		box.x = 0;
		box.w = widget->wd;
		SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
		box.w = widget->wd;
		box.x = box.w - 1;
		box.w = 1;
		box.y = 0;
		box.h = len;
		SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
		box.x = 0;
		box.y = 0;
		box.h = len;
		box.w = 1;
		SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));

		show_window(widget, x, y - 2, &bltfx);
	}

	box.x = x;
	box.y = y;
	box2.x = 0;
	box2.y = 0;
	box2.w = widget->wd;
	box2.h = len = textwin->size * 10 + 14;

	SDL_BlitSurface(widget->widgetSF, &box2, ScreenSurface, &box);
}

/**
 * Handle mouse button events inside the text window.
 * @param actWin The text window.
 * @param event SDL event type. */
void textwin_button_event(widgetdata *widget, SDL_Event event)
{
	_textwin *textwin = TEXTWIN(widget);

	/* sanity check */
	if (!textwin)
	{
		return;
	}

	/* Scrolling or resizing */
	if (event.motion.x < widget->x1 || (textwin_flags & (TW_SCROLL | TW_RESIZE | TW_RESIZE2)))
	{
		return;
	}

	/* Mousewheel up */
	if (event.button.button == 4)
	{
		textwin->scroll--;
		WIDGET_REDRAW(widget);
	}
	/* Mousewheel down */
	else if (event.button.button == 5)
	{
		textwin->scroll++;
		WIDGET_REDRAW(widget);
	}
	else if (event.button.button == SDL_BUTTON_LEFT)
	{
		WIDGET_REDRAW(widget);

		/* Clicked scroller button up */
		if (textwin->highlight == TW_HL_UP)
		{
			textwin->scroll--;
		}
		/* Clicked above the slider */
		else if (textwin->highlight == TW_ABOVE)
		{
			textwin->scroll -= textwin->size;
		}
		else if (textwin->highlight == TW_HL_SLIDER)
		{
			/* Clicked on the slider */
			textwin_flags |= TW_SCROLL;
			textwin->highlight = TW_HL_SLIDER;
			old_slider_pos = event.motion.y - textwin->slider_y;
		}
		/* Clicked under the slider */
		else if (textwin->highlight == TW_UNDER)
		{
			textwin->scroll += textwin->size;
		}
		/* Clicked scroller button down */
		else if (textwin->highlight == TW_HL_DOWN)
		{
			textwin->scroll++;
		}
		/* Size change */
		else if (event.motion.x < widget->x1 + widget->wd - 15 && event.motion.y > widget->y1 + 2 && event.motion.y < widget->y1 + 7 && cursor_type == 1)
		{
			textwin_flags |= TW_RESIZE;
		}
		else if (cursor_type == 2 && event.motion.x > widget->x1 + 2 && event.motion.x < widget->x1 + 7 && event.motion.y > widget->y1 && event.motion.y < widget->y1 + widget->ht)
		{
			textwin_flags |= TW_RESIZE2;
		}
	}

	textwin_scroll_adjust(widget);
}

/**
 * Mouse move event inside the text window.
 * @param actWin The text window.
 * @param event SDL event type.
 * @return 1 if mouse is out of window, 0 otherwise. */
int textwin_move_event(widgetdata *widget, SDL_Event event)
{
	_textwin *textwin = TEXTWIN(widget);

	if (!textwin)
	{
		return 1;
	}

	if (textwin->highlight != TW_HL_NONE)
	{
		textwin->highlight = TW_HL_NONE;
		WIDGET_REDRAW(widget);
	}

	/* Show resize cursor */
	if ((event.motion.x > widget->x1 + 2 && event.motion.x < widget->x1 + 7 && event.motion.y > widget->y1 && event.motion.y < widget->y1 + widget->ht) || (event.button.button == SDL_BUTTON_LEFT && (textwin_flags & TW_RESIZE2)))
	{
		if (!(textwin_flags & TW_SCROLL))
		{
			cursor_type = 2;
		}
	}
	else if ((event.motion.y > widget->y1 + 2 && event.motion.y < widget->y1 + 7 && (event.motion.x < widget->x1 + widget->wd - 15)) || (event.button.button == SDL_BUTTON_LEFT && (textwin_flags & (TW_SCROLL | TW_RESIZE))))
	{
		if (!(textwin_flags & TW_SCROLL) && event.motion.x > widget->x1)
		{
			cursor_type = 1;
		}
	}
	else
	{
		cursor_type = 0;
		textwin_flags &= ~(TW_SCROLL | TW_RESIZE | TW_RESIZE2);
	}

	/* Mouse out of window */
	/* We have to leave this here! For sanity, also the widget stuff does some area checking */
	if (event.motion.y < widget->y1 || event.motion.x > widget->x1 + widget->wd || event.motion.y > widget->y1 + textwin->size * 10 + 13 + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
	{
		if (!(textwin_flags & (TW_RESIZE | TW_RESIZE2)))
		{
			return 1;
		}
	}

	/* Highlighting */
	if (event.motion.x > widget->x1 + widget->wd - 11 && event.motion.y > widget->y1 && event.motion.x < widget->x1 + widget->wd)
	{
#define OFFSET (textwin->y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
		if (event.motion.y < OFFSET)
		{
			textwin->highlight = TW_HL_UP;
		}
		else if (event.motion.y < OFFSET + textwin->slider_y)
		{
			textwin->highlight = TW_ABOVE;
		}
		else if (event.motion.y < OFFSET + textwin->slider_y + textwin->slider_h + 3)
		{
			textwin->highlight = TW_HL_SLIDER;
		}
		else if (event.motion.y < widget->y1 + textwin->size * 10 + 4)
		{
			textwin->highlight = TW_UNDER;
		}
		else if (event.motion.y < widget->y1 + textwin->size * 10 + 13)
		{
			textwin->highlight = TW_HL_DOWN;
		}
#undef OFFSET

		WIDGET_REDRAW(widget);

		if (event.button.button != SDL_BUTTON_LEFT)
		{
			return 0;
		}
	}

	/* Slider scrolling */
	if (textwin_flags & TW_SCROLL)
	{
		textwin->slider_y = event.motion.y - old_slider_pos;

		textwin->scroll = TEXTWIN_ROWS_VISIBLE(widget) + MAX(0, textwin->slider_y) * textwin->num_entries / (widget->ht - 20);
		textwin_scroll_adjust(widget);

		WIDGET_REDRAW(widget);

		return 0;
	}

	/* Resizing */
	if (textwin_flags & (TW_RESIZE | TW_RESIZE2))
	{
		if (cursor_type == 1)
		{
			int newsize = ((widget->y1 + widget->ht) - event.motion.y) / 10;

			/* set minimum bounds */
			if (newsize < 3)
			{
				newsize = 3;
			}

			/* we need to calc the new x for the widget, and set the new size */
			resize_widget(widget, RESIZE_TOP, newsize * 10 + 13);

			textwin->size = newsize;
			textwin_scroll_adjust(widget);
			WIDGET_REDRAW(widget);
		}
		else if (cursor_type == 2)
		{
			resize_widget(widget, RESIZE_LEFT, MIN(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->w, MAX(30, widget->x1 - event.motion.x + widget->wd)));
			textwin_readjust(widget);
			WIDGET_REDRAW(widget);
		}
	}

	return 0;
}

/**
 * Handle text window mouse events.
 * @param e Event.
 * @param event SDL event type.
 * @param WidgetID Widget ID. */
void textwin_event(int e, SDL_Event *event, widgetdata *widget)
{
	if (e == TW_CHECK_BUT_DOWN)
	{
		textwin_button_event(widget, *event);
	}
	else
	{
		textwin_move_event(widget, *event);
	}
}

/**
 * Change the font used for drawing text in text windows.
 * @param font ID of the font to change to. */
void change_textwin_font(int font)
{
	font = FONT_ARIAL10 + font;

	if (TEXTWIN(cur_widget[MSGWIN_ID])->font != font)
	{
		TEXTWIN(cur_widget[MSGWIN_ID])->font = font;
		textwin_readjust(cur_widget[MSGWIN_ID]);
		WIDGET_REDRAW(cur_widget[MSGWIN_ID]);
	}

	if (TEXTWIN(cur_widget[CHATWIN_ID])->font != font)
	{
		TEXTWIN(cur_widget[CHATWIN_ID])->font = font;
		textwin_readjust(cur_widget[CHATWIN_ID]);
		WIDGET_REDRAW(cur_widget[CHATWIN_ID]);
	}
}

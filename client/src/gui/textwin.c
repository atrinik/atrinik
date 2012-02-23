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
 * Implements text window widgets.
 *
 * @author Alex Tokar */

#include <global.h>

/** Color to use for the text window border. */
static Uint32 textwin_border_color;
/**
 * Color to use for the text window border when the mouse is hovering
 * over it. */
static Uint32 textwin_border_color_selected;

/**
 * Initialize text window variables. */
void textwin_init(void)
{
	textwin_border_color = SDL_MapRGB(ScreenSurface->format, 96, 96, 96);
	textwin_border_color_selected = SDL_MapRGB(ScreenSurface->format, 177, 126, 5);
}

/**
 * Readjust text window's scroll/entries counts due to a font size
 * change.
 * @param widget Text window's widget. */
void textwin_readjust(widgetdata *widget)
{
	textwin_struct *textwin = TEXTWIN(widget);

	if (textwin->entries)
	{
		SDL_Rect box;

		box.w = TEXTWIN_TEXT_WIDTH(widget);
		box.h = 0;
		box.x = 0;
		box.y = 0;
		string_blt(NULL, textwin->font, textwin->entries, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);

		/* Adjust the counts. */
		textwin->num_lines = box.h - 1;
	}

	textwin_create_scrollbar(widget);
	scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
	WIDGET_REDRAW(widget);
}

void draw_info_flags(const char *color, int flags, const char *str)
{
	widgetdata *widget;
	textwin_struct *textwin;
	size_t len, scroll;
	SDL_Rect box;
	uint32 bottom;

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
	bottom = SCROLL_BOTTOM(&textwin->scrollbar);
	box.w = TEXTWIN_TEXT_WIDTH(widget);
	box.h = 0;

	len = strlen(str);
	/* Resize the characters array as needed. */
	textwin->entries = realloc(textwin->entries, textwin->entries_size + len + 9);
	/* Tells the text module what color to use. */
	textwin->entries[textwin->entries_size] = '\r';
	textwin->entries[textwin->entries_size + 1] = color[0];
	textwin->entries[textwin->entries_size + 2] = color[1];
	textwin->entries[textwin->entries_size + 3] = color[2];
	textwin->entries[textwin->entries_size + 4] = color[3];
	textwin->entries[textwin->entries_size + 5] = color[4];
	textwin->entries[textwin->entries_size + 6] = color[5];
	textwin->entries_size += 7;
	/* Add the string, newline and terminating \0. */
	strcpy(textwin->entries + textwin->entries_size, str);
	textwin->entries[textwin->entries_size + len] = '\n';
	textwin->entries[textwin->entries_size + len + 1] = '\0';
	textwin->entries_size += len + 1;

	box.y = 0;
	/* Get the string's height. */
	string_blt(NULL, textwin->font, str, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
	scroll = box.h;

	/* Adjust the counts. */
	textwin->num_lines += scroll;

	/* Have the entries gone over maximum allowed lines? */
	if (textwin->entries && textwin->num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES))
	{
		char *cp;

		while (textwin->num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES) && (cp = strchr(textwin->entries, '\n')))
		{
			size_t pos = cp - textwin->entries + 1;
			char *buf = malloc(pos + 1);

			/* Copy the string together with the newline to a temporary
			 * buffer. */
			memcpy(buf, textwin->entries, pos);
			buf[pos] = '\0';

			/* Get the string's height. */
			box.h = 0;
			string_blt(NULL, textwin->font, buf, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
			scroll = box.h - 1;

			free(buf);

			/* Move the string after the found newline to the beginning,
			 * effectively erasing the previous line. */
			textwin->entries_size -= pos;
			memmove(textwin->entries, textwin->entries + pos, textwin->entries_size);
			textwin->entries[textwin->entries_size] = '\0';

			/* Adjust the counts. */
			textwin->num_lines -= scroll;
		}
	}

	if (textwin->scroll_offset == bottom)
	{
		scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
	}
}

/**
 * Draw info with format arguments.
 * @param flags Various flags, like color.
 * @param format Format arguments. */
void draw_info_format(const char *color, char *format, ...)
{
	char buf[HUGE_BUF * 2];
	va_list ap;

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	draw_info_flags(color, 0, buf);
}

/**
 * Add string to the text window.
 * @param flags Various flags, like color.
 * @param str The string. */
void draw_info(const char *color, const char *str)
{
	draw_info_flags(color, 0, str);
}

/**
 * Handle ctrl+C for textwin widget
 * @param widget The textwin widget. If NULL, try to find the first one
 * in the priority list. */
void textwin_handle_copy(widgetdata *widget)
{
	sint64 start, end;
	textwin_struct *textwin;
	char *str, *cp;
	size_t i, pos;

	if (!widget)
	{
		widget = widget_find_copy_from();

		if (!widget)
		{
			return;
		}
	}

	textwin = TEXTWIN(widget);

	start = textwin->selection_start;
	end = textwin->selection_end;

	if (end < start)
	{
		start = textwin->selection_end;
		end = textwin->selection_start;
	}

	if (end - start <= 0)
	{
		return;
	}

	/* Get the string to copy, depending on the start and end positions. */
	str = malloc(sizeof(char) * (end - start + 1 + 1));
	memcpy(str, textwin->entries + start, end - start + 1);
	str[end - start + 1] = '\0';

	cp = malloc(sizeof(char) * (end - start + 1 + 1));
	i = 0;

	/* Remove the special \r color changers. */
	for (pos = 0; pos < (size_t) (end - start + 1); pos++)
	{
		if (str[pos] == '\r')
		{
			pos += 6;
		}
		else
		{
			cp[i] = str[pos];
			i++;
		}
	}

	cp[i] = '\0';

	clipboard_set(cp);
	free(str);
	free(cp);
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
	textwin_struct *textwin = TEXTWIN(widget);
	SDL_Rect box;
	int scroll;

	box.w = w - 3;
	box.h = 0;
	box.x = 0;
	box.y = 0;
	string_blt(NULL, textwin->font, textwin->entries, 3, 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
	scroll = box.h;

	box.x = x;
	box.y = y;
	box.w = w;
	box.h = h;
	SDL_FillRect(ScreenSurface, &box, 0);
	draw_frame(ScreenSurface, x, y, box.w, box.h);

	box.w = w - 3;
	box.h = h;

	box.y = MAX(0, scroll - (h / FONT_HEIGHT(textwin->font)));

	string_blt(ScreenSurface, textwin->font, textwin->entries, x + 3, y + 1, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box);
}

/**
 * Initialize text window scrollbar.
 * @param widget The text window. */
void textwin_create_scrollbar(widgetdata *widget)
{
	textwin_struct *textwin = TEXTWIN(widget);

	scrollbar_create(&textwin->scrollbar, 9, TEXTWIN_TEXT_HEIGHT(widget), &textwin->scroll_offset, &textwin->num_lines, TEXTWIN_ROWS_VISIBLE(widget));
	textwin->scrollbar.redraw = &widget->redraw;
}

/**
 * Display widget text windows.
 * @param widget The widget object. */
void widget_textwin_show(widgetdata *widget)
{
	SDL_Rect box;
	textwin_struct *textwin = TEXTWIN(widget);
	int mx, my, alpha;

	/* Sanity check. */
	if (!textwin)
	{
		return;
	}

	/* If we don't have a backbuffer, create it */
	if (!widget->widgetSF || widget->wd != widget->widgetSF->w || widget->ht != widget->widgetSF->h)
	{
		if (widget->widgetSF)
		{
			SDL_FreeSurface(widget->widgetSF);
		}

		widget->widgetSF = SDL_CreateRGBSurface(get_video_flags(), widget->wd, widget->ht, video_get_bpp(), 0, 0, 0, 0);
		SDL_SetColorKey(widget->widgetSF, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
		textwin_readjust(widget);
	}

	if ((alpha = setting_get_int(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_TRANSPARENCY)))
	{
		filledRectAlpha(ScreenSurface, widget->x1, widget->y1, widget->x1 + widget->wd - 1, widget->y1 + widget->ht - 1, alpha);
	}

	/* Let's draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		SDL_FillRect(widget->widgetSF, NULL, 0);

		/* Show the text entries, if any. */
		if (textwin->entries)
		{
			SDL_Rect box_text;

			box_text.w = TEXTWIN_TEXT_WIDTH(widget);
			box_text.h = TEXTWIN_TEXT_HEIGHT(widget);
			box_text.y = textwin->scroll_offset;
			text_set_selection(&textwin->selection_start, &textwin->selection_end, &textwin->selection_started);
			string_blt(widget->widgetSF, textwin->font, textwin->entries, TEXTWIN_TEXT_STARTX(widget), TEXTWIN_TEXT_STARTY(widget), COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box_text);
			text_set_selection(NULL, NULL, NULL);
		}

		textwin->scrollbar.max_lines = TEXTWIN_ROWS_VISIBLE(widget);
		textwin->scrollbar.px = widget->x1;
		textwin->scrollbar.py = widget->y1;
		scrollbar_show(&textwin->scrollbar, widget->widgetSF, widget->wd - 1 - textwin->scrollbar.background.w, 1);

		widget->redraw = scrollbar_need_redraw(&textwin->scrollbar);
	}

	box.x = widget->x1;
	box.y = widget->y1;
	SDL_BlitSurface(widget->widgetSF, NULL, ScreenSurface, &box);

	SDL_GetMouseState(&mx, &my);

	if (mx < widget->x1 || mx > widget->x1 + widget->wd || my < widget->y1 || my > widget->y1 + widget->ht)
	{
		widget->resize_flags = 0;
	}

	BORDER_CREATE_TOP(ScreenSurface, widget->x1, widget->y1, widget->wd, widget->ht, widget->resize_flags & RESIZE_TOP ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_BOTTOM(ScreenSurface, widget->x1, widget->y1, widget->wd, widget->ht, widget->resize_flags & RESIZE_BOTTOM ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_LEFT(ScreenSurface, widget->x1, widget->y1, widget->wd, widget->ht, widget->resize_flags & RESIZE_LEFT ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_RIGHT(ScreenSurface, widget->x1, widget->y1, widget->wd, widget->ht, widget->resize_flags & RESIZE_RIGHT ? textwin_border_color_selected : textwin_border_color, 1);
}

/**
 * Handle text window mouse events.
 * @param event SDL event type.
 * @param WidgetID Widget ID. */
void textwin_event(widgetdata *widget, SDL_Event *event)
{
	textwin_struct *textwin = TEXTWIN(widget);

	if (!textwin)
	{
		return;
	}

	if (scrollbar_event(&textwin->scrollbar, event))
	{
		WIDGET_REDRAW(widget);
		return;
	}

	if (event->button.button == SDL_BUTTON_LEFT)
	{
		if (event->type == SDL_MOUSEBUTTONUP)
		{
			textwin->selection_started = 0;
		}
		else if (event->type == SDL_MOUSEBUTTONDOWN)
		{
			textwin->selection_started = 0;
			textwin->selection_start = -1;
			textwin->selection_end = -1;
			WIDGET_REDRAW(widget);
		}
		else if (event->type == SDL_MOUSEMOTION)
		{
			WIDGET_REDRAW(widget);
			textwin->selection_started = 1;
		}
	}

	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			scrollbar_scroll_adjust(&textwin->scrollbar, -1);
		}
		else if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			scrollbar_scroll_adjust(&textwin->scrollbar, 1);
		}
	}
}

/**
 * The 'Clear' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_clear(widgetdata *widget, int x, int y)
{
	textwin_struct *textwin;

	(void) x;
	(void) y;

	textwin = TEXTWIN(widget);
	free(textwin->entries);
	textwin->entries = NULL;
	textwin->num_lines = textwin->entries_size = textwin->scroll_offset = 0;
	WIDGET_REDRAW(widget);
}

/**
 * Handle the 'Copy' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_copy(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;

	textwin_handle_copy(widget);
}

/**
 * Adjust the specified text window widget's font size.
 * @param widget The text window.
 * @param adjust How much to adjust the size by. */
static void textwin_font_adjust(widgetdata *widget, int adjust)
{
	textwin_struct *textwin;
	int font;

	textwin = TEXTWIN(widget);
	font = MAX(FONT_ARIAL10, MIN(FONT_ARIAL16, textwin->font + adjust));

	if (textwin->font != font)
	{
		textwin->font = font;
		textwin_readjust(widget);
		WIDGET_REDRAW(widget);
	}
}

/**
 * The 'Increase Font Size' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_font_inc(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	textwin_font_adjust(widget, 1);
}

/**
 * The 'Decrease Font Size' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_font_dec(widgetdata *widget, int x, int y)
{
	(void) x;
	(void) y;
	textwin_font_adjust(widget, -1);
}

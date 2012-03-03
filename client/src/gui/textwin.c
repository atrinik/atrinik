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
const char *const textwin_tab_names[] =
{
	"[ALL]", "[GAME]", "[CHAT]", "[PUBLIC]", "[PRIVATE]", "[GUILD]", "[PARTY]"
};

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

	if (textwin->tabs[textwin->tab_selected].entries)
	{
		SDL_Rect box;

		box.w = TEXTWIN_TEXT_WIDTH(widget);
		box.h = 0;
		box.x = 0;
		box.y = 0;
		string_show(NULL, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);

		/* Adjust the counts. */
		textwin->tabs[textwin->tab_selected].num_lines = box.h - 1;
	}

	textwin_create_scrollbar(widget);
	scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
	WIDGET_REDRAW(widget);
}

static int textwin_tab_is_allowed(textwin_struct *textwin, const char *name)
{
	size_t i;

	for (i = 0; i < textwin->tabs_allowed_num; i++)
	{
		if (strcmp(textwin->tabs_allowed[i], name) == 0)
		{
			return 1;
		}
	}

	return 0;
}

static int textwin_tab_compare(const void *a, const void *b)
{
	textwin_tab_struct *tab_one, *tab_two;

	tab_one = (textwin_tab_struct *) a;
	tab_two = (textwin_tab_struct *) b;

	if (tab_one->type == CHAT_TYPE_PRIVATE && strcmp(tab_one->name, textwin_tab_names[tab_one->type]) != 0)
	{
		return strcmp(tab_one->name, tab_two->name);
	}

	return tab_one->type - tab_two->type;
}

static void textwin_tab_ensure(widgetdata *widget, textwin_struct *textwin, size_t type, const char *tab_name)
{
	size_t i;
	char buf[MAX_BUF];
	int w;

	for (i = 0; i < textwin->tabs_num; i++)
	{
		if (textwin->tabs[i].type == type && strcmp(textwin->tabs[i].name, tab_name) == 0)
		{
			return;
		}
	}

	textwin->tabs = memory_reallocz(textwin->tabs, sizeof(*textwin->tabs) * textwin->tabs_num, sizeof(*textwin->tabs) * (textwin->tabs_num + 1));
	textwin->tabs[textwin->tabs_num].type = type;
	textwin->tabs[textwin->tabs_num].name = strdup(tab_name);
	textwin->tabs[textwin->tabs_num].entries = NULL;
	textwin->tabs[textwin->tabs_num].entries_size = 0;
	button_create(&textwin->tabs[textwin->tabs_num].button);
	w = string_get_width(textwin->tabs[textwin->tabs_num].button.font, textwin->tabs[textwin->tabs_num].name, 0) + 10;
	snprintf(buf, sizeof(buf), "rectangle:%d,%d,255;<border=#909090 %d %d>", w, TEXTWIN_TAB_HEIGHT, w, TEXTWIN_TAB_HEIGHT);
	textwin->tabs[textwin->tabs_num].button.texture = texture_get(TEXTURE_TYPE_SOFTWARE, buf);
	textwin->tabs[textwin->tabs_num].button.texture_over = textwin->tabs[textwin->tabs_num].button.texture_pressed = NULL;
	textwin->tabs_num++;

	qsort((void *) textwin->tabs, textwin->tabs_num, sizeof(*textwin->tabs), (void *) (int (*)()) textwin_tab_compare);

	textwin_create_scrollbar(widget);
}

static void textwin_tab_append(widgetdata *widget, size_t id, const char *tab_name, const char *name, const char *color, const char *str)
{
	textwin_struct *textwin;
	SDL_Rect box;
	size_t len, scroll;
	char *cp;

	textwin = TEXTWIN(widget);
	box.w = TEXTWIN_TEXT_WIDTH(widget);
	box.h = 0;

	cp = string_join("", "\r", color, str, "\n", NULL);
	len = strlen(cp);
	/* Resize the characters array as needed. */
	textwin->tabs[id].entries = realloc(textwin->tabs[id].entries, textwin->tabs[id].entries_size + len + 1);
	memcpy(textwin->tabs[id].entries + textwin->tabs[id].entries_size, cp, len);
	textwin->tabs[id].entries[textwin->tabs[id].entries_size + len] = '\0';
	textwin->tabs[id].entries_size += len;
	free(cp);

	//logger_print(LOG(INFO), "%s", textwin->tabs[id].entries);

	box.y = 0;
	/* Get the string's height. */
	string_show(NULL, textwin->font, str, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
	scroll = box.h;

	/* Adjust the counts. */
	textwin->tabs[id].num_lines += scroll;

	/* Have the entries gone over maximum allowed lines? */
	if (textwin->tabs[id].entries && textwin->tabs[id].num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES))
	{
		while (textwin->tabs[id].num_lines >= (size_t) setting_get_int(OPT_CAT_GENERAL, OPT_MAX_CHAT_LINES) && (cp = strchr(textwin->tabs[id].entries, '\n')))
		{
			size_t pos = cp - textwin->tabs[id].entries + 1;
			char *buf = malloc(pos + 1);

			/* Copy the string together with the newline to a temporary
			 * buffer. */
			memcpy(buf, textwin->tabs[id].entries, pos);
			buf[pos] = '\0';

			/* Get the string's height. */
			box.h = 0;
			string_show(NULL, textwin->font, buf, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
			scroll = box.h - 1;

			free(buf);

			/* Move the string after the found newline to the beginning,
			 * effectively erasing the previous line. */
			textwin->tabs[id].entries_size -= pos;
			memmove(textwin->tabs[id].entries, textwin->tabs[id].entries + pos, textwin->tabs[id].entries_size);
			textwin->tabs[id].entries[textwin->tabs[id].entries_size] = '\0';

			/* Adjust the counts. */
			textwin->tabs[id].num_lines -= scroll;
		}
	}
}

void textwin_tabs_allowed_remove(textwin_struct *textwin, const char *tab_name)
{
	size_t i;

	for (i = 0; i < textwin->tabs_allowed_num; i++)
	{
		if (strcmp(textwin->tabs_allowed[i], tab_name) == 0)
		{
			free(textwin->tabs_allowed[i]);

			for (i = i + 1; i < textwin->tabs_allowed_num; i++)
			{
				textwin->tabs_allowed[i - 1] = textwin->tabs_allowed[i];
			}

			textwin->tabs_allowed = realloc(textwin->tabs_allowed, sizeof(*textwin->tabs_allowed) * (textwin->tabs_allowed_num - 1));
			textwin->tabs_allowed_num--;
			break;
		}
	}
}

void textwin_tabs_allowed_add(textwin_struct *textwin, const char *tab_name)
{
	if (strcmp(tab_name, "*") == 0)
	{
		size_t i;

		for (i = 0; i < arraysize(textwin_tab_names); i++)
		{
			textwin_tabs_allowed_add(textwin, textwin_tab_names[i]);
		}

		return;
	}

	textwin->tabs_allowed = realloc(textwin->tabs_allowed, sizeof(*textwin->tabs_allowed) * (textwin->tabs_allowed_num + 1));
	textwin->tabs_allowed[textwin->tabs_allowed_num] = strdup(tab_name);
	textwin->tabs_allowed_num++;
}

void draw_info_tab(size_t type, const char *name, const char *color, const char *str)
{
	const char *tab_name;
	widgetdata *widget;
	textwin_struct *textwin;
	uint32 bottom;
	size_t i;

	tab_name = textwin_tab_names[type];

	for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next)
	{
		textwin = TEXTWIN(widget);

		if (!textwin_tab_is_allowed(textwin, tab_name) && !textwin_tab_is_allowed(textwin, name))
		{
			continue;
		}

		WIDGET_REDRAW(widget);
		textwin_tab_ensure(widget, textwin, type, tab_name);
		bottom = SCROLL_BOTTOM(&textwin->scrollbar);

		if (textwin->tabs_allowed_num != 1)
		{
			textwin_tab_ensure(widget, textwin, CHAT_TYPE_ALL, textwin_tab_names[CHAT_TYPE_ALL]);
		}

		for (i = 0; i < textwin->tabs_num; i++)
		{
			if (textwin->tabs[i].type == CHAT_TYPE_ALL || textwin->tabs[i].type == type || strcmp(textwin->tabs[i].name, tab_name) == 0)
			{
				textwin_tab_append(widget, i, tab_name, name, color, str);
			}
		}

		if (textwin->tabs[textwin->tab_selected].scroll_offset == bottom)
		{
			scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
		}
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

	draw_info_tab(CHAT_TYPE_CHAT, "Xxx", color, buf);
}

/**
 * Add string to the text window.
 * @param flags Various flags, like color.
 * @param str The string. */
void draw_info(const char *color, const char *str)
{
	draw_info_tab(CHAT_TYPE_CHAT, "Xxx", color, str);
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
	memcpy(str, textwin->tabs[textwin->tab_selected].entries + start, end - start + 1);
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

void widget_textwin_deinit(widgetdata *widget)
{
}

void widget_textwin_init(widgetdata *widget)
{
	textwin_struct *textwin;

	textwin = calloc(1, sizeof(*textwin));

	if (!textwin)
	{
		logger_print(LOG(ERROR), "OOM.");
		exit(1);
	}

	textwin->font = FONT_ARIAL11;
	textwin->selection_start = -1;
	textwin->selection_end = -1;
	widget->subwidget = textwin;
	textwin_create_scrollbar(widget);
}

void widget_textwin_load(widgetdata *widget, const char *keyword, const char *parameter)
{
	textwin_struct *textwin;

	textwin = TEXTWIN(widget);

	if (strcmp(keyword, "font") == 0)
	{
		char font_name[MAX_BUF];
		int font_size, font_id;

		if (sscanf(parameter, "%s %d", font_name, &font_size) == 2 && (font_id = get_font_id(font_name, font_size)) != -1)
		{
			textwin->font = font_id;
		}
	}
	else if (strcmp(keyword, "tabs_allowed") == 0)
	{
		size_t pos;
		char word[MAX_BUF];

		pos = 0;

		while (string_get_word(parameter, &pos, ':', word, sizeof(word)))
		{
			if (string_startswith(word, "/"))
			{
				textwin_tabs_allowed_remove(textwin, word + 1);
			}
			else
			{
				textwin_tabs_allowed_add(textwin, word);
			}
		}
	}
}

void widget_textwin_save(widgetdata *widget, FILE *fp, const char *padding)
{
	textwin_struct *textwin;
	char *tabs_allowed;

	textwin = TEXTWIN(widget);

	fprintf(fp, "%sfont = %s %"FMT64U"\n", padding, get_font_filename(textwin->font), (uint64) fonts[textwin->font].size);
	tabs_allowed = string_join_array(":", textwin->tabs_allowed, textwin->tabs_allowed_num);
	fprintf(fp, "%stabs_allowed = %s\n", padding, tabs_allowed);
	free(tabs_allowed);
}

/**
 * Display the message text window, without handling scrollbar/mouse
 * actions.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param w Maximum width.
 * @param h Maximum height. */
void textwin_show(SDL_Surface *surface, int x, int y, int w, int h)
{
	widgetdata *widget;
	textwin_struct *textwin;
	SDL_Rect box;
	int scroll;

	for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next)
	{
		textwin = TEXTWIN(widget);

		if (textwin_tab_is_allowed(textwin, textwin_tab_names[CHAT_TYPE_GAME]))
		{
			break;
		}
	}

	if (!widget)
	{
		return;
	}

	box.w = w - 3;
	box.h = 0;
	box.x = 0;
	box.y = 0;
	string_show(NULL, textwin->font, textwin->tabs[0].entries, 3, 0, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
	scroll = box.h;

	box.x = x;
	box.y = y;
	box.w = w;
	box.h = h;
	SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0, 0, 0));
	draw_frame(surface, x, y, box.w, box.h);

	box.w = w - 3;
	box.h = h;

	box.y = MAX(0, scroll - (h / FONT_HEIGHT(textwin->font)));

	string_show(surface, textwin->font, textwin->tabs[0].entries, x + 3, y + 1, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box);
}

/**
 * Initialize text window scrollbar.
 * @param widget The text window. */
void textwin_create_scrollbar(widgetdata *widget)
{
	textwin_struct *textwin = TEXTWIN(widget);

	scrollbar_create(&textwin->scrollbar, 9, TEXTWIN_TEXT_HEIGHT(widget), &textwin->tabs[textwin->tab_selected].scroll_offset, &textwin->tabs[textwin->tab_selected].num_lines, TEXTWIN_ROWS_VISIBLE(widget));
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
	}

	if ((alpha = setting_get_int(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_TRANSPARENCY)))
	{
		filledRectAlpha(ScreenSurface, widget->x1, widget->y1, widget->x1 + widget->wd - 1, widget->y1 + widget->ht - 1, alpha);
	}

	/* Let's draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		SDL_FillRect(widget->widgetSF, NULL, 0);

		if (textwin->tabs)
		{
			size_t i;
			int button_x;

			button_x = 0;

			for (i = 0; i < textwin->tabs_num; i++)
			{
				if (i == textwin->tab_selected)
				{
					textwin->tabs[i].button.pressed_forced = 1;
				}

				textwin->tabs[i].button.x = button_x;
				textwin->tabs[i].button.y = 0;
				textwin->tabs[i].button.surface = widget->widgetSF;
				button_set_parent(&textwin->tabs[i].button, widget->x1, widget->y1);
				button_show(&textwin->tabs[i].button, textwin->tabs[i].name);
				textwin->tabs[i].button.pressed_forced = 0;

				button_x += TEXTURE_SURFACE(textwin->tabs[i].button.texture)->w - 1;
			}

			/* Show the text entries, if any. */
			if (textwin->tabs[textwin->tab_selected].entries)
			{
				SDL_Rect box_text;

				box_text.w = TEXTWIN_TEXT_WIDTH(widget);
				box_text.h = TEXTWIN_TEXT_HEIGHT(widget);
				box_text.y = textwin->tabs[textwin->tab_selected].scroll_offset;
				text_set_selection(&textwin->selection_start, &textwin->selection_end, &textwin->selection_started);
				string_show(widget->widgetSF, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), TEXTWIN_TEXT_STARTY(widget) + TEXTWIN_TAB_HEIGHT, COLOR_WHITE, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box_text);
				text_set_selection(NULL, NULL, NULL);
			}

			textwin->scrollbar.max_lines = TEXTWIN_ROWS_VISIBLE(widget);
			textwin->scrollbar.px = widget->x1;
			textwin->scrollbar.py = widget->y1;
			scrollbar_show(&textwin->scrollbar, widget->widgetSF, widget->wd - 1 - textwin->scrollbar.background.w, TEXTWIN_TEXT_STARTY(widget) + TEXTWIN_TAB_HEIGHT);

			widget->redraw = scrollbar_need_redraw(&textwin->scrollbar);
		}
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
	size_t i;

	for (i = 0; i < textwin->tabs_num; i++)
	{
		if (button_event(&textwin->tabs[i].button, event))
		{
			textwin->tab_selected = i;
			WIDGET_REDRAW(widget);
			return;
		}
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
	free(textwin->tabs[textwin->tab_selected].entries);
	textwin->tabs[textwin->tab_selected].entries = NULL;
	textwin->tabs[textwin->tab_selected].num_lines = textwin->tabs[textwin->tab_selected].entries_size = textwin->tabs[textwin->tab_selected].scroll_offset = 0;
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

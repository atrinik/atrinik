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
 * Implements text window type widgets.
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

	if (!textwin->tabs)
	{
		return;
	}

	if (textwin->tabs[textwin->tab_selected].entries)
	{
		SDL_Rect box;

		box.w = TEXTWIN_TEXT_WIDTH(widget);
		box.h = 0;
		box.x = 0;
		box.y = 0;
		string_show(NULL, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);

		/* Adjust the counts. */
		textwin->tabs[textwin->tab_selected].num_lines = box.h - 1;
	}

	textwin_create_scrollbar(widget);
	scrollbar_scroll_to(&textwin->scrollbar, SCROLL_BOTTOM(&textwin->scrollbar));
	WIDGET_REDRAW(widget);
}

static void textwin_tab_append(widgetdata *widget, uint8 id, uint8 type, const char *name, const char *color, const char *str)
{
	textwin_struct *textwin;
	SDL_Rect box;
	size_t len, scroll;
	char timebuf[MAX_BUF], tabname[MAX_BUF], plname[MAX_BUF], *cp;

	textwin = TEXTWIN(widget);
	box.w = TEXTWIN_TEXT_WIDTH(widget);
	box.h = 0;

	timebuf[0] = tabname[0] = plname[0] = '\0';

	if (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS) && textwin->timestamps)
	{
		time_t now = time(NULL);
		char tmptimebuf[MAX_BUF], *format;
		struct tm *tm = localtime(&now);
		size_t timelen;

		switch (setting_get_int(OPT_CAT_GENERAL, OPT_CHAT_TIMESTAMPS))
		{
			/* HH:MM */
			case 1:
			default:
				format = "%H:%M";
				break;

			/* HH:MM:SS */
			case 2:
				format = "%H:%M:%S";
				break;

			/* H:MM AM/PM */
			case 3:
				format = "%I:%M %p";
				break;

			/* H:MM:SS AM/PM */
			case 4:
				format = "%I:%M:%S %p";
				break;
		}

		timelen = strftime(tmptimebuf, sizeof(tmptimebuf), format, tm);

		if (timelen != 0)
		{
			snprintf(timebuf, sizeof(timebuf), "[%s] ", tmptimebuf);
		}
	}

	if (textwin->tabs[id].type == CHAT_TYPE_ALL)
	{
		snprintf(tabname, sizeof(tabname), "%s ", textwin_tab_names[type - 1]);
	}

	if (!string_isempty(name))
	{
		snprintf(plname, sizeof(plname), "%s: ", name);
	}

	cp = string_join("", "<c=#", color, ", 1>", timebuf, tabname, plname, str, "\n", NULL);
	len = strlen(cp);
	/* Resize the characters array as needed. */
	textwin->tabs[id].entries = realloc(textwin->tabs[id].entries, textwin->tabs[id].entries_size + len + 1);
	memcpy(textwin->tabs[id].entries + textwin->tabs[id].entries_size, cp, len);
	textwin->tabs[id].entries[textwin->tabs[id].entries_size + len] = '\0';
	textwin->tabs[id].entries_size += len;
	free(cp);

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

static int textwin_tab_compare(const void *a, const void *b)
{
	textwin_tab_struct *tab_one, *tab_two;

	tab_one = (textwin_tab_struct *) a;
	tab_two = (textwin_tab_struct *) b;

	if (tab_one->name && !tab_two->name)
	{
		return 1;
	}
	else if (!tab_one->name && tab_two->name)
	{
		return -1;
	}
	else if (tab_one->name && tab_two->name)
	{
		return strcmp(tab_one->name, tab_two->name);
	}

	return tab_one->type - tab_two->type;
}

size_t textwin_tab_name_to_id(const char *name)
{
	size_t i;

	for (i = 0; i < arraysize(textwin_tab_names); i++)
	{
		if (strcmp(textwin_tab_names[i], name) == 0)
		{
			return i + 1;

		}
	}

	return CHAT_TYPE_PRIVATE;
}

void textwin_tab_free(textwin_tab_struct *tab)
{
}

void textwin_tab_remove(widgetdata *widget, const char *name)
{
	textwin_struct *textwin;
	size_t i;

	textwin = TEXTWIN(widget);

	for (i = 0; i < textwin->tabs_num; i++)
	{
		if (strcmp(TEXTWIN_TAB_NAME(&textwin->tabs[i]), name) != 0)
		{
			continue;
		}

		textwin_tab_free(&textwin->tabs[i]);

		for (i = i + 1; i < textwin->tabs_num; i++)
		{
			textwin->tabs[i - 1] = textwin->tabs[i];
		}

		textwin->tabs = realloc(textwin->tabs, sizeof(*textwin->tabs) * (textwin->tabs_num - 1));
		textwin->tabs_num--;
		textwin_readjust(widget);
		break;
	}
}

void textwin_tab_add(widgetdata *widget, const char *name)
{
	textwin_struct *textwin;
	char buf[MAX_BUF];

	textwin = TEXTWIN(widget);
	textwin->tabs = memory_reallocz(textwin->tabs, sizeof(*textwin->tabs) * textwin->tabs_num, sizeof(*textwin->tabs) * (textwin->tabs_num + 1));

	textwin->tabs[textwin->tabs_num].type = textwin_tab_name_to_id(name);

	if (!string_startswith(name, "[") && !string_endswith(name, "]"))
	{
		textwin->tabs[textwin->tabs_num].name = strdup(name);
	}

	button_create(&textwin->tabs[textwin->tabs_num].button);
	snprintf(buf, sizeof(buf), "rectangle:%1$d,20,255;<border=#606060 %1$d 20>", string_get_width(textwin->tabs[textwin->tabs_num].button.font, TEXTWIN_TAB_NAME(&textwin->tabs[textwin->tabs_num]), 0) + 10);
	textwin->tabs[textwin->tabs_num].button.texture = texture_get(TEXTURE_TYPE_SOFTWARE, buf);
	textwin->tabs[textwin->tabs_num].button.texture_over = textwin->tabs[textwin->tabs_num].button.texture_pressed = NULL;
	textwin->tabs_num++;

	qsort((void *) textwin->tabs, textwin->tabs_num, sizeof(*textwin->tabs), (void *) (int (*)()) textwin_tab_compare);
	textwin_readjust(widget);
}

int textwin_tab_find(widgetdata *widget, uint8 type, const char *name, size_t *id)
{
	textwin_struct *textwin;

	textwin = TEXTWIN(widget);

	for (*id = 0; *id < textwin->tabs_num; (*id)++)
	{
		if (textwin->tabs[*id].type == type || (textwin->tabs[*id].type == CHAT_TYPE_PRIVATE && textwin->tabs[*id].name && !string_isempty(name) && strcmp(textwin->tabs[*id].name, name) == 0))
		{
			return 1;
		}
	}

	return 0;
}

void draw_info_tab(size_t type, const char *name, const char *color, const char *str)
{
	widgetdata *widget;
	textwin_struct *textwin;
	uint32 bottom;
	size_t i;

	for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next)
	{
		textwin = TEXTWIN(widget);

		if (!textwin->tabs)
		{
			continue;
		}

		WIDGET_REDRAW(widget);
		bottom = SCROLL_BOTTOM(&textwin->scrollbar);

		if (textwin_tab_find(widget, type, name, &i))
		{
			textwin_tab_append(widget, i, type, name, color, str);

			if (textwin_tab_find(widget, CHAT_TYPE_ALL, NULL, &i))
			{
				textwin_tab_append(widget, i, type, name, color, str);
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

	draw_info_tab(CHAT_TYPE_GAME, NULL, color, buf);
}

/**
 * Add string to the text window.
 * @param flags Various flags, like color.
 * @param str The string. */
void draw_info(const char *color, const char *str)
{
	draw_info_tab(CHAT_TYPE_GAME, NULL, color, str);
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
		widget = widget_find_by_type(CHATWIN_ID);

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
	size_t i;
	SDL_Rect box;
	int scroll;

	for (widget = cur_widget[CHATWIN_ID]; widget; widget = widget->type_next)
	{
		textwin = TEXTWIN(widget);

		for (i = 0; i < textwin->tabs_num; i++)
		{
			if (textwin->tabs[i].type == CHAT_TYPE_GAME)
			{
				box.w = w - 3;
				box.h = 0;
				box.x = 0;
				box.y = 0;
				string_show(NULL, textwin->font, textwin->tabs[i].entries, 3, 0, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_CALC, &box);
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

				string_show(surface, textwin->font, textwin->tabs[i].entries, x + 3, y + 1, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box);
				break;
			}
		}
	}
}

int textwin_tabs_height(widgetdata *widget)
{
	textwin_struct *textwin;
	int button_x, button_y;
	size_t i;

	textwin = TEXTWIN(widget);
	button_x = button_y = 0;

	if (textwin->tabs_num <= 1)
	{
		return 0;
	}

	for (i = 0; i < textwin->tabs_num; i++)
	{
		if (button_x + TEXTURE_SURFACE(textwin->tabs[i].button.texture)->w > widget->w)
		{
			button_x = 0;
			button_y += TEXTWIN_TAB_HEIGHT - 1;
		}

		button_x += TEXTURE_SURFACE(textwin->tabs[i].button.texture)->w - 1;
	}

	return button_y + TEXTWIN_TAB_HEIGHT - 1;
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

/** @copydoc widgetdata::draw_func */
static void widget_draw(widgetdata *widget)
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
	if (!widget->surface || widget->w != widget->surface->w || widget->h != widget->surface->h)
	{
		if (widget->surface)
		{
			SDL_FreeSurface(widget->surface);
		}

		widget->surface = SDL_CreateRGBSurface(get_video_flags(), widget->w, widget->h, video_get_bpp(), 0, 0, 0, 0);
		SDL_SetColorKey(widget->surface, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
		textwin_readjust(widget);
	}

	if ((alpha = setting_get_int(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_TRANSPARENCY)))
	{
		SDL_Color bg_color;

		if (text_color_parse(setting_get_str(OPT_CAT_CLIENT, OPT_TEXT_WINDOW_BG_COLOR), &bg_color))
		{
			filledRectAlpha(ScreenSurface, widget->x, widget->y, widget->x + widget->w - 1, widget->y + widget->h - 1, ((uint32) bg_color.r << 24) | ((uint32) bg_color.g << 16) | ((uint32) bg_color.b << 8) | (uint32) alpha);
		}
	}

	/* Let's draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		SDL_FillRect(widget->surface, NULL, 0);

		if (textwin->tabs)
		{
			int yadjust;

			yadjust = 0;

			if (textwin->tabs_num > 1)
			{
				size_t i;
				int button_x, button_y;

				button_x = button_y = 0;

				for (i = 0; i < textwin->tabs_num; i++)
				{
					if (button_x + TEXTURE_SURFACE(textwin->tabs[i].button.texture)->w > widget->w)
					{
						button_x = 0;
						button_y += TEXTWIN_TAB_HEIGHT - 1;
					}

					textwin->tabs[i].button.x = button_x;
					textwin->tabs[i].button.y = button_y;
					textwin->tabs[i].button.surface = widget->surface;
					button_set_parent(&textwin->tabs[i].button, widget->x, widget->y);
					button_show(&textwin->tabs[i].button, TEXTWIN_TAB_NAME(&textwin->tabs[i]));

					button_x += TEXTURE_SURFACE(textwin->tabs[i].button.texture)->w - 1;
				}

				yadjust = button_y + TEXTWIN_TAB_HEIGHT;
				BORDER_CREATE_BOTTOM(widget->surface, 0, 0, widget->w, yadjust, textwin_border_color, 1);
				yadjust -= 1;
			}

			/* Show the text entries, if any. */
			if (textwin->tabs[textwin->tab_selected].entries)
			{
				SDL_Rect box_text;

				box_text.w = TEXTWIN_TEXT_WIDTH(widget);
				box_text.h = TEXTWIN_TEXT_HEIGHT(widget);
				box_text.y = textwin->tabs[textwin->tab_selected].scroll_offset;
				text_set_selection(&textwin->selection_start, &textwin->selection_end, &textwin->selection_started);
				string_show(widget->surface, textwin->font, textwin->tabs[textwin->tab_selected].entries, TEXTWIN_TEXT_STARTX(widget), TEXTWIN_TEXT_STARTY(widget) + yadjust, COLOR_BLACK, TEXTWIN_TEXT_FLAGS(widget) | TEXT_LINES_SKIP, &box_text);
				text_set_selection(NULL, NULL, NULL);
			}

			textwin->scrollbar.max_lines = TEXTWIN_ROWS_VISIBLE(widget);
			textwin->scrollbar.px = widget->x;
			textwin->scrollbar.py = widget->y;
			scrollbar_show(&textwin->scrollbar, widget->surface, widget->w - 1 - textwin->scrollbar.background.w, TEXTWIN_TEXT_STARTY(widget) + yadjust);

			widget->redraw = scrollbar_need_redraw(&textwin->scrollbar);
		}
	}

	box.x = widget->x;
	box.y = widget->y;
	SDL_BlitSurface(widget->surface, NULL, ScreenSurface, &box);

	SDL_GetMouseState(&mx, &my);

	if (mx < widget->x || mx > widget->x + widget->w || my < widget->y || my > widget->y + widget->h)
	{
		widget->resize_flags = 0;
	}

	BORDER_CREATE_TOP(ScreenSurface, widget->x, widget->y, widget->w, widget->h, widget->resize_flags & RESIZE_TOP ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_BOTTOM(ScreenSurface, widget->x, widget->y, widget->w, widget->h, widget->resize_flags & RESIZE_BOTTOM ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_LEFT(ScreenSurface, widget->x, widget->y, widget->w, widget->h, widget->resize_flags & RESIZE_LEFT ? textwin_border_color_selected : textwin_border_color, 1);
	BORDER_CREATE_RIGHT(ScreenSurface, widget->x, widget->y, widget->w, widget->h, widget->resize_flags & RESIZE_RIGHT ? textwin_border_color_selected : textwin_border_color, 1);
}

/** @copydoc widgetdata::event_func */
static int widget_event(widgetdata *widget, SDL_Event *event)
{
	textwin_struct *textwin = TEXTWIN(widget);

	if (textwin->tabs_num > 1 && (event->type == SDL_MOUSEMOTION || event->type == SDL_MOUSEBUTTONDOWN))
	{
		size_t i;

		for (i = 0; i < textwin->tabs_num; i++)
		{
			if (button_event(&textwin->tabs[i].button, event))
			{
				textwin->tab_selected = i;
				WIDGET_REDRAW(widget);
				return 1;
			}
			else if (BUTTON_MOUSE_OVER(&textwin->tabs[i].button, event->motion.x, event->motion.y, TEXTURE_SURFACE(textwin->tabs[i].button.texture)))
			{
				WIDGET_REDRAW(widget);
			}
		}
	}

	if (scrollbar_event(&textwin->scrollbar, event))
	{
		WIDGET_REDRAW(widget);
		return 1;
	}

	if (event->button.button == SDL_BUTTON_LEFT)
	{
		if (event->type == SDL_MOUSEBUTTONUP)
		{
			textwin->selection_started = 0;
			return 1;
		}
		else if (event->type == SDL_MOUSEBUTTONDOWN)
		{
			textwin->selection_started = 0;
			textwin->selection_start = -1;
			textwin->selection_end = -1;
			WIDGET_REDRAW(widget);
			return 1;
		}
		else if (event->type == SDL_MOUSEMOTION)
		{
			WIDGET_REDRAW(widget);
			textwin->selection_started = 1;
			return 1;
		}
	}

	if (event->type == SDL_MOUSEBUTTONDOWN)
	{
		if (event->button.button == SDL_BUTTON_WHEELUP)
		{
			scrollbar_scroll_adjust(&textwin->scrollbar, -1);
			return 1;
		}
		else if (event->button.button == SDL_BUTTON_WHEELDOWN)
		{
			scrollbar_scroll_adjust(&textwin->scrollbar, 1);
			return 1;
		}
	}

	return 0;
}

/** @copydoc widgetdata::deinit_func */
static void widget_deinit(widgetdata *widget)
{
}

/** @copydoc widgetdata::load_func */
static int widget_load(widgetdata *widget, const char *keyword, const char *parameter)
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
			return 1;
		}
	}
	else if (strcmp(keyword, "tabs") == 0)
	{
		size_t pos;
		char word[MAX_BUF];

		pos = 0;

		while (string_get_word(parameter, &pos, ':', word, sizeof(word)))
		{
			if (string_startswith(word, "/"))
			{
				textwin_tab_remove(widget, word + 1);
			}
			else
			{
				if (strcmp(word, "*") == 0)
				{
					size_t i;

					for (i = 0; i < arraysize(textwin_tab_names); i++)
					{
						textwin_tab_add(widget, textwin_tab_names[i]);
					}
				}
				else
				{
					textwin_tab_add(widget, word);
				}
			}
		}

		return 1;
	}
	else if (strcmp(keyword, "timestamps") == 0)
	{
		KEYWORD_TO_BOOLEAN(parameter, textwin->timestamps);
		return 1;
	}

	return 0;
}

/** @copydoc widgetdata::save_func */
static void widget_save(widgetdata *widget, FILE *fp, const char *padding)
{
	textwin_struct *textwin;

	textwin = TEXTWIN(widget);

	fprintf(fp, "%sfont = %s %"FMT64U"\n", padding, get_font_filename(textwin->font), (uint64) fonts[textwin->font].size);
	fprintf(fp, "%stimestamps = %s\n", padding, textwin->timestamps ? "yes" : "no");

	if (textwin->tabs_num)
	{
		size_t i;

		fprintf(fp, "%stabs = ", padding);

		for (i = 0; i < textwin->tabs_num; i++)
		{
			fprintf(fp, "%s%s", i == 0 ? "" : ":", TEXTWIN_TAB_NAME(&textwin->tabs[i]));
		}

		fprintf(fp, "\n");
	}
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

	widget->draw_func = widget_draw;
	widget->event_func = widget_event;
	widget->save_func = widget_save;
	widget->load_func = widget_load;
	widget->deinit_func = widget_deinit;
	widget->subwidget = textwin;

	textwin_create_scrollbar(widget);
}

/**
 * The 'Clear' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_clear(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	textwin_struct *textwin;

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
void menu_textwin_copy(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
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
void menu_textwin_font_inc(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	textwin_font_adjust(widget, 1);
}

/**
 * The 'Decrease Font Size' menu action for text windows.
 * @param widget The text window widget.
 * @param x X.
 * @param y Y. */
void menu_textwin_font_dec(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	textwin_font_adjust(widget, -1);
}

void menu_textwin_timestamps(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	textwin_struct *textwin;

	textwin = TEXTWIN(widget);
	textwin->timestamps = !textwin->timestamps;
}

void menu_textwin_tab(widgetdata *widget, widgetdata *menuitem, SDL_Event *event)
{
	widgetdata *tmp;
	_widget_label *label;
	size_t id;

	for (tmp = menuitem->inv; tmp; tmp = tmp->next)
	{
		if (tmp->type == LABEL_ID)
		{
			label = LABEL(menuitem->inv);

			if (textwin_tab_find(widget, textwin_tab_name_to_id(label->text), label->text, &id))
			{
				textwin_tab_remove(widget, label->text);
			}
			else
			{
				textwin_tab_add(widget, label->text);
			}

			WIDGET_REDRAW(widget);
			break;
		}
	}
}

void textwin_submenu_tabs(widgetdata *widget, widgetdata *menu)
{
	widgetdata *tmp, *menuitem;
	textwin_struct *textwin;
	size_t i, id;
	uint8 found;
	_widget_label *label;

	for (i = 0; i < arraysize(textwin_tab_names); i++)
	{
		add_menuitem(menu, textwin_tab_names[i], &menu_textwin_tab, MENU_CHECKBOX, textwin_tab_find(widget, i + 1, NULL, &id));
	}

	for (tmp = cur_widget[CHATWIN_ID]; tmp; tmp = tmp->type_next)
	{
		textwin = TEXTWIN(tmp);

		for (i = 0; i < textwin->tabs_num; i++)
		{
			if (!textwin->tabs[i].name)
			{
				continue;
			}

			found = 0;

			for (menuitem = menu->inv; menuitem; menuitem = menuitem->next)
			{
				if (menuitem->inv->type == LABEL_ID)
				{
					label = LABEL(menuitem->inv);

					if (strcmp(label->text, textwin->tabs[i].name) == 0)
					{
						found = 1;
						break;
					}
				}
			}

			if (!found)
			{
				add_menuitem(menu, textwin->tabs[i].name, &menu_textwin_tab, MENU_CHECKBOX, tmp == widget);
			}
		}
	}
}

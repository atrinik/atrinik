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

static struct _Font *textwin_font = &SystemFont;

/* Definition of keyword:
 *  A keyword is the text between two '^' characters.
 *  The size can vary between a single word and a complete sentence.
 *  The max length of a keyword is 1 row (inclusive '^') because only
 *  one LF within is allowed. */

/**
 * Figure out the start position of a keyword.
 * @param actWin The text window.
 * @param mouseX Mouse X.
 * @param row Row to check.
 * @param wID Widget ID.
 * @return NULL if no keyword, otherwise the keyword is returned. */
static char *get_keyword_start(widgetdata *widget, int mouseX, int *row)
{
	_textwin *textwin = TEXTWIN(widget);
	int pos, pos2 = widget->x1, key_start;
	char *text;

	if (!textwin)
	{
		return 0;
	}

	pos = (textwin->top_drawLine + (*row)) % TEXT_WIN_MAX - textwin->scroll;

	if (pos < 0)
	{
		pos += TEXT_WIN_MAX;
	}

	if (mouseX > 0)
	{
		/* Check if keyword starts in the row before */
		if (textwin->scroll + 1 != textwin->act_bufsize - textwin->size && textwin->text[pos].key_clipped)
		{
			/* Was a clipped keyword clicked? */
			int index = -1;
			text = textwin->text[pos].buf;

			while (text[++index] && pos2 <= mouseX && text[index] != '^')
			{
				pos2 += textwin_font->c[(int) text[index]].w + textwin_font->char_offset;
			}

			/* Clipped keyword was clicked, so we must start one row before */
			if (text[index] != '^')
			{
				(*row)--;
				pos = (textwin->top_drawLine + (*row)) % TEXT_WIN_MAX - textwin->scroll;

				if (pos < 0)
				{
					pos += TEXT_WIN_MAX;
				}

				mouseX = Screensize->x;
			}
		}
	}

	text = textwin->text[pos].buf;

	/* Find the first character of the keyword */
	if (textwin->text[pos].key_clipped)
	{
		key_start = 0;
	}
	else
	{
		key_start = -1;
	}

	pos = 0;
	pos2 = widget->x1;

	while (text[pos] && pos2 <= mouseX)
	{
		if (text[pos++] == '^')
		{
			/* Start of a keyword */
			if (key_start < 0)
			{
				key_start = pos;
			}
			/* End of a keyword */
			else
			{
				key_start = -1;
			}

			continue;
		}

		pos2 += textwin_font->c[(int) text[pos]].w + textwin_font->char_offset;
	}

	/* No keyword here */
	if (key_start < 0)
	{
		return NULL;
	}

	(*row)++;

	return &text[key_start];
}

/**
 * Check for keyword and send it to server.
 * @param actWin The text window.
 * @param mouseX Mouse X.
 * @param mouseY Mouse Y. */
void say_clickedKeyword(widgetdata *widget, int mouseX, int mouseY)
{
	_textwin *textwin = TEXTWIN(widget);
	char cmdBuf[MAX_KEYWORD_LEN + 1] = {"/say "};
	char cmdBuf2[MAX_KEYWORD_LEN + 1] = {""};
	char *text;
	int clicked_row, pos = 5;

	/* sanity check */
	if (!textwin)
		return;

	clicked_row = (mouseY - widget->y1 - 2.5f) / 10;

	text = get_keyword_start(widget, mouseX, &clicked_row);

	if (text == NULL)
	{
		return;
	}

	while (*text && *text != '^')
	{
		cmdBuf[pos++] = *text++;
	}

	if (*text != '^')
	{
		text = get_keyword_start(widget, -1, &clicked_row);

		if (text != NULL)
		{
			while (*text && *text != '^')
			{
				cmdBuf[pos++] = *text++;
			}
		}
	}

	cmdBuf[pos++] = '\0';

	/* Clickable keywords can be commands too.
	 * Commands will get executed instead of said. */

	/* Get rid of /say */
	if (cmdBuf[5] == '/')
	{
		pos = 5;

		while (cmdBuf[pos] != '\0')
		{
			cmdBuf2[pos - 5] = cmdBuf[pos];
			pos++;
		}

		if (!client_command_check(cmdBuf2))
		{
			send_command(cmdBuf2, -1, SC_NORMAL);
		}
	}
	else
	{
		send_command(cmdBuf, -1, SC_NORMAL);
	}
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
 * Add string to the text window (perform auto clipping).
 * @param str The string.
 * @param flags Various flags, like color. */
void draw_info(char *str, int flags)
{
	static int key_start = 0;
	static int key_count = 0;
	int i, len,a, color, mode;
	int winlen = 239;
	char buf[4096];
	char *text;
	int z;
	widgetdata *widget;
	_textwin *textwin;

	color = flags & 0xff;
	mode = flags;

	/* Remove special characters. */
	for (i = 0; str[i] != 0; i++)
	{
		if (str[i] < ' ' && str[i] != '\n')
		{
			str[i] = ' ';
		}
	}

	len = 0;

	for (a = i = 0; ; i++)
	{
		widget = cur_widget[MIXWIN_ID];
		textwin = TEXTWIN(widget);

		if (str[i] != '^')
		{
			len += textwin_font->c[(int) (str[i])].w + textwin_font->char_offset;
		}

		if (len >= winlen || str[i] == '\n' || str[i] == '\0')
		{
			/* Now the special part - let's look for a good point to cut */
			if (len >= winlen && a > 10)
			{
				int ii = a, it = i, ix = a, tx = i;

				while (ii >= a / 2)
				{
					if (str[it] == ' ' || str[it] == ':' || str[it] == '.' || str[it] == ',' || str[it] == '(' || str[it] == ';' || str[it] == '-' || str[it] == '+' || str[it] == '*' || str[it] == '?' || str[it] == '/' || str[it] == '=' || str[it] == '.' || str[it] == '\0' || str[it] == '\n')
					{
						tx = it;
						ix = ii;
						break;
					}

					it--;
					ii--;
				}

				i = tx;
				a = ix;
			}

			buf[a] = 0;

			/* TODO: remind me to give this bit of code a nice mashing up later */
			for (z = 0; z < 2; z++)
			{
				/* Add messages to mixed textwin and either to msg OR chat textwin */
				strcpy(textwin->text[textwin->bot_drawLine % TEXT_WIN_MAX].buf, buf);
				textwin->text[textwin->bot_drawLine % TEXT_WIN_MAX].color = color;
				textwin->text[textwin->bot_drawLine % TEXT_WIN_MAX].flags = mode;
				textwin->text[textwin->bot_drawLine % TEXT_WIN_MAX].key_clipped = key_start;

				if (textwin->scroll)
				{
					textwin->scroll++;
				}

				if (textwin->act_bufsize < TEXT_WIN_MAX)
				{
					textwin->act_bufsize++;
				}

				textwin->bot_drawLine++;
				textwin->bot_drawLine %= TEXT_WIN_MAX;

				if (mode & NDI_PLAYER)
				{
					widget = cur_widget[CHATWIN_ID];
					/* Next window => MSG_CHAT */
                    /* just a little debug until we have proper text window filters in place */
					if (widget->type_next)
					{
						widget = widget->type_next;
					}
					textwin = TEXTWIN(widget);
					WIDGET_REDRAW(widget);
				}
				else
				{
					widget = cur_widget[MSGWIN_ID];
					/* Next window => MSG_WIN */
					textwin = TEXTWIN(widget);
					WIDGET_REDRAW(widget);
				}
			}

			/* Because of autoclip we must scan every line again */
			for (text = buf; *text; text++)
			{
				if (*text == '^')
				{
					key_count = (key_count + 1) & 1;
				}
			}

			if (key_count)
			{
				key_start = 0x1000;
			}
			else
			{
				key_start = 0;
			}

			a = len = 0;

			if (str[i] == 0)
			{
				break;
			}
		}

		if (str[i] != '\n')
		{
			buf[a++] = str[i];
		}
	}
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
	int i, temp;

	textwin->x = x;
	textwin->y = y;

    /* this probably won't be needed soon, since I plan to merge the different types of textwin widgets into one */
    if (widget->WidgetTypeID != MIXWIN_ID)
	{
		x = y = 0;
	}

	textwin->top_drawLine = textwin->bot_drawLine - (textwin->size + 1);

	if (textwin->top_drawLine < 0 && textwin->act_bufsize == TEXT_WIN_MAX)
	{
		textwin->top_drawLine += TEXT_WIN_MAX;
	}
	else if (textwin->top_drawLine < 0)
	{
		textwin->top_drawLine = 0;
	}

	if (textwin->scroll > textwin->act_bufsize - (textwin->size + 1))
	{
		textwin->scroll = textwin->act_bufsize - (textwin->size + 1);
	}

	if (textwin->scroll < 0)
	{
		textwin->scroll = 0;
	}

	for (i = 0; i <= textwin->size && i < textwin->act_bufsize; i++)
	{
		temp = (textwin->top_drawLine + i) % TEXT_WIN_MAX;

		if (textwin->act_bufsize > textwin->size)
		{
			temp -= textwin->scroll;

			if (temp < 0)
			{
				temp = TEXT_WIN_MAX + temp;
			}
		}

		if (textwin->text[temp].key_clipped)
		{
			StringBlt(bltfx->surface, textwin_font, &textwin->text[temp].buf[0], x + 2, (y + 1 + i * 10), textwin->text[temp].color | COLOR_FLAG_CLIPPED, NULL, NULL);
		}
		else
		{
			StringBlt(bltfx->surface, textwin_font, &textwin->text[temp].buf[0], x + 2, (y + 1 + i * 10), textwin->text[temp].color, NULL, NULL);
		}
	}

	/* Only draw scrollbar if needed */
	if (textwin->act_bufsize > textwin->size)
	{
		SDL_Rect box;

		box.x = box.y = 0;
		box.w = Bitmaps[BITMAP_SLIDER]->bitmap->w;
		box.h = textwin->size * 10 + 1;

		/* No textinput-line */
		temp = -9;
		sprite_blt(Bitmaps[BITMAP_SLIDER_UP], x + 250, y + 2, NULL, bltfx);
		sprite_blt(Bitmaps[BITMAP_SLIDER_DOWN], x + 250, y + 13 + temp + textwin->size * 10, NULL, bltfx);
		sprite_blt(Bitmaps[BITMAP_SLIDER], x + 250, y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2 + temp, &box, bltfx);
		box.h += temp - 2;
		box.w -= 2;

		textwin->slider_y = ((textwin->act_bufsize - (textwin->size + 1) - textwin->scroll) * box.h) / textwin->act_bufsize;
		/* between 0.0 <-> 1.0 */
		textwin->slider_h = (box.h * (textwin->size + 1)) / textwin->act_bufsize;

		if (textwin->slider_h < 1)
		{
			textwin->slider_h = 1;
		}

		if (!textwin->scroll && textwin->slider_y + textwin->slider_h < box.h)
		{
			textwin->slider_y++;
		}

		box.h = textwin->slider_h;
		sprite_blt(Bitmaps[BITMAP_TWIN_SCROLL], x + 252, y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + textwin->slider_y, &box, bltfx);

		if (textwin->highlight == TW_HL_UP)
		{
			box.x = x + 250;
			box.y = y + 2;
			box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
			box.w = 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			box.h = 1;
			box.x = x + 250;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.y += Bitmaps[BITMAP_SLIDER_UP]->bitmap->h - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
		}
		else if (textwin->highlight == TW_ABOVE)
		{
			box.x = x + 252;
			box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 2;
			box.h = textwin->slider_y + 1;
			box.w = 5;
			SDL_FillRect(bltfx->surface, &box, 0);
		}
		else if (textwin->highlight == TW_HL_SLIDER)
		{
			box.x = x + 252;
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
			box.x = x + 252;
			box.y = y + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h + 3 + textwin->slider_y + box.h;
			box.h = textwin->size * 10 - textwin->slider_y - textwin->slider_h - 10;
			box.w = 5;
			SDL_FillRect(bltfx->surface, &box, 0);
		}
		else if (textwin->highlight == TW_HL_DOWN)
		{
			box.x = x + 250;
			box.y = y + textwin->size * 10 + 4;
			box.h = Bitmaps[BITMAP_SLIDER_UP]->bitmap->h;
			box.w = 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.x += Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			SDL_FillRect(bltfx->surface, &box, -1);
			box.w = Bitmaps[BITMAP_SLIDER_UP]->bitmap->w - 1;
			box.h = 1;
			box.x = x + 250;
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
 * Display one text window with text from both.
 *
 * Used only at startup.
 * @param x X position of the text window.
 * @param y Y position of the text window. */
void textwin_show(int x, int y)
{
	int len;
	SDL_Rect box;
	_BLTFX bltfx;
	widgetdata *widget = cur_widget[MIXWIN_ID];
	_textwin *textwin = TEXTWIN(widget);

	/* sanity check */
	if (!textwin)
		return;

	bltfx.alpha = options.textwin_alpha;
	bltfx.flags = BLTFX_FLAG_SRCALPHA;
	bltfx.surface = NULL;
	box.x = box.y = 0;
	box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
	y = Screensize->y - (Screensize->y / 2 - Bitmaps[BITMAP_DIALOG_BG]->bitmap->h / 2) + 25;

	box.h = len = textwin->size * 10 + 23;
	y -= len;

	if (options.use_TextwinAlpha)
	{
		sprite_blt(Bitmaps[BITMAP_TEXTWIN_MASK], x, y, &box, &bltfx);
	}
	else
	{
		sprite_blt(Bitmaps[BITMAP_TEXTWIN], x, y, &box, NULL);
	}

	bltfx.alpha = 255;
	bltfx.surface = ScreenSurface;
	bltfx.flags = 0;

	/* show the first textwin widget, probably need to tweak this later when the textwins get merged */
	show_window(widget, x, y - 1, &bltfx);
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
	box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
	box.h = len = textwin->size * 10 + 13;

	/* If we don't have a backbuffer, create it */
	if (!widget->widgetSF)
	{
		/* Need to do this, or the foreground could be semi-transparent too. */
		SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA | SDL_RLEACCEL, 255);
		widget->widgetSF = SDL_ConvertSurface(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->format, Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap->flags);
		SDL_SetColorKey(widget->widgetSF, SDL_SRCCOLORKEY | SDL_RLEACCEL, SDL_MapRGB(widget->widgetSF->format, 0, 0, 0));
	}

	if (options.use_TextwinAlpha)
	{
		if (old_txtwin_alpha != options.textwin_alpha || old_alpha_option != options.use_TextwinAlpha)
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
	}
    /* It is actually faster to draw the background every frame than it is to use it for the surface and only redraw when needed. */
	else
	{
		if (old_alpha_option != options.use_TextwinAlpha)
		{
			WIDGET_REDRAW(widget);
		}

		sprite_blt(Bitmaps[BITMAP_TEXTWIN], x, y, &box, NULL);
	}

	/* Let's draw the widgets in the backbuffer */
	if (widget->redraw)
	{
		widget->redraw = 0;

		SDL_FillRect(widget->widgetSF, NULL, SDL_MapRGBA(widget->widgetSF->format, 0, 0, 0, options.textwin_alpha));

		bltfx.surface = widget->widgetSF;
		bltfx.flags = 0;
		bltfx.alpha = 0;

		if (options.use_TextwinAlpha)
		{
			box.x = 0;
			box.y = 0;
			box.h = 1;
			box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
			box.y = len;
			box.h = 1;
			box.x = 0;
			box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
			SDL_FillRect(widget->widgetSF, &box, SDL_MapRGBA(widget->widgetSF->format, 0x60, 0x60, 0x60, 255));
			box.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
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
		}

		show_window(widget, x, y - 2, &bltfx);
	}

	box.x = x;
	box.y = y;
	box2.x = 0;
	box2.y = 0;
	box2.w = Bitmaps[BITMAP_TEXTWIN]->bitmap->w;
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
		return;

	/* Scrolling or resizing */
	if (event.motion.x < widget->x1 || (textwin_flags & (TW_SCROLL | TW_RESIZE)))
	{
		return;
	}

    WIDGET_REDRAW(widget);

	/* Mousewheel up */
	if (event.button.button == 4)
	{
		textwin->scroll++;
	}
	/* Mousewheel down */
	else if (event.button.button == 5)
	{
		textwin->scroll--;
	}
	else if (event.button.button == SDL_BUTTON_LEFT)
	{
		/* Clicked scroller button up */
		if (textwin->highlight == TW_HL_UP)
		{
			textwin->scroll++;
		}
		/* Clicked above the slider */
		else if (textwin->highlight == TW_ABOVE)
		{
			textwin->scroll += textwin->size;
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
			textwin->scroll -= textwin->size;
		}
		/* Clicked scroller button down */
		else if (textwin->highlight == TW_HL_DOWN)
		{
			textwin->scroll--;
		}
		/* Size change */
		else if (event.motion.x < widget->x1 + 246 && event.motion.y > widget->y1 + 2 && event.motion.y < widget->y1 + 7 && cursor_type == 1)
		{
			textwin_flags |= TW_RESIZE;
		}
		else if (event.motion.x < widget->x1 + 250)
		{
			say_clickedKeyword(widget, event.motion.x, event.motion.y);
		}
	}
}

/**
 * Mouse move event inside the text window.
 * @param actWin The text window.
 * @param event SDL event type.
 * @return 1 if mouse is out of window, 0 otherwise. */
int textwin_move_event(widgetdata *widget, SDL_Event event)
{
	_textwin *textwin = TEXTWIN(widget);
	int newsize;

	if (!textwin)
		return 1;

	textwin->highlight = TW_HL_NONE;

	WIDGET_REDRAW(widget);

	/* Show resize cursor */
	if ((event.motion.y > widget->y1 + 2 && event.motion.y < widget->y1 + 7 && (event.motion.x < widget->x1 + 246)) || (event.button.button == SDL_BUTTON_LEFT && (textwin_flags & (TW_SCROLL | TW_RESIZE))))
	{
		if (!(textwin_flags & TW_SCROLL) && event.motion.x > widget->x1)
		{
			cursor_type = 1;
		}
	}
	else
	{
		cursor_type = 0;
		textwin_flags &= ~(TW_SCROLL | TW_RESIZE);
	}

	/* Mouse out of window */
	/* We have to leave this here! For sanity, also the widget stuff does some area checking */
	if (event.motion.y < widget->y1 || event.motion.x > widget->x1 + Bitmaps[BITMAP_TEXTWIN]->bitmap->w || event.motion.y > widget->y1 + textwin->size * 10 + 13 + Bitmaps[BITMAP_SLIDER_UP]->bitmap->h)
	{
		if (!(textwin_flags & TW_RESIZE))
		{
			return 1;
		}
	}

	/* Highlighting */
	if (event.motion.x > widget->x1 + 250 && event.motion.y > widget->y1 && event.motion.x < widget->x1 + Bitmaps[BITMAP_TEXTWIN]->bitmap->w)
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

		if (event.button.button != SDL_BUTTON_LEFT)
		{
			return 0;
		}
	}

	/* Slider scrolling */
	if (textwin_flags & TW_SCROLL)
	{
		textwin->slider_y = event.motion.y - old_slider_pos;
		textwin->scroll = textwin->act_bufsize - (textwin->size + 1) - (textwin->act_bufsize * textwin->slider_y) / (textwin->size * 10 - 1);
		WIDGET_REDRAW(widget);

		return 0;
	}

	/* Resizing */
	if (textwin_flags & TW_RESIZE)
	{
		newsize = ((widget->y1 + widget->ht) - event.motion.y) / 10;

		/* set minimum bounds */
		if (newsize < 3)
		{
			newsize = 3;
		}

		/* we need to calc the new x for the widget, and set the new size */
		resize_widget(widget, RESIZE_TOP, newsize * 10 + 13);

		textwin->size = newsize;
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
 * Add history to the text window.
 * @param text The text to add to the history. */
void textwin_addhistory(char *text)
{
	int i;

	/* If new line is empty or identical to last inserted one, skip it */
	if (!text[0] || strcmp(InputHistory[1], text) == 0)
	{
		return;
	}

	/* Shift history lines */
	for (i = MAX_HISTORY_LINES - 1; i > 1; i--)
	{
		strncpy(InputHistory[i], InputHistory[i - 1], MAX_INPUT_STRING);
	}

	/* Insert new one */
	strncpy(InputHistory[1], text, MAX_INPUT_STRING);
	/* Clear temporary editing line */
	*InputHistory[0] = '\0';
	HistoryPos = 0;
}

/**
 * Clear all the text window history. */
void textwin_clearhistory()
{
	int i;

	for (i = 0; i < MAX_HISTORY_LINES; i++)
	{
		/* It's enough to clear only the first byte of each history line */
		InputHistory[i][0] = '\0';
	}

	HistoryPos = 0;
}

/**
 * Put string to the text window.
 * @param text The string. */
void textwin_putstring(char *text)
{
	int len = (int) strlen(text);

	/* Copy buf to input buffer */
	strncpy(InputString, text, MAX_INPUT_STRING);

	/* Set cursor after inserted text */
	CurrentCursorPos = InputCount = len;
}

/**
 * Change the font used for drawing text in text windows.
 * @param font ID of the font to change to. */
void change_textwin_font(int font)
{
	switch (font)
	{
		case 0:
		default:
			textwin_font = &SystemFont;
			break;

		case 1:
			textwin_font = &MediumFont;
			break;
	}
}

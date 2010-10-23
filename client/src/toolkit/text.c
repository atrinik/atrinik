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
 * Text drawing API. Used SDL_ttf for rendering. */

#include <include.h>

/**
 * If 1, all text shown using 'box' parameter of string_blt() for max
 * width/height will have a frame around it. */
#define TEXT_SIZE_DEBUG 0

/** All the usable fonts. */
font_struct fonts[FONTS_MAX] =
{
	{"fonts/vera/sans.ttf", 8, NULL, 0},
	{"fonts/vera/sans.ttf", 10, NULL, 0},
	{"fonts/vera/sans.ttf", 11, NULL, 0},
	{"fonts/vera/sans.ttf", 12, NULL, 0},
	{"fonts/vera/sans.ttf", 13, NULL, 0},
	{"fonts/vera/sans.ttf", 14, NULL, 0},
	{"fonts/vera/sans.ttf", 15, NULL, 0},
	{"fonts/vera/sans.ttf", 16, NULL, 0},
	{"fonts/vera/sans.ttf", 18, NULL, 0},
	{"fonts/vera/sans.ttf", 20, NULL, 0},
	{"fonts/vera/serif.ttf", 8, NULL, 0},
	{"fonts/vera/serif.ttf", 10, NULL, 0},
	{"fonts/vera/serif.ttf", 12, NULL, 0},
	{"fonts/vera/serif.ttf", 14, NULL, 0},
	{"fonts/vera/serif.ttf", 16, NULL, 0},
	{"fonts/vera/serif.ttf", 18, NULL, 0},
	{"fonts/vera/serif.ttf", 20, NULL, 0},
	{"fonts/vera/mono.ttf", 8, NULL, 0},
	{"fonts/vera/mono.ttf", 10, NULL, 0},
	{"fonts/vera/mono.ttf", 12, NULL, 0},
	{"fonts/vera/mono.ttf", 14, NULL, 0},
	{"fonts/vera/mono.ttf", 16, NULL, 0},
	{"fonts/vera/mono.ttf", 18, NULL, 0},
	{"fonts/vera/mono.ttf", 20, NULL, 0},
	{"fonts/arial.ttf", 8, NULL, 0},
	{"fonts/arial.ttf", 10, NULL, 0},
	{"fonts/arial.ttf", 12, NULL, 0},
	{"fonts/arial.ttf", 14, NULL, 0},
	{"fonts/arial.ttf", 16, NULL, 0},
	{"fonts/arial.ttf", 18, NULL, 0},
	{"fonts/arial.ttf", 20, NULL, 0}
};

/**
 * Initialize the text API. Should only be done once. */
void text_init()
{
	size_t i;
	TTF_Font *font;

	TTF_Init();

	for (i = 0; i < FONTS_MAX; i++)
	{
		font = TTF_OpenFont(fonts[i].path, fonts[i].size);

		if (!font)
		{
			LOG(llevError, "Unable to load font (%s): %s\n", fonts[i].path, TTF_GetError());
		}

		fonts[i].font = font;
		fonts[i].height = TTF_FontLineSkip(font);
	}
}

/**
 * Deinitializes the text API. */
void text_deinit()
{
	size_t i;

	for (i = 0; i < FONTS_MAX; i++)
	{
		TTF_CloseFont(fonts[i].font);
	}

	TTF_Quit();
}

/**
 * Get font's ID from its xxx.ttf name (not including path) and the pixel
 * size.
 * @param name The font name.
 * @param size The size.
 * @return The font ID, -1 if there is no such font. */
static int get_font_id(const char *name, size_t size)
{
	size_t i;
	const char *cp;

	for (i = 0; i < FONTS_MAX; i++)
	{
		cp = strrchr(fonts[i].path, '/');

		if (!cp)
		{
			cp = fonts[i].path;
		}
		else
		{
			cp++;
		}

		if (!strcmp(cp, name) && fonts[i].size == size)
		{
			return i;
		}
	}

	return -1;
}

/**
 * Reset r, g, b values of 'color' to that of 'orig_color'.
 * @param surface If NULL, will not do anything.
 * @param color Values to copy to.
 * @param orig_color Values to copy from. */
static void reset_color(SDL_Surface *surface, SDL_Color *color, SDL_Color orig_color)
{
	if (!surface)
	{
		return;
	}

	color->r = orig_color.r;
	color->g = orig_color.g;
	color->b = orig_color.b;
}

/**
 * Draw one character on the screen or parse markup (if applicable).
 * @param[out] font Font to use. One of @ref FONT_xxx.
 * @param orig_font Original font, used for the font tag.
 * @param surface Surface to draw on. If NULL, there is no drawing done,
 * but the return value is still calculated along with dest->w.
 * @param dest Destination, will have width (and x, if surface wasn't
 * NULL) updated.
 * @param cp String we are working on, cp[0] is the character to draw.
 * @param color Color to use.
 * @param orig_color Original color.
 * @param flags Flags as passed to string_blt().
 * @return How many characters to jump. Usually 1, but can be more in
 * case of markup tags that need to be jumped over, since they are not
 * actually drawn. */
int blt_character(int *font, int orig_font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, SDL_Color *color, SDL_Color orig_color, int flags, SDL_Rect *box)
{
	int width, ret = 1;
	char c = *cp;
	static char *anchor_tag = NULL, anchor_action[MAX_BUF];

	/* Doing markup? */
	if (flags & TEXT_MARKUP && c == '<')
	{
		/* Color tag: <c=r,g,b> */
		if (!strncmp(cp, "<c=", 3))
		{
			char *pos;

			if (surface && !(flags & TEXT_NO_COLOR_CHANGE))
			{
				int r, g, b;

				/* Parse the r,g,b colors. */
				if (sscanf(cp, "<c=%d,%d,%d>", &r, &g, &b) == 3)
				{
					color->r = r;
					color->g = g;
					color->b = b;
				}
				else
				{
					return 3;
				}
			}

			/* Get the position of the ending '>'. */
			pos = strchr(cp, '>');

			if (!pos)
			{
				return 3;
			}

			return pos - cp + 1;
		}
		/* End of color tag. */
		else if (!strncmp(cp, "</c>", 4))
		{
			reset_color(surface, color, orig_color);
			return 4;
		}
		/* Convenience tag to make string green. */
		else if (!strncmp(cp, "<green>", 7))
		{
			if (surface && !(flags & TEXT_NO_COLOR_CHANGE))
			{
				color->r = 0;
				color->g = 255;
				color->b = 0;
			}

			return 7;
		}
		else if (!strncmp(cp, "</green>", 8))
		{
			reset_color(surface, color, orig_color);
			return 8;
		}
		/* Convenience tag to make string yellow. */
		else if (!strncmp(cp, "<yellow>", 8))
		{
			if (surface && !(flags & TEXT_NO_COLOR_CHANGE))
			{
				color->r = 255;
				color->g = 255;
				color->b = 0;
			}

			return 8;
		}
		else if (!strncmp(cp, "</yellow>", 9))
		{
			reset_color(surface, color, orig_color);
			return 9;
		}
		/* Convenience tag to make string red. */
		else if (!strncmp(cp, "<red>", 5))
		{
			if (surface && !(flags & TEXT_NO_COLOR_CHANGE))
			{
				color->r = 255;
				color->g = 0;
				color->b = 0;
			}

			return 5;
		}
		else if (!strncmp(cp, "</red>", 6))
		{
			reset_color(surface, color, orig_color);
			return 6;
		}
		/* Convenience tag to make string blue. */
		else if (!strncmp(cp, "<blue>", 6))
		{
			if (surface && !(flags & TEXT_NO_COLOR_CHANGE))
			{
				color->r = 0;
				color->g = 0;
				color->b = 255;
			}

			return 6;
		}
		else if (!strncmp(cp, "</blue>", 7))
		{
			reset_color(surface, color, orig_color);
			return 7;
		}
		/* Bold. */
		else if (!strncmp(cp, "<b>", 3))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_BOLD);
			}

			return 3;
		}
		else if (!strncmp(cp, "</b>", 4))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_BOLD);
			}

			return 4;
		}
		/* Italic. */
		else if (!strncmp(cp, "<i>", 3))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_ITALIC);
			}

			return 3;
		}
		else if (!strncmp(cp, "</i>", 4))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_ITALIC);
			}

			return 4;
		}
		/* Underscore. */
		else if (!strncmp(cp, "<u>", 3))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_UNDERLINE);
			}

			return 3;
		}
		else if (!strncmp(cp, "</u>", 4))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_UNDERLINE);
			}

			return 4;
		}
		/* Font change. */
		else if (!strncmp(cp, "<font=", 6))
		{
			char *pos;

			if (surface)
			{
				int font_size = 10;
				char font_name[MAX_BUF];

				if (sscanf(cp, "<font=%64[^ >] %d>", font_name, &font_size) >= 1)
				{
					int font_id = get_font_id(font_name, font_size);

					if (font_id == -1)
					{
						LOG(llevBug, "blt_character(): Invalid font in string (%s, %d): %.80s.\n", font_name, font_size, cp);
					}
					else
					{
						*font = font_id;
					}
				}
				else
				{
					return 6;
				}
			}

			/* Get the position of the ending '>'. */
			pos = strchr(cp, '>');

			if (!pos)
			{
				return 6;
			}

			return pos - cp + 1;
		}
		else if (!strncmp(cp, "</font>", 7))
		{
			if (surface)
			{
				*font = orig_font;
			}

			return 7;
		}
		/* Make text centered. */
		else if (!strncmp(cp, "<center>", 8))
		{
			/* Find the ending tag. */
			char *pos = strstr(cp, "</center");

			if (pos && box && box->w)
			{
				char *buf = malloc(pos - cp - 8);
				int w;

				/* Copy the string between <center> and </center> to a
				 * temporary buffer so we can calculate its width. */
				memcpy(buf, cp + 8, pos - cp - 8);
				buf[pos - cp - 8] = '\0';
				w = dest->x / 2 + box->w / 2 - string_get_width(*font, buf, flags) / 2;
				free(buf);

				if (surface)
				{
					dest->x = w;
				}

				dest->w = w;
			}

			return 8;
		}
		else if (!strncmp(cp, "</center>", 9))
		{
			return 9;
		}
		/* Anchor tag. */
		else if (!strncmp(cp, "<a>", 3) || !strncmp(cp, "<a=", 3))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_UNDERLINE);

				/* Change to light blue only if no custom color was specified. */
				if (color->r == orig_color.r && color->g == orig_color.g && color->b == orig_color.b)
				{
					color->r = 96;
					color->g = 160;
					color->b = 255;
				}

				anchor_tag = strchr(cp, '>') + 1;
				anchor_action[0] = '\0';
			}

			/* Scan for action other than the default. */
			if (sscanf(cp, "<a=%64[^>]>", anchor_action) == 1)
			{
				return strchr(cp + 3, '>') - cp + 1;
			}

			return 3;
		}
		else if (!strncmp(cp, "</a>", 4))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_UNDERLINE);
				reset_color(surface, color, orig_color);
				anchor_tag = NULL;
			}

			return 4;
		}
		else if (!strncmp(cp, "<y=", 3))
		{
			if (surface)
			{
				int height;

				if (sscanf(cp, "<y=%d>", &height) == 1)
				{
					dest->y += height;
				}
			}

			return strchr(cp + 3, '>') - cp + 1;
		}
	}

	/* Parse entities. */
	if (flags & TEXT_MARKUP && c == '&')
	{
		if (!strncmp(cp, "&lt;", 4))
		{
			c = '<';
			ret = 4;
		}
		else if (!strncmp(cp, "&gt;", 4))
		{
			c = '>';
			ret = 4;
		}
	}

	/* Get the glyph's metrics. */
	if (TTF_GlyphMetrics(fonts[*font].font, c, NULL, NULL, NULL, NULL, &width) == -1)
	{
		return ret;
	}

	/* Draw the character (unless it's a space, since there's no point in
	 * drawing whitespace [but only if underline style is not active,
	 * since we do want the underline below the space]). */
	if (surface && (c != ' ' || TTF_GetFontStyle(fonts[*font].font) & TTF_STYLE_UNDERLINE))
	{
		SDL_Surface *ttf_surface;
		char buf[2];
		int mx, my;
		static uint32 ticks = 0;

		buf[0] = c;
		buf[1] = '\0';

		/* Are we inside an anchor tag and we clicked the text? */
		if (anchor_tag && SDL_GetMouseState(&mx, &my) == SDL_BUTTON(SDL_BUTTON_LEFT) && mx >= dest->x && mx <= dest->x + width && my >= dest->y && my <= dest->y + FONT_HEIGHT(*font) && (!ticks || SDL_GetTicks() - ticks > 125))
		{
			size_t len;
			char *buf;

			ticks = SDL_GetTicks();

			/* Get the length of the text until the ending </a>. */
			len = strstr(anchor_tag, "</a>") - anchor_tag;
			/* Allocate a temporary buffer and copy the text until the
			 * ending </a>, so we have the text between the anchor tags. */
			buf = malloc(len + 1);
			memcpy(buf, anchor_tag, len);
			buf[len] = '\0';

			/* Default to executing player commands such as /say. */
			if (GameStatus == GAME_STATUS_PLAY && anchor_action[0] == '\0')
			{
				/* It's not a command, so prepend "/say " to it. */
				if (buf[0] != '/')
				{
					/* Resize the buffer so it can hold 5 more bytes. */
					buf = realloc(buf, len + 5 + 1);
					/* Copy the existing bytes to the end, so we have 5
					 * we can use in the front. */
					memmove(buf + 5, buf, len + 1);
					/* Prepend "/say ". */
					memcpy(buf, "/say ", 5);
				}

				send_command(buf);
			}
			/* Help GUI. */
			else if (GameStatus == GAME_STATUS_PLAY && !strcmp(anchor_action, "help"))
			{
				show_help(buf);
			}

			free(buf);
		}

		/* Render the character. */
		if (flags & TEXT_SOLID)
		{
			ttf_surface = TTF_RenderText_Solid(fonts[*font].font, buf, *color);
		}
		else
		{
			ttf_surface = TTF_RenderText_Blended(fonts[*font].font, buf, *color);
		}

		/* Output the rendered character to the screen and free the
		 * used surface. */
		SDL_BlitSurface(ttf_surface, NULL, surface, dest);
		SDL_FreeSurface(ttf_surface);
	}

	/* Update the x/w of the destination with the character's width. */
	if (surface)
	{
		dest->x += width;
	}

	dest->w += width;

	return ret;
}

/**
 * Draw a string on the specified surface.
 * @param surface Surface to draw on.
 * @param font Font to use. One of @ref FONT_xxx.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color Color to use.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void string_blt(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, int flags, SDL_Rect *box)
{
	const char *cp = text;
	SDL_Rect dest;
	int pos = 0, last_space = 0, is_lf, ret, skip, max_height, height = 0;
	SDL_Color orig_color = color;
	int orig_font = font;

#if TEXT_SIZE_DEBUG
	if (box)
	{
		draw_frame(surface, x, y, box->w, box->h);
	}
#endif

	/* Align to the center. */
	if (box && flags & TEXT_ALIGN_CENTER)
	{
		x += box->w / 2 - string_get_width(font, text, flags) / 2;
	}

	if (box && flags & TEXT_VALIGN_CENTER)
	{
		y += box->h / 2 - FONT_HEIGHT(font) / 2;
	}

	/* Store the x/y. */
	dest.x = x;
	dest.y = y;
	dest.w = 0;
	height = 0;
	max_height = 0;

	while (cp[pos] != '\0')
	{
		/* Have we gone over the height limit yet? */
		if (box && box->h && dest.y + FONT_HEIGHT(font) - y > box->h)
		{
			/* We are calculating height, keep going on but without any
			 * more drawing. */
			if (flags & TEXT_HEIGHT)
			{
				surface = NULL;
			}
			else
			{
				return;
			}
		}

		is_lf = cp[pos] == '\n';

		/* Is this a newline, or word wrap was set and we are over
		 * maximum width? */
		if (is_lf || (flags & TEXT_WORD_WRAP && box && box->w && dest.w > box->w))
		{
			/* Store the last space. */
			if (is_lf || last_space == 0)
			{
				last_space = pos;
			}

			/* See if we should skip drawing. */
			skip = (flags & TEXT_HEIGHT) && box->y && height / FONT_HEIGHT(font) < box->y;

			max_height = FONT_HEIGHT(font);

			/* Draw characters until we have reached the cut point (last_space). */
			while (*cp != '\0' && last_space > 0)
			{
				ret = blt_character(&font, orig_font, skip ? NULL : surface, &dest, cp, &color, orig_color, flags, box);
				cp += ret;
				last_space -= ret;

				/* If we changed font, there might be a larger one... */
				if (font != orig_font && FONT_HEIGHT(font) > max_height)
				{
					max_height = FONT_HEIGHT(font);
				}
			}

			/* Update the Y position. */
			if (!skip)
			{
				dest.y += max_height;
			}

			height += max_height;

			/* Jump over the newline, if any. */
			if (is_lf)
			{
				cp++;
			}

			/* Strip leading spaces. */
			while (*cp != '\0' && *cp == ' ')
			{
				cp++;
			}

			/* Update the coordinates. */
			last_space = pos = 0;
			dest.w = 0;
			dest.x = x;
			max_height = 0;
		}
		else
		{
			/* Store last space position. */
			if (cp[pos] == ' ')
			{
				last_space = pos;
			}

			/* Do not do any drawing, just calculate how many characters
			 * to jump and the width. */
			pos += blt_character(&font, font, NULL, &dest, cp + pos, &color, orig_color, flags, box);
		}
	}

	/* Draw leftover characters. */
	while (*cp != '\0')
	{
		cp += blt_character(&font, orig_font, surface, &dest, cp, &color, orig_color, flags, box);
	}

	/* Give caller access to the calculated height. */
	if (box && flags & TEXT_HEIGHT)
	{
		box->h = height;
	}
}

/**
 * Draw a string with a shadow.
 * @param surface Surface to draw on.
 * @param font Font to use. One of @ref FONT_xxx.
 * @param text The string to draw.
 * @param x X position.
 * @param y Y position.
 * @param color Color to use.
 * @param color_shadow Color to use for the shadow.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @param box If word wrap was enabled by passing @ref TEXT_WORD_WRAP as
 * one of the 'flags', this is used to get the max width from. Also even
 * if word wrap is disabled, this is used to get the max height from, if
 * set (both box->w and box->h can be 0 to indicate unlimited). */
void string_blt_shadow(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, SDL_Color color_shadow, int flags, SDL_Rect *box)
{
	string_blt(surface, font, text, x + 1, y - 1, color_shadow, flags | TEXT_NO_COLOR_CHANGE, box);
	string_blt(surface, font, text, x, y - 2, color, flags, box);
}

/**
 * Like string_blt(), but allows using printf-like format specifiers.
 *
 * @copydoc string_blt() */
void string_blt_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, int flags, SDL_Rect *box, const char *text, ...)
{
	char buf[HUGE_BUF * 4];
	va_list ap;

	va_start(ap, text);
	vsnprintf(buf, sizeof(buf), text, ap);
	string_blt(surface, font, buf, x, y, color, flags, box);
	va_end(ap);
}

/**
 * Like string_blt_shadow(), but allows using printf-like format specifiers.
 *
 * @copydoc string_blt_shadow() */
void string_blt_shadow_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, SDL_Color color_shadow, int flags, SDL_Rect *box, const char *text, ...)
{
	char buf[HUGE_BUF * 4];
	va_list ap;

	va_start(ap, text);
	vsnprintf(buf, sizeof(buf), text, ap);
	string_blt_shadow(surface, font, buf, x, y, color, color_shadow, flags, box);
	va_end(ap);
}

/**
 * Calculate string's pixel width, taking into account markup, if
 * applicable.
 * @param font Font. One of @ref FONT_xxx.
 * @param text String to get width of.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @return The string's width. */
int string_get_width(int font, const char *text, int flags)
{
	SDL_Rect dest;
	const char *cp = text;

	dest.w = 0;

	while (*cp != '\0')
	{
		cp += blt_character(&font, font, NULL, &dest, cp, NULL, (SDL_Color) {0, 0, 0, 0}, flags, NULL);
	}

	return dest.w;
}

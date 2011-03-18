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
 * Text drawing API. Used SDL_ttf for rendering. */

#include <include.h>

/**
 * If 1, all text shown using 'box' parameter of string_blt() for max
 * width/height will have a frame around it. */
static uint8 text_debug = 0;

/**
 * If not 0, ::text_anchor_help is used to open the help GUI after
 * finishing drawing.
 *
 * This is used so the text being drawn is not changed in the middle of
 * the drawing by clicking on a link. */
static uint8 text_anchor_help_clicked = 0;

/** Help GUI to open if ::text_anchor_help_clicked is 1. */
static char text_anchor_help[HUGE_BUF];

/** All the usable fonts. */
font_struct fonts[FONTS_MAX] =
{
	{"fonts/vera/sans.ttf", 7, NULL, 0},
	{"fonts/vera/sans.ttf", 8, NULL, 0},
	{"fonts/vera/sans.ttf", 9, NULL, 0},
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
	{"fonts/vera/serif.ttf", 22, NULL, 0},
	{"fonts/vera/serif.ttf", 24, NULL, 0},
	{"fonts/vera/serif.ttf", 26, NULL, 0},
	{"fonts/vera/serif.ttf", 28, NULL, 0},
	{"fonts/vera/serif.ttf", 30, NULL, 0},
	{"fonts/vera/serif.ttf", 32, NULL, 0},
	{"fonts/vera/serif.ttf", 34, NULL, 0},
	{"fonts/vera/serif.ttf", 36, NULL, 0},
	{"fonts/vera/serif.ttf", 38, NULL, 0},
	{"fonts/vera/serif.ttf", 40, NULL, 0},
	{"fonts/vera/mono.ttf", 8, NULL, 0},
	{"fonts/vera/mono.ttf", 9, NULL, 0},
	{"fonts/vera/mono.ttf", 10, NULL, 0},
	{"fonts/vera/mono.ttf", 12, NULL, 0},
	{"fonts/vera/mono.ttf", 14, NULL, 0},
	{"fonts/vera/mono.ttf", 16, NULL, 0},
	{"fonts/vera/mono.ttf", 18, NULL, 0},
	{"fonts/vera/mono.ttf", 20, NULL, 0},
	{"fonts/arial.ttf", 8, NULL, 0},
	{"fonts/arial.ttf", 10, NULL, 0},
	{"fonts/arial.ttf", 11, NULL, 0},
	{"fonts/arial.ttf", 12, NULL, 0},
	{"fonts/arial.ttf", 13, NULL, 0},
	{"fonts/arial.ttf", 14, NULL, 0},
	{"fonts/arial.ttf", 15, NULL, 0},
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
	uint8 ext = strstr(name, ".ttf") ? 1 : 0;

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

		if ((!strcmp(cp, name) || (!ext && !strncmp(cp, name, strlen(cp) - 4))) && fonts[i].size == size)
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
static void reset_color(SDL_Surface *surface, SDL_Color *color, SDL_Color *orig_color)
{
	if (!surface)
	{
		return;
	}

	color->r = orig_color->r;
	color->g = orig_color->g;
	color->b = orig_color->b;
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
int blt_character(int *font, int orig_font, SDL_Surface *surface, SDL_Rect *dest, const char *cp, SDL_Color *color, SDL_Color *orig_color, uint64 flags, SDL_Rect *box, int *x_adjust)
{
	int width, minx, ret = 1;
	char c = *cp;
	static char *anchor_tag = NULL, anchor_action[HUGE_BUF];
	static SDL_Color outline_color = {0, 0, 0, 0};
	static uint8 outline_show = 0, in_book_title = 0, used_alpha = 255;

	if (c == '\r')
	{
		SDL_Color new_color = COLOR_SIMPLE((uint8) cp[1] - 1);

		color->r = orig_color->r = new_color.r;
		color->g = orig_color->g = new_color.g;
		color->b = orig_color->b = new_color.b;

		return 2;
	}

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
				if ((cp[3] == '#' && sscanf(cp, "<c=#%2X%2X%2X>", &r, &g, &b) == 3) || sscanf(cp, "<c=%d,%d,%d>", &r, &g, &b) == 3)
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
			TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_BOLD);
			return 3;
		}
		else if (!strncmp(cp, "</b>", 4))
		{
			TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_BOLD);
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
			int font_size = 10;
			char font_name[MAX_BUF];

			if (!(flags & TEXT_NO_FONT_CHANGE) && sscanf(cp, "<font=%64[^ >] %d>", font_name, &font_size) >= 1)
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

			/* Get the position of the ending '>'. */
			pos = strchr(cp, '>');

			if (!pos)
			{
				return 6;
			}

			return pos - cp + 1;
		}
		else if (!strncmp(cp, "<size=", 6))
		{
			int font_size;
			char *pos;

			if (!(flags & TEXT_NO_FONT_CHANGE) && sscanf(cp, "<size=%d>", &font_size) == 1)
			{
				const char *tmp = strrchr(fonts[*font].path, '/');
				int font_id;

				if (!tmp)
				{
					tmp = fonts[*font].path;
				}
				else
				{
					tmp++;
				}

				font_id = get_font_id(tmp, font_size);

				if (font_id == -1)
				{
					LOG(llevBug, "blt_character(): Invalid font in string (%d): %.80s.\n", font_size, cp);
				}
				else
				{
					*font = font_id;
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
		else if (!strncmp(cp, "</font>", 7) || !strncmp(cp, "</size>", 7))
		{
			*font = orig_font;
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
				w = dest->x + box->w / 2 - string_get_width(*font, buf, flags) / 2;
				free(buf);

				if (surface)
				{
					dest->x = w;
				}
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
				/* Change to light blue only if no custom color was specified. */
				if (color->r == orig_color->r && color->g == orig_color->g && color->b == orig_color->b)
				{
					color->r = 96;
					color->g = 160;
					color->b = 255;
				}

				anchor_tag = strchr(cp, '>') + 1;
				anchor_action[0] = '\0';
				outline_show = 1;
			}

			/* Scan for action other than the default. */
			if (sscanf(cp, "<a=%1024[^>]>", anchor_action) == 1)
			{
				return strchr(cp + 3, '>') - cp + 1;
			}

			return 3;
		}
		else if (!strncmp(cp, "</a>", 4))
		{
			if (surface)
			{
				reset_color(surface, color, orig_color);
				anchor_tag = NULL;
				outline_show = 0;
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
		else if (!strncmp(cp, "<x=", 3))
		{
			if (surface)
			{
				int w;

				if (sscanf(cp, "<x=%d>", &w) == 1)
				{
					dest->x += w;
				}
			}

			return strchr(cp + 3, '>') - cp + 1;
		}
		else if (!strncmp(cp, "<img=", 5))
		{
			if (surface)
			{
				char face[MAX_BUF];
				int x = 0, y = 0, alpha = 255, align = 0;

				if (sscanf(cp, "<img=%128[^ >] %d %d %d %d>", face, &x, &y, &align, &alpha) >= 1)
				{
					_BLTFX bltfx;
					int id = get_bmap_id(face);

					bltfx.surface = surface;
					bltfx.alpha = alpha;
					bltfx.flags = alpha != 255 ? BLTFX_FLAG_SRCALPHA : 0;

					if (id != -1 && FaceList[id].sprite)
					{
						if (align & 1)
						{
							x += box->w - FaceList[id].sprite->bitmap->w - dest->x;
						}

						if (align & 2)
						{
							y += -dest->y;
						}

						sprite_blt(FaceList[id].sprite, dest->x + x, dest->y + y, NULL, &bltfx);
					}
				}
			}

			return strchr(cp + 5, '>') - cp + 1;
		}
		else if (!strncmp(cp, "<o=", 3))
		{
			if (surface)
			{
				int r, g, b;

				/* Parse the r,g,b colors. */
				if ((cp[3] == '#' && sscanf(cp, "<o=#%2X%2X%2X>", &r, &g, &b) == 3) || sscanf(cp, "<o=%d,%d,%d>", &r, &g, &b) == 3)
				{
					outline_color.r = r;
					outline_color.g = g;
					outline_color.b = b;
					outline_show = 1;
				}
			}

			return strchr(cp + 3, '>') - cp + 1;
		}
		else if (!strncmp(cp, "</o>", 4))
		{
			if (surface)
			{
				outline_color.r = outline_color.g = outline_color.b = 0;
				outline_show = 0;
			}

			return 4;
		}
		else if (!strncmp(cp, "<alpha=", 7))
		{
			if (surface)
			{
				int alpha;

				if (sscanf(cp + 7, "%d>", &alpha) == 1)
				{
					used_alpha = alpha;
				}
			}

			return strchr(cp + 3, '>') - cp + 1;
		}
		else if (!strncmp(cp, "</alpha>", 8))
		{
			if (surface)
			{
				used_alpha = 255;
			}

			return 8;
		}
		else if (!strncmp(cp, "<book=", 6))
		{
			char *pos = strchr(cp + 6, '>');

			if (!pos)
			{
				return 6;
			}

			if (flags & TEXT_LINES_CALC)
			{
				book_name_change(cp + 6, pos - cp - 6);
			}

			return pos - cp + 1;
		}
		else if (!strncmp(cp, "<b t=\"", 6))
		{
			char *pos = strchr(cp + 6, '>');

			if (!pos)
			{
				return 6;
			}

			if (flags & TEXT_LINES_CALC)
			{
				book_name_change(cp + 6, pos - cp - 7);
			}

			return pos - cp + 1;
		}
		else if (!strncmp(cp, "<book>", 6))
		{
			char *pos = strstr(cp, "</book");

			if (!pos)
			{
				return 6;
			}

			if (flags & TEXT_LINES_CALC)
			{
				book_name_change(cp + 6, pos - cp - 6);
			}

			return pos - cp + 7;
		}
		else if (!strncmp(cp, "<p>", 3))
		{
			if (surface && box && box->w)
			{
				SDL_Rect rect;

				rect.y = dest->y + FONT_HEIGHT(*font) / 2;
				rect.w = 1;
				rect.h = 3;
				rect.x = dest->x;
				SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
				rect.x += box->w;
				SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));

				rect.x = dest->x + 1;
				rect.w = box->w - 1;
				rect.h = 1;
				SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
				rect.y++;
				SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 110, 110, 110));
				rect.y++;
				SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, 96, 96, 96));
			}

			return 3;
		}
		else if (!strncmp(cp, "<title>", 7))
		{
			if (!(flags & TEXT_NO_FONT_CHANGE))
			{
				*font = FONT_SERIF14;
			}

			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_UNDERLINE);
			}

			return 7;
		}
		else if (!strncmp(cp, "</title>", 8))
		{
			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_UNDERLINE);
			}

			*font = orig_font;
			return 8;
		}
		else if (!strncmp(cp, "<t t=\"", 6) || !strncmp(cp, "<tt=\"", 5))
		{
			in_book_title = 1;

			if (!(flags & TEXT_NO_FONT_CHANGE))
			{
				*font = FONT_SERIF14;
			}

			if (surface)
			{
				TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) | TTF_STYLE_UNDERLINE);
			}

			return strchr(cp + 4, '"') - cp + 1;
		}
		else if (!strncmp(cp, "<padding=", 9))
		{
			int val;

			if (x_adjust && sscanf(cp + 9, "%d>", &val) == 1)
			{
				if (surface)
				{
					dest->x += val;
				}

				dest->w += val;
				*x_adjust = val;
			}

			return strchr(cp + 3, '>') - cp + 1;
		}
		else if (!strncmp(cp, "</padding>", 10))
		{
			if (x_adjust)
			{
				*x_adjust = 0;
			}

			return 10;
		}
	}

	if (in_book_title && !strncmp(cp, "\">", 2))
	{
		if (surface)
		{
			TTF_SetFontStyle(fonts[*font].font, TTF_GetFontStyle(fonts[*font].font) & ~TTF_STYLE_UNDERLINE);
		}

		*font = orig_font;
		in_book_title = 0;
		return 2;
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
	if (TTF_GlyphMetrics(fonts[*font].font, c, &minx, NULL, NULL, NULL, &width) == -1)
	{
		return ret;
	}

	if (minx < 0)
	{
		width -= minx;
	}

	/* Draw the character (unless it's a space, since there's no point in
	 * drawing whitespace [but only if underline style is not active,
	 * since we do want the underline below the space]). */
	if (surface && (c != ' ' || TTF_GetFontStyle(fonts[*font].font) & TTF_STYLE_UNDERLINE || anchor_tag))
	{
		SDL_Surface *ttf_surface;
		char buf[2];
		int mx, my;
		static uint32 ticks = 0;

		buf[0] = c;
		buf[1] = '\0';

		/* Are we inside an anchor tag and we clicked the text? */
		if (anchor_tag && SDL_GetMouseState(&mx, &my) == SDL_BUTTON(SDL_BUTTON_LEFT))
		{
			if (surface != ScreenSurface)
			{
				widgetdata *widget = widget_find_by_surface(surface);

				if (widget)
				{
					mx -= widget->x1;
					my -= widget->y1;
				}
			}

			if (mx >= dest->x && mx <= dest->x + width && my >= dest->y && my <= dest->y + FONT_HEIGHT(*font) && (!ticks || SDL_GetTicks() - ticks > 125))
			{
				size_t len;
				char *buf, *pos;

				ticks = SDL_GetTicks();

				pos = strchr(anchor_action, ':');

				if (pos && pos + 1)
				{
					buf = strdup(pos + 1);
					len = strlen(buf);
					anchor_action[pos - anchor_action] = '\0';
				}
				else
				{
					/* Get the length of the text until the ending </a>. */
					len = strstr(anchor_tag, "</a>") - anchor_tag;
					/* Allocate a temporary buffer and copy the text until the
					 * ending </a>, so we have the text between the anchor tags. */
					buf = malloc(len + 1);
					memcpy(buf, anchor_tag, len);
					buf[len] = '\0';
				}

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
					strncpy(text_anchor_help, buf, sizeof(text_anchor_help) - 1);
					text_anchor_help[sizeof(text_anchor_help) - 1] = '\0';
					text_anchor_help_clicked = 1;
				}
				else if (!strcmp(anchor_action, "url"))
				{
					browser_open(buf);
				}

				free(buf);
			}
		}

		if (outline_show || flags & TEXT_OUTLINE)
		{
			int outline_x, outline_y;
			SDL_Rect outline_box;

			for (outline_x = -1; outline_x < 2; outline_x++)
			{
				for (outline_y = -1; outline_y < 2; outline_y++)
				{
					outline_box.x = dest->x + outline_x;
					outline_box.y = dest->y + outline_y;

					if (flags & TEXT_SOLID)
					{
						ttf_surface = TTF_RenderText_Solid(fonts[*font].font, buf, outline_color);
					}
					else
					{
						ttf_surface = TTF_RenderText_Blended(fonts[*font].font, buf, outline_color);
					}

					SDL_BlitSurface(ttf_surface, NULL, surface, &outline_box);
					SDL_FreeSurface(ttf_surface);
				}
			}
		}

		/* Render the character. */
		if (flags & TEXT_SOLID || used_alpha != 255)
		{
			ttf_surface = TTF_RenderText_Solid(fonts[*font].font, buf, *color);

			/* Opacity. */
			if (used_alpha != 255)
			{
				SDL_Surface *new_ttf_surface;

				/* Remove black border. */
				SDL_SetColorKey(ttf_surface, SDL_SRCCOLORKEY | SDL_ANYFORMAT, 0);
				/* Set the opacity. */
				SDL_SetAlpha(ttf_surface, SDL_SRCALPHA | SDL_RLEACCEL, used_alpha);
				/* Create new surface to blit. */
				new_ttf_surface = SDL_DisplayFormatAlpha(ttf_surface);
				/* Free the old one. */
				SDL_FreeSurface(ttf_surface);
				ttf_surface = new_ttf_surface;
			}
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
 * Get glyph's width.
 * @param font Font of the glyph.
 * @param c The glyph.
 * @return The width. */
int glyph_get_width(int font, char c)
{
	int minx, width;

	if (TTF_GlyphMetrics(fonts[font].font, c, &minx, NULL, NULL, NULL, &width) != -1)
	{
		if (minx < 0)
		{
			width -= minx;
		}

		return width;
	}

	return 0;
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
void string_blt(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, uint64 flags, SDL_Rect *box)
{
	const char *cp = text;
	SDL_Rect dest;
	int pos = 0, last_space = 0, is_lf, ret, skip, max_height, height = 0;
	SDL_Color orig_color = color;
	int orig_font = font, lines = 1, width = 0;
	uint16 *heights = NULL;
	size_t num_heights = 0;
	int x_adjust = 0;

	if (text_debug && box && surface)
	{
		draw_frame(surface, x, y, box->w, box->h);
	}

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
			/* We are calculating height/lines, keep going on but without
			 * any more drawing. */
			if (flags & (TEXT_HEIGHT | TEXT_LINES_CALC))
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
		if (is_lf || (flags & TEXT_WORD_WRAP && box && box->w && dest.w + glyph_get_width(font, cp[pos]) > box->w))
		{
			/* Store the last space. */
			if (is_lf || last_space == 0)
			{
				last_space = pos;
			}

			/* See if we should skip drawing. */
			skip = (flags & TEXT_HEIGHT) && box->y && height / FONT_HEIGHT(font) < box->y;

			max_height = FONT_HEIGHT(font);

			if (flags & TEXT_LINES_SKIP)
			{
				skip = box->y && lines - 1 < box->y;
			}

			/* Draw characters until we have reached the cut point (last_space). */
			while (*cp != '\0' && last_space > 0)
			{
				ret = blt_character(&font, orig_font, skip ? NULL : surface, &dest, cp, &color, &orig_color, flags, box, &x_adjust);
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
			lines++;

			/* Calculating lines, store the height of this line. */
			if (flags & TEXT_LINES_CALC)
			{
				heights = realloc(heights, sizeof(*heights) * (num_heights + 1));
				heights[num_heights] = max_height;
				num_heights++;
			}

			/* Jump over the newline, if any. */
			if (is_lf)
			{
				cp++;
			}
			else
			{
				/* Strip leading spaces. */
				while (*cp != '\0' && *cp == ' ')
				{
					cp++;
				}
			}

			/* Update the coordinates. */
			last_space = pos = 0;
			dest.w = x_adjust;
			dest.x = x + x_adjust;
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
			pos += blt_character(&font, orig_font, NULL, &dest, cp + pos, &color, &orig_color, flags, box, &x_adjust);
		}
	}

	max_height = FONT_HEIGHT(font);

	/* Draw leftover characters. */
	while (*cp != '\0')
	{
		if (flags & TEXT_WIDTH && box)
		{
			int w = glyph_get_width(font, *cp);

			if (box->w && width + w > box->w)
			{
				break;
			}

			width += w;
		}

		cp += blt_character(&font, orig_font, surface, &dest, cp, &color, &orig_color, flags, box, &x_adjust);

		/* If we changed font, there might be a larger one... */
		if (font != orig_font && FONT_HEIGHT(font) > max_height)
		{
			max_height = FONT_HEIGHT(font);
		}
	}

	/* Give caller access to the calculated height. */
	if (box && flags & TEXT_HEIGHT)
	{
		box->h = height;
	}

	/* Calculating lines? */
	if (box && flags & TEXT_LINES_CALC)
	{
		int total_height = 0, i, last_lines = 0;

		heights = realloc(heights, sizeof(*heights) * (num_heights + 1));
		heights[num_heights] = max_height;
		num_heights++;

		/* Go backwards to figure out the maximum number of lines shown
		 * at the end of the string. */
		for (i = num_heights - 1; i >= 0; i--)
		{
			if (total_height + heights[i] > box->h)
			{
				break;
			}

			total_height += heights[i];
			last_lines++;
		}

		free(heights);
		box->y = last_lines;
		box->h = lines;
	}

	if (text_anchor_help_clicked)
	{
		text_anchor_help_clicked = 0;
		show_help(text_anchor_help);
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
void string_blt_shadow(SDL_Surface *surface, int font, const char *text, int x, int y, SDL_Color color, SDL_Color color_shadow, uint64 flags, SDL_Rect *box)
{
	string_blt(surface, font, text, x + 1, y - 1, color_shadow, flags | TEXT_NO_COLOR_CHANGE, box);
	string_blt(surface, font, text, x, y - 2, color, flags, box);
}

/**
 * Like string_blt(), but allows using printf-like format specifiers.
 *
 * @copydoc string_blt() */
void string_blt_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, uint64 flags, SDL_Rect *box, const char *text, ...)
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
void string_blt_shadow_format(SDL_Surface *surface, int font, int x, int y, SDL_Color color, SDL_Color color_shadow, uint64 flags, SDL_Rect *box, const char *text, ...)
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
int string_get_width(int font, const char *text, uint64 flags)
{
	SDL_Rect dest;
	const char *cp = text;

	dest.w = 0;

	while (*cp != '\0')
	{
		cp += blt_character(&font, font, NULL, &dest, cp, NULL, NULL, flags, NULL, NULL);
	}

	return dest.w;
}

/**
 * Calculate string's pixel height, taking into account markup, if
 * applicable.
 *
 * It is usually enough to use FONT_HEIGHT() to get the string's
 * font height, unless markup is allowed, in which case the maximum used
 * height might be different.
 * @param font Font. One of @ref FONT_xxx.
 * @param text String to get height of.
 * @param flags One or a combination of @ref TEXT_xxx.
 * @return The string's height. */
int string_get_height(int font, const char *text, uint64 flags)
{
	SDL_Rect dest;
	const char *cp;
	int max_height;

	max_height = FONT_HEIGHT(font);

	/* No markup, the text cannot become different size. */
	if (!(flags & TEXT_MARKUP))
	{
		return max_height;
	}

	cp = text;
	dest.w = 0;

	while (*cp != '\0')
	{
		cp += blt_character(&font, font, NULL, &dest, cp, NULL, NULL, flags, NULL, NULL);

		if (FONT_HEIGHT(font) > max_height)
		{
			max_height = FONT_HEIGHT(font);
		}
	}

	return max_height;
}

/**
 * Enable text debugging. */
void text_enable_debug()
{
	text_debug = 1;
}

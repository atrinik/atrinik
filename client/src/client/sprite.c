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
 * Sprite related functions. */

#include <include.h>

/** Anim queue of current active map */
struct _anim *start_anim;

/** Format holder for red_scale(), fow_scale() and grey_scale() functions. */
static SDL_Surface *FormatHolder;

/** Darkness alpha values. */
static int dark_alpha[DARK_LEVELS] =
{
	0, 44, 80, 117, 153, 190, 226
};

/** Darkness filter. */
static SDL_Surface *darkness_filter[DARK_LEVELS] =
{
	NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static void red_scale(_Sprite *sprite);
static void grey_scale(_Sprite *sprite);
static void fow_scale(_Sprite *sprite);
static int GetBitmapBorders(SDL_Surface *Surface, int *up, int *down, int *left, int *right, uint32 ckey);
static void zoomSurfaceY(SDL_Surface *src, SDL_Surface *dst);
static void zoomSurfaceRGBA(SDL_Surface * src, SDL_Surface * dst, int flipx, int flipy, int smooth);

/**
 * Initialize the sprite system. */
void sprite_init_system()
{
	FormatHolder = SDL_CreateRGBSurface(SDL_SRCALPHA, 1, 1, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	SDL_SetAlpha(FormatHolder, SDL_SRCALPHA, 255);
}

/**
 * Load sprite file.
 * @param fname Sprite filename.
 * @param flags Flags for the sprite.
 * @return NULL if failed, the sprite otherwise. */
_Sprite *sprite_load_file(char *fname, uint32 flags)
{
	_Sprite *sprite = sprite_tryload_file(fname, flags, NULL);

	if (sprite == NULL)
	{
		LOG(llevBug, "sprite_load_file(): Can't load sprite %s\n", fname);
		return NULL;
	}

	return sprite;
}

/**
 * Try to load a sprite image file.
 * @param fname Sprite filename
 * @param flag Flags
 * @param rwop Pointer to memory for the image
 * @return The sprite if success, NULL otherwise */
_Sprite *sprite_tryload_file(char *fname, uint32 flag, SDL_RWops *rwop)
{
	_Sprite *sprite;
	SDL_Surface *bitmap;
	uint32 ckflags, tmp = 0;

	if (fname)
	{
		if (!(bitmap = IMG_Load_wrapper(fname)))
		{
			return NULL;
		}
	}
	else
	{
		bitmap = IMG_LoadPNG_RW(rwop);
	}

	if (!(sprite = malloc(sizeof(_Sprite))))
	{
		return NULL;
	}

	memset(sprite, 0, sizeof(_Sprite));

	sprite->status = SPRITE_STATUS_LOADED;
	sprite->type = SPRITE_TYPE_NORMAL;

	ckflags = SDL_SRCCOLORKEY | SDL_ANYFORMAT;

	if (options.rleaccel_flag)
	{
		ckflags |= SDL_RLEACCEL;
	}

	if (bitmap->format->palette)
	{
		SDL_SetColorKey(bitmap, ckflags, (tmp = bitmap->format->colorkey));
	}
	/* We force a true color png to colorkey. Default colkey is black (0). */
	else if (flag & SURFACE_FLAG_COLKEY_16M)
	{
		SDL_SetColorKey(bitmap, ckflags, 0);
	}

	GetBitmapBorders(bitmap, &sprite->border_up, &sprite->border_down, &sprite->border_left, &sprite->border_right, tmp);

	/* We store our original bitmap */
	sprite->bitmap = bitmap;

	if (flag & SURFACE_FLAG_DISPLAYFORMAT)
	{
		sprite->bitmap = SDL_DisplayFormat(bitmap);
		SDL_FreeSurface(bitmap);
	}

	return sprite;
}

/**
 * Free a spriate.
 * @param sprite Sprite to free. */
void sprite_free_sprite(_Sprite *sprite)
{
	void *tmp_free;
	int i;

	if (!sprite)
	{
		return;
	}

	if (sprite->bitmap)
	{
		SDL_FreeSurface(sprite->bitmap);
	}

	if (sprite->grey)
	{
		SDL_FreeSurface(sprite->grey);
	}

	if (sprite->red)
	{
		SDL_FreeSurface(sprite->red);
	}

	if (sprite->fog_of_war)
	{
		SDL_FreeSurface(sprite->fog_of_war);
	}

	if (sprite->dark_level)
	{
		for (i = 0; i < DARK_LEVELS; i++)
		{
			if (sprite->dark_level[i])
			{
				SDL_FreeSurface(sprite->dark_level[i]);
			}
		}
	}

	tmp_free = sprite;
	FreeMemory(&tmp_free);
}

/**
 * Blit a sprite.
 * @param sprite Sprite.
 * @param x X position.
 * @param y Y position.
 * @param box Box.
 * @param bltfx Bltfx. */
void sprite_blt(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx)
{
	SDL_Rect dst;
	SDL_Surface *surface, *blt_sprite;

	if (!sprite)
	{
		return;
	}

	blt_sprite = sprite->bitmap;

	if (bltfx && bltfx->surface)
	{
		surface = bltfx->surface;
	}
	else
	{
		surface = ScreenSurface;
	}

	dst.x = x;
	dst.y = y;

	if (bltfx)
	{
		if (bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
		{
			SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, bltfx->alpha);
		}
	}

	if (!blt_sprite)
	{
		return;
	}

	if (box)
	{
		SDL_BlitSurface(blt_sprite, box, surface, &dst);
	}
	else
	{
		SDL_BlitSurface(blt_sprite, NULL, surface, &dst);
	}

	if (bltfx && bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
	{
		SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
	}
}

/**
 * Blit a sprite on map surface.
 * @param sprite Sprite.
 * @param x X position.
 * @param y Y position.
 * @param box Box.
 * @param bltfx Bltfx.
 * @param stretch How much to stretch the sprite. */
void sprite_blt_map(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx, uint32 stretch, sint16 zoom)
{
	SDL_Rect dst;
	SDL_Surface *surface, *blt_sprite, *tmp;

	if (!sprite)
	{
		return;
	}

	blt_sprite = sprite->bitmap;
	surface = ScreenSurfaceMap;
	dst.x = x;
	dst.y = y;

	if (bltfx)
	{
		if (bltfx->flags & BLTFX_FLAG_DARK)
		{
			/* Last dark level is "no color" */
			if (bltfx->dark_level == DARK_LEVELS)
			{
				return;
			}

			/* We create the filter surfaces only when needed, and then store them */
			if (!darkness_filter[bltfx->dark_level])
			{
				SDL_SetAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap, SDL_SRCALPHA, dark_alpha[bltfx->dark_level]);
				darkness_filter[bltfx->dark_level] = SDL_DisplayFormatAlpha(Bitmaps[BITMAP_TEXTWIN_MASK]->bitmap);
			}

			if (sprite->dark_level[bltfx->dark_level])
			{
				blt_sprite = sprite->dark_level[bltfx->dark_level];
			}
			else
			{
				blt_sprite = SDL_DisplayFormatAlpha(sprite->bitmap);
				SDL_BlitSurface(darkness_filter[bltfx->dark_level], NULL, blt_sprite, NULL);
				sprite->dark_level[bltfx->dark_level] = blt_sprite;
			}
		}
		else if (bltfx->flags & BLTFX_FLAG_FOW)
		{
			if (!sprite->fog_of_war)
			{
				fow_scale(sprite);
			}

			blt_sprite = sprite->fog_of_war;
		}
		else if (bltfx->flags & BLTFX_FLAG_RED)
		{
			if (!sprite->red)
			{
				red_scale(sprite);
			}

			blt_sprite = sprite->red;
		}
		else if (bltfx->flags & BLTFX_FLAG_GREY)
		{
			if (!sprite->grey)
			{
				grey_scale(sprite);
			}

			blt_sprite = sprite->grey;
		}

		if (!blt_sprite)
		{
			return;
		}

		if (bltfx->flags & BLTFX_FLAG_STRETCH)
		{
			Uint8 n = (stretch >> 24) & 0xFF;
			Uint8 e = (stretch >> 16) & 0xFF;
			Uint8 w = (stretch >> 8) & 0xFF;
			Uint8 s = stretch & 0xFF;
			int ht_diff;

			tmp = tile_stretch(blt_sprite, n, e, s, w);

			if (!tmp)
			{
				return;
			}

			ht_diff = tmp->h - blt_sprite->h;

			blt_sprite = tmp;
			dst.y = dst.y - ht_diff;
		}

		if (bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
		{
			SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, bltfx->alpha);
		}
	}

	if (!blt_sprite)
	{
		return;
	}

	if (zoom && zoom != 100)
	{
		blt_sprite = zoomSurface(blt_sprite, zoom / 100.0, zoom / 100.0, options.zoom_smooth);

		if (!blt_sprite)
		{
			return;
		}
	}

	SDL_BlitSurface(blt_sprite, box, surface, &dst);

	if (bltfx && bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
	{
		SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
	}

	if (stretch || (zoom && zoom != 100))
	{
		SDL_FreeSurface(blt_sprite);
	}
}

/**
 * Get pixel from an SDL surface at specified X/Y position
 * @param surface SDL surface to get the pixel from.
 * @param x X position.
 * @param y Y position.
 * @return The pixel. */
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
	int bpp = surface->format->BytesPerPixel;
	/* The address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp)
	{
		case 1:
			return *p;

		case 2:
			return *(Uint16 *) p;

		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			{
				return p[0] << 16 | p[1] << 8 | p[2];
			}
			else
			{
				return p[0] | p[1] << 8 | p[2] << 16;
			}

		case 4:
			return *(Uint32 *) p;
	}

	return 0;
}

/**
 * Puts a pixel to specified X/Y position on SDL surface.
 * @param surface The surface.
 * @param x X position.
 * @param y Y position.
 * @param pixel Pixel to put. */
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	/* The address to the pixel we want to set */
	Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

	switch (bpp)
	{
		case 1:
			*p = pixel;
			break;

		case 2:
			*(Uint16 *) p = pixel;
			break;

		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
			{
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
			}
			else
			{
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}

			break;

		case 4:
			*(Uint32 *) p = pixel;
			break;
	}
}

/**
 * Create _Sprite::red surface for a sprite.
 * @param sprite Sprite. */
static void red_scale(_Sprite *sprite)
{
	int j, k;
	Uint8 r, g, b, a;
	SDL_Surface *temp = SDL_ConvertSurface(sprite->bitmap, FormatHolder->format, FormatHolder->flags);

	for (k = 0; k < temp->h; k++)
	{
		for (j = 0; j < temp->w; j++)
		{
			SDL_GetRGBA(getpixel(temp, j, k), temp->format, &r, &g, &b, &a);
			r = (int) (0.212671 * r + 0.715160 * g + 0.072169 * b);
			g = b = 0;
			putpixel(temp, j, k, SDL_MapRGBA(temp->format, r, g, b, a));
		}
	}

	sprite->red = SDL_DisplayFormatAlpha(temp);
	SDL_FreeSurface(temp);
}

/**
 * Create _Sprite::grey surface for a sprite.
 * @param sprite Sprite. */
static void grey_scale(_Sprite *sprite)
{
	int j, k;
	Uint8 r, g, b, a;
	SDL_Surface *temp = SDL_ConvertSurface(sprite->bitmap, FormatHolder->format, FormatHolder->flags);

	for (k = 0; k < temp->h; k++)
	{
		for (j = 0; j < temp->w; j++)
		{
			SDL_GetRGBA(getpixel(temp, j, k), temp->format, &r, &g, &b, &a);
			r = g = b = (int) (0.212671 * r + 0.715160 * g + 0.072169 * b);
			putpixel(temp, j, k, SDL_MapRGBA(temp->format, r, g, b, a));
		}
	}

	sprite->grey = SDL_DisplayFormatAlpha(temp);
	SDL_FreeSurface(temp);
}

/**
 * Create _Sprite::fow surface for a sprite.
 * @param sprite Sprite. */
static void fow_scale(_Sprite *sprite)
{
	int j, k;
	Uint8 r, g, b, a;
	SDL_Surface *temp = SDL_ConvertSurface(sprite->bitmap, FormatHolder->format, FormatHolder->flags);

	for (k = 0; k < temp->h; k++)
	{
		for (j = 0; j < temp->w; j++)
		{
			SDL_GetRGBA(getpixel(temp, j, k), temp->format, &r, &g, &b, &a);
			r = g = b = (int) ((0.212671 * r + 0.715160 * g + 0.072169 * b) * 0.34);
			b += 16;
			putpixel(temp, j, k, SDL_MapRGBA(temp->format, r, g, b, a));
		}
	}

	sprite->fog_of_war = SDL_DisplayFormatAlpha(temp);
	SDL_FreeSurface(temp);
}

/**
 * Get bitmap borders.
 * @param Surface Bitmap's surface.
 * @param[out] up Top border.
 * @param[out] down Bottom border.
 * @param[out] left Left border.
 * @param[out] right Right border.
 * @param ckey Color key.
 * @return 1 on success, 0 on failure. */
static int GetBitmapBorders(SDL_Surface *Surface, int *up, int *down, int *left, int *right, uint32 ckey)
{
	int x, y;

	*up = 0;
	*down = 0;
	*left = 0;
	*right = 0;

	/* Left side border */
	for (x = 0; x < Surface->w; x++)
	{
		for (y = 0; y < Surface->h; y++)
		{
			if (getpixel(Surface, x, y) != ckey)
			{
				*left = x;
				goto right_border;
			}
		}
	}

	/* We only need check this one time here - if we are here, the sprite is blank */
	return 0;

right_border:
	/* Right side border */
	for (x = Surface->w - 1; x >= 0; x--)
	{
		for (y = 0; y < Surface->h; y++)
		{
			if (getpixel(Surface, x, y) != ckey)
			{
				*right = (Surface->w - 1) - x;
				goto up_border;
			}
		}
	}

up_border:
	/* Up side border */
	for (y = 0; y < Surface->h; y++)
	{
		for (x = 0; x < Surface->w; x++)
		{
			if (getpixel(Surface, x, y) != ckey)
			{
				*up = y;
				goto down_border;
			}
		}
	}

down_border:
	/* Down side border */
	for (y = Surface->h - 1; y >= 0; y--)
	{
		for (x = 0; x < Surface->w; x++)
		{
			if (getpixel(Surface, x, y) != ckey)
			{
				*down = (Surface->h - 1) - y;
				return 1;
			}
		}
	}

	return 1;
}

/**
 * Blit a string.
 * @param surf Surface to blit the string on.
 * @param font Font used to blit the string.
 * @param text The string to blit.
 * @param x X position.
 * @param y Y position.
 * @param col Color.
 * @param area Area.
 * @param bltfx Bltfx.
 * @deprecated Use the new text API -- string_blt() and family. */
void StringBlt(SDL_Surface *surf, _Font *font, const char *text, int x, int y, int col, SDL_Rect *area, _BLTFX *bltfx)
{
	int i, tmp, line_clip = -1, line_count = 0;
	int gflag, colorToggle = 0, mode, color_real;
	SDL_Rect src, dst;
	SDL_Color color, color_g;

	if (area)
	{
		line_clip = area->w;
	}

	color_real = col & 0xff;
	mode = col;

	/* .w/h are not used from BlitSurface to draw */
	dst.x = x;
	dst.y = y;

	/* Dark brown text will have dark blue links */
	if (color_real == COLOR_DBROWN)
	{
		color_g.r = 72;
		color_g.g = 5;
		color_g.b = 195;
	}
	/* Otherwise light blue is used */
	else
	{
		color_g.r = 96;
		color_g.g = 160;
		color_g.b = 255;
	}

	color.r = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].r;
	color.g = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].g;
	color.b = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].b;

	SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color, 1, 1);

	if (bltfx && bltfx->flags & BLTFX_FLAG_SRCALPHA)
	{
		SDL_SetAlpha(font->sprite->bitmap, SDL_SRCALPHA, bltfx->alpha);
	}
	else
	{
		SDL_SetAlpha(font->sprite->bitmap, SDL_RLEACCEL, 255);
	}

	gflag = 0;

	if (mode & COLOR_FLAG_CLIPPED)
	{
		SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color_g, 1, 1);
		gflag = 1;
	}

	for (i = 0; text[i] != '\0'; i++)
	{
		/* Change text color */
		if (text[i] == '~' || text[i] == '|')
		{
			/* No highlighting in black text */
			if (color_real == COLOR_BLACK)
			{
				continue;
			}

			if (colorToggle)
			{
				color.r = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].r;
				color.g = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].g;
				color.b = Bitmaps[BITMAP_PALETTE]->bitmap->format->palette->colors[color_real].b;
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color, 1, 1);
			}
			else
			{
				/* Change color for dark brown (generally the book interface) */
				if (color_real == COLOR_DBROWN)
				{
					/* For | the color will be red */
					if (text[i] == '|')
					{
						color.r = 184;
						color.g = 6;
						color.b = 6;
					}
					/* Otherwise purple */
					else
					{
						color.r = 149;
						color.g = 24;
						color.b = 172;
					}
				}
				/* Otherwise green is used for ~, yellow for | */
				else
				{
					if (text[i] == '|')
					{
						color.r = 0xff;
					}
					else
					{
						color.r = 0x00;
					}

					color.g = 0xff;
					color.b = 0x00;
				}

				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color, 1, 1);
			}

			colorToggle = (colorToggle + 1) & 1;
			continue;
		}
		/* Link */
		else if (text[i] == '^')
		{
			if (gflag)
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color, 1, 1);
				gflag = 0;
			}
			else
			{
				SDL_SetPalette(font->sprite->bitmap, SDL_LOGPAL | SDL_PHYSPAL, &color_g, 1, 1);
				gflag = 1;
			}

			continue;
		}

		tmp = font->c[(int) (text[i])].w + font->char_offset;

		/* If set, we have a clipping line */
		if (line_clip >= 0)
		{
			if ((line_count += tmp) > line_clip)
			{
				return;
			}
		}

		if (text[i] != ' ')
		{
			src.x = font->c[(int) (text[i])].x;
			src.y = font->c[(int) (text[i])].y;
			src.w = font->c[(int) (text[i])].w;
			src.h = font->c[(int) (text[i])].h;
			SDL_BlitSurface(font->sprite->bitmap, &src, surf, &dst);
		}

		dst.x += tmp;
	}
}

/**
 * Initialize a new font.
 * @param sprite Sprite we're initializing the font from.
 * @param font The font.
 * @param xlen X len of one character.
 * @param ylen Y len of one character.
 * @param c32len Char offset. */
void CreateNewFont(_Sprite *sprite, _Font *font, int xlen, int ylen, int c32len)
{
	int i, y, flag;

	SDL_LockSurface(sprite->bitmap);
	font->sprite = sprite;

	for (i = 0; i < 256; i++)
	{
		font->c[i].x = (i % 32) * (xlen + 1) + 1;
		font->c[i].y = (i / 32) * (ylen + 1) + 1;
		font->c[i].h = ylen;
		font->c[i].w = xlen;
		flag = 0;

		/* Better no error in font bitmap... or this will lock up */
		while (1)
		{
			for (y = font->c[i].h - 1; y >= 0; y--)
			{
				if (getpixel(sprite->bitmap, font->c[i].x + font->c[i].w - 1, font->c[i].y + y))
				{
					flag = 1;
					break;
				}
			}

			if (flag)
				break;

			font->c[i].w--;
		}
	}

	SDL_UnlockSurface(sprite->bitmap);
	font->char_offset = c32len;
}

/**
 * Show a tooltip.
 * @param mx X position.
 * @param my Y position.
 * @param text Text for the tooltip. */
void show_tooltip(int mx, int my, char *text)
{
	SDL_Rect rec;
	char *tooltip = text;

	if (!options.show_tooltips)
	{
		return;
	}

	rec.w = 3;

	while (*text)
	{
		rec.w += SystemFont.c[(int) *text++].w + SystemFont.char_offset;
	}

	rec.x = mx + 9;
	rec.y = my + 17;
	rec.h = 12;

	if (rec.x + rec.w >= Screensize->x)
	{
		rec.x -= (rec.x + rec.w + 1) - Screensize->x;
	}

	SDL_FillRect(ScreenSurface, &rec, -1);
	StringBlt(ScreenSurface, &SystemFont, tooltip, rec.x + 2, rec.y - 1, COLOR_BLACK, NULL, NULL);
}

/**
 * Get the pixel length of a text.
 * @param text Text.
 * @param font Font used.
 * @return Length of the text. */
int get_string_pixel_length(const char *text, struct _Font *font)
{
	int i, len = 0;

	for (i = 0; text[i] != '\0'; i++)
	{
		if (text[i] == '^' || text[i] == '~' || text[i] == '|')
		{
			continue;
		}

		len += font->c[(int) text[i]].w + font->char_offset;
	}

	return len;
}

/**
 * Calculate the displayed width of the text.
 * @param font Font used to display the text
 * @param text The text to calculate width
 * @return The width in integer value */
int StringWidth(_Font *font, char *text)
{
	int w = 0, i;

	for (i = 0; text[i] != '\0'; i++)
	{
		switch (text[i])
		{
			case '~':
			case '|':
			case '^':
				break;

			default:
				w += font->c[(int) (text[i])].w + font->char_offset;
				break;
		}
	}

	return w;
}

/**
 * Calculate the displayed characters for a given width
 * @param font The font used for text
 * @param text The text
 * @param line Line
 * @param len Length
 * @return 1 if width will be more than length, 0 otherwise */
int StringWidthOffset(_Font *font, char *text, int *line, int len)
{
	int w = 0, i, c, flag = 0;

	for (c = i = 0; text[i] != '\0'; i++)
	{
		switch (text[i])
		{
			case '~':
			case '|':
			case '^':
				break;

			default:
				w += font->c[(int) (text[i])].w + font->char_offset;

				if (w >= len && !flag)
				{
					flag = 1;
					*line = c;
				}

				break;
		}

		c++;
	}

	/* Line is in limit */
	if (!flag)
	{
		*line = c;
	}

	return flag;
}

/**
 * Add an animation.
 * @param type
 * @param mapx
 * @param mapy
 * @param value
 * @return  */
struct _anim *add_anim(int type, int mapx, int mapy, int value)
{
	struct _anim *tmp, *anim;
	int num_ticks;

	for (tmp = start_anim; tmp; tmp = tmp->next)
	{
		if (!tmp->next)
		{
			break;
		}
	}

	/* tmp == null - no anim in que, else tmp = last anim */
	anim = (struct _anim *) malloc(sizeof(struct _anim));

	if (!tmp)
	{
		start_anim = anim;
	}
	else
	{
		tmp->next = anim;
	}

	anim->before = tmp;
	anim->next = NULL;

	anim->type = type;

	/* Starting X position */
	anim->x = 0;

	/* Starting Y position */
	anim->y = -5;
	anim->xoff = 0;

	/* This looks like it makes it move up the screen -- was 0 */
	anim->yoff = 1;

	/* Map cordinates */
	anim->mapx = mapx;
	anim->mapy = mapy;

	/* Amount of damage */
	anim->value = value;

	/* Current time in MilliSeconds */
	anim->start_tick = LastTick;

	switch (type)
	{
		case ANIM_DAMAGE:
			/* How many ticks to display */
			num_ticks = 850;
			anim->last_tick = anim->start_tick + num_ticks;
			/* 850 ticks 25 pixel move up */
			anim->yoff = (25.0f / 850.0f);
			break;

		case ANIM_KILL:
			/* How many ticks to display */
			num_ticks = 850;
			anim->last_tick = anim->start_tick + num_ticks;
			/* 850 ticks 25 pixel move up */
			anim->yoff = (25.0f / 850.0f);
			break;
	}

	return anim;
}

/**
 * Remove an animation.
 * @param anim The animation to remove. */
void remove_anim(struct _anim *anim)
{
	struct _anim *tmp, *tmp_next;
	void *tmp_free;

	if (!anim)
	{
		return;
	}

	tmp = anim->before;
	tmp_next = anim->next;
	tmp_free = &anim;
	/* free node memory */
	FreeMemory(tmp_free);

	if (tmp)
	{
		tmp->next = tmp_next;
	}
	else
	{
		start_anim = tmp_next;
	}

	if (tmp_next)
	{
		tmp_next->before = tmp;
	}
}

/**
 * Walk through the map anim list, and display the anims. */
void play_anims()
{
	struct _anim *anim, *tmp;
	int xpos, ypos, tmp_off;
	int num_ticks;
	char buf[32];
	int tmp_y;

	for (anim = start_anim; anim; anim = tmp)
	{
		tmp = anim->next;

		/* Have we passed the last tick */
		if (LastTick > anim->last_tick)
			remove_anim(anim);
		else
		{
			num_ticks = LastTick - anim->start_tick;

			switch (anim->type)
			{
				case ANIM_DAMAGE:
					tmp_y = anim->y - (int) ((float) num_ticks * anim->yoff);

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + options.map_size_x && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + options.map_size_y)
					{
						xpos = options.mapstart_x + (int) ((MAP_START_XOFF + (anim->mapx - MapData.posx) * MAP_TILE_YOFF - (anim->mapy - MapData.posy - 1) * MAP_TILE_YOFF - 4) * (options.zoom / 100.0));
						ypos = options.mapstart_y + (int) ((MAP_START_YOFF + (anim->mapx - MapData.posx) * MAP_TILE_XOFF + (anim->mapy - MapData.posy - 1) * MAP_TILE_XOFF - 34) * (options.zoom / 100.0));

						if (anim->value < 0)
						{
							snprintf(buf, sizeof(buf), "%d", abs(anim->value));
							string_blt(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 - (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1, COLOR_SIMPLE(COLOR_GREEN), TEXT_OUTLINE, NULL);
						}
						else
						{
							snprintf(buf, sizeof(buf), "%d", anim->value);
							string_blt(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 - (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1, COLOR_SIMPLE(COLOR_ORANGE), TEXT_OUTLINE, NULL);
						}
					}

					break;

				case ANIM_KILL:
					tmp_y = anim->y - (int) ((float) num_ticks * anim->yoff);

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + options.map_size_x && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + options.map_size_y)
					{
						xpos = options.mapstart_x + (int) ((MAP_START_XOFF + (anim->mapx - MapData.posx) * MAP_TILE_YOFF - (anim->mapy - MapData.posy - 1) * MAP_TILE_YOFF - 4) * (options.zoom / 100.0));
						ypos = options.mapstart_y + (int) ((MAP_START_YOFF + (anim->mapx - MapData.posx) * MAP_TILE_XOFF + (anim->mapy - MapData.posy - 1) * MAP_TILE_XOFF - 34) * (options.zoom / 100.0));

						sprite_blt(Bitmaps[BITMAP_DEATH], xpos + anim->x - 5, ypos + tmp_y - 4, NULL, NULL);
						snprintf(buf, sizeof(buf), "%d", anim->value);

						tmp_off = 0;

						/* Let's check the size of the value */
						if (anim->value < 10)
							tmp_off = 6;
						else if (anim->value < 100)
							tmp_off = 0;
						else if (anim->value < 1000)
							tmp_off = -6;
						else if (anim->value < 10000)
							tmp_off = -12;

						string_blt(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + tmp_off, ypos + tmp_y, COLOR_SIMPLE(COLOR_ORANGE), TEXT_OUTLINE, NULL);
					}

					break;

				default:
					LOG(llevBug, "Unknown animation type\n");
					break;
			}
		}
	}
}

/**
 * Check for possible sprite collision.
 *
 * Used to make the player overlapping objects transparent.
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param sprite1
 * @param sprite2
 * @return  */
int sprite_collision(int x1, int y1, int x2, int y2, _Sprite *sprite1, _Sprite *sprite2)
{
	int left1, left2;
	int right1, right2;
	int top1, top2;
	int bottom1, bottom2;

	left1 = x1 + sprite1->border_left;
	left2 = x2 + sprite2->border_left;

	right1 = x1 + sprite1->bitmap->w - sprite1->border_right;
	right2 = x2 + sprite2->bitmap->w - sprite2->border_right;

	top1 = y1 + sprite1->border_up;
	top2 = y2 + sprite2->border_down;

	bottom1 = y1 + sprite1->bitmap->h - sprite1->border_down;
	bottom2 = y2 + sprite2->bitmap->h - sprite2->border_down;

	if (bottom1 < top2)
	{
		return 0;
	}

	if (top1 > bottom2)
	{
		return 0;
	}

	if (right1 < left2)
	{
		return 0;
	}

	if (left1 > right2)
	{
		return 0;
	}

	return 1;
}

/**
 * Zoom an SDL surface.
 * @param src The surface to zoom.
 * @param zoomx How much to zoom X.
 * @param zoomy How much to zoom Y.
 * @param smooth Use smooth zoom?
 * @return Zoomed surface. */
SDL_Surface *zoomSurface(SDL_Surface* src, double zoomx, double zoomy, int smooth)
{
	SDL_Surface *rz_src, *rz_dst;
	int dstwidth, dstheight, is32bit;
	int i, src_converted, flipx, flipy;

	if (!src)
	{
		return NULL;
	}

	/* Determine if source surface is 32bit or 8bit */
	is32bit = (src->format->BitsPerPixel == 32);

	if (is32bit || src->format->BitsPerPixel == 8)
	{
		/* Use source surface 'as is' */
		rz_src = src;
		src_converted = 0;
	}
	else
	{
		/* New source surface is 32bit with a defined RGBA ordering */
		rz_src = SDL_CreateRGBSurface(SDL_SWSURFACE, src->w, src->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		SDL_BlitSurface(src, NULL, rz_src, NULL);
		src_converted = 1;
		is32bit = 1;
	}

	flipx = (zoomx < 0);

	if (flipx)
	{
		zoomx = -zoomx;
	}

	flipy = (zoomy < 0);

	if (flipy)
	{
		zoomy = -zoomy;
	}

	zoomSurfaceSize(rz_src->w, rz_src->h, zoomx, zoomy, &dstwidth, &dstheight);

	/* Alloc space to completely contain the zoomed surface */
	rz_dst = NULL;

	if (is32bit)
	{
		/* Target surface is 32bit with source RGBA/ABGR ordering */
		rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight, 32, rz_src->format->Rmask, rz_src->format->Gmask, rz_src->format->Bmask, rz_src->format->Amask);
	}
	else
	{
		/* Target surface is 8bit */
		rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight, 8, 0, 0, 0, 0);
	}

	SDL_LockSurface(rz_src);

	if (is32bit)
	{
		/* Call the 32bit transformation routine to do the zooming (using alpha) */
		zoomSurfaceRGBA(rz_src, rz_dst, flipx, flipy, smooth);
		SDL_SetAlpha(rz_dst, SDL_SRCALPHA, 255);
	}
	else
	{
		/* Copy palette and colorkey info */
		for (i = 0; i < rz_src->format->palette->ncolors; i++)
		{
			rz_dst->format->palette->colors[i] = rz_src->format->palette->colors[i];
		}

		rz_dst->format->palette->ncolors = rz_src->format->palette->ncolors;
		/* Call the 8bit transformation routine to do the zooming */
		zoomSurfaceY(rz_src, rz_dst);
		SDL_SetColorKey(rz_dst, SDL_SRCCOLORKEY | SDL_RLEACCEL, rz_src->format->colorkey);
	}

	SDL_UnlockSurface(rz_src);

	if (src_converted)
	{
		SDL_FreeSurface(rz_src);
	}

	return rz_dst;
}

/**
 * Calculate size for new zoomed surface.
 * @param width Source surface width.
 * @param height Source surface height.
 * @param zoomx Zoom X.
 * @param zoomy Zoom Y.
 * @param[out] dstwidth Will contain calculated width.
 * @param[out] dstheight  Will contain calculated height. */
void zoomSurfaceSize(int width, int height, double zoomx, double zoomy, int *dstwidth, int *dstheight)
{
	if (zoomx < VALUE_LIMIT)
	{
		zoomx = VALUE_LIMIT;
	}

	if (zoomy < VALUE_LIMIT)
	{
		zoomy = VALUE_LIMIT;
	}

	/* Calculate target size */
	*dstwidth = (int) ((double) width * zoomx);
	*dstheight = (int) ((double) height * zoomy);

	if (*dstwidth < 1)
	{
		*dstwidth = 1;
	}
	if (*dstheight < 1)
	{
		*dstheight = 1;
	}
}

/**
 * 8-bit surface zoom without smoothing.
 * @param src Source surface.
 * @param dst Destination surface. */
static void zoomSurfaceY(SDL_Surface *src, SDL_Surface *dst)
{
	Uint32 x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy;
	Uint8 *sp, *dp, *csp;
	int dgap;

	sx = (Uint32) (65536.0 * (float) src->w / (float) dst->w);
	sy = (Uint32) (65536.0 * (float) src->h / (float) dst->h);

	/* Allocate memory for row increments. */
	if (!(sax = (Uint32 *) malloc(dst->w * sizeof(Uint32))))
	{
		LOG(llevError, "zoomSurfaceY(): Out of memory.\n");
	}

	if (!(say = (Uint32 *) malloc(dst->h * sizeof(Uint32))))
	{
		LOG(llevError, "zoomSurfaceY(): Out of memory.\n");
	}

	/* Precalculate row increments */
	csx = 0;
	csax = sax;

	for (x = 0; (int) x < dst->w; x++)
	{
		csx += sx;
		*csax = (csx >> 16);
		csx &= 0xffff;
		csax++;
	}

	csy = 0;
	csay = say;

	for (y = 0; (int) y < dst->h; y++)
	{
		csy += sy;
		*csay = (csy >> 16);
		csy &= 0xffff;
		csay++;
	}

	csx = 0;
	csax = sax;

	for (x = 0; (int) x < dst->w; x++)
	{
		csx += (*csax);
		csax++;
	}

	csy = 0;
	csay = say;

	for (y = 0; (int) y < dst->h; y++)
	{
		csy += (*csay);
		csay++;
	}

	/* Pointer setup */
	sp = csp = (Uint8 *) src->pixels;
	dp = (Uint8 *) dst->pixels;
	dgap = dst->pitch - dst->w;

	/* Draw */
	csay = say;

	for (y = 0; (int) y < dst->h; y++)
	{
		csax = sax;
		sp = csp;

		for (x = 0; (int) x < dst->w; x++)
		{
			/* Draw */
			*dp = *sp;
			/* Advance source pointers */
			sp += (*csax);
			csax++;
			/* Advance destination pointer */
			dp++;
		}

		/* Advance source pointer (for row) */
		csp += ((*csay) * src->pitch);
		csay++;
		/* Advance destination pointers */
		dp += dgap;
	}

	free(sax);
	free(say);
}

/**
 * 32bit zoom with optional anti-aliasing by bilinear interpolation.
 * @param src Source surface.
 * @param dst Destination surface.
 * @param flipx Flip X?
 * @param flipy Flip Y?
 * @param smooth Use smooth zoom? */
static void zoomSurfaceRGBA(SDL_Surface * src, SDL_Surface * dst, int flipx, int flipy, int smooth)
{
	int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
	tColorRGBA *c00, *c01, *c10, *c11, *sp, *csp, *dp;
	int dgap;

	if (smooth)
	{
		/* For interpolation: assume source dimension is one pixel */
		sx = (int) (65536.0 * (float) (src->w - 1) / (float) dst->w);
		sy = (int) (65536.0 * (float) (src->h - 1) / (float) dst->h);
	}
	else
	{
		sx = (int) (65536.0 * (float) src->w / (float) dst->w);
		sy = (int) (65536.0 * (float) src->h / (float) dst->h);
	}

	if (!(sax = (int *) malloc((dst->w + 1) * sizeof(Uint32))))
	{
		LOG(llevError, "zoomSurfaceRGBA(): Out of memory.\n");
	}

	if (!(say = (int *) malloc((dst->h + 1) * sizeof(Uint32))))
	{
		LOG(llevError, "zoomSurfaceRGBA(): Out of memory.\n");
	}

	/* Precalculate row increments */
	sp = csp = (tColorRGBA *) src->pixels;
	dp = (tColorRGBA *) dst->pixels;

	if (flipx)
	{
		csp += (src->w-1);
	}

	if (flipy)
	{
		csp  = (tColorRGBA *) ((Uint8 *) csp + src->pitch * (src->h - 1));
	}

	csx = 0;
	csax = sax;

	for (x = 0; x <= dst->w; x++)
	{
		*csax = csx;
		csax++;
		csx &= 0xffff;
		csx += sx;
	}

	csy = 0;
	csay = say;

	for (y = 0; y <= dst->h; y++)
	{
		*csay = csy;
		csay++;
		csy &= 0xffff;
		csy += sy;
	}

	dgap = dst->pitch - dst->w * 4;

	/* Switch between interpolating and non-interpolating code */
	if (smooth)
	{
		/* Scan destination */
		csay = say;

		for (y = 0; y < dst->h; y++)
		{
			/* Setup color source pointers */
			c00 = csp;
			c01 = csp;
			c01++;
			c10 = (tColorRGBA *) ((Uint8 *) csp + src->pitch);
			c11 = c10;
			c11++;
			csax = sax;

			for (x = 0; x < dst->w; x++)
			{
				/* Interpolate colors */
				ex = (*csax & 0xffff);
				ey = (*csay & 0xffff);
				t1 = ((((c01->r - c00->r) * ex) >> 16) + c00->r) & 0xff;
				t2 = ((((c11->r - c10->r) * ex) >> 16) + c10->r) & 0xff;
				dp->r = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->g - c00->g) * ex) >> 16) + c00->g) & 0xff;
				t2 = ((((c11->g - c10->g) * ex) >> 16) + c10->g) & 0xff;
				dp->g = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->b - c00->b) * ex) >> 16) + c00->b) & 0xff;
				t2 = ((((c11->b - c10->b) * ex) >> 16) + c10->b) & 0xff;
				dp->b = (((t2 - t1) * ey) >> 16) + t1;
				t1 = ((((c01->a - c00->a) * ex) >> 16) + c00->a) & 0xff;
				t2 = ((((c11->a - c10->a) * ex) >> 16) + c10->a) & 0xff;
				dp->a = (((t2 - t1) * ey) >> 16) + t1;

				/* Advance source pointers */
				csax++;
				sstep = (*csax >> 16);
				c00 += sstep;
				c01 += sstep;
				c10 += sstep;
				c11 += sstep;
				/* Advance destination pointer */
				dp++;
			}

			/* Advance source pointer */
			csay++;
			csp = (tColorRGBA *) ((Uint8 *) csp + (*csay >> 16) * src->pitch);
			/* Advance destination pointers */
			dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
		}
	}
	else
	{
		csay = say;

		for (y = 0; y < dst->h; y++)
		{
			sp = csp;
			csax = sax;

			for (x = 0; x < dst->w; x++)
			{
				/* Draw */
				*dp = *sp;
				/* Advance source pointers */
				csax++;
				sstep = (*csax >> 16);

				if (flipx)
				{
					sstep = -sstep;
				}

				sp += sstep;
				/* Advance destination pointer */
				dp++;
			}

			/* Advance source pointer */
			csay++;
			sstep = (*csay >> 16) * src->pitch;

			if (flipy)
			{
				sstep = -sstep;
			}

			csp = (tColorRGBA *) ((Uint8 *) csp + sstep);
			/* Advance destination pointers */
			dp = (tColorRGBA *) ((Uint8 *) dp + dgap);
		}
	}

	/* Remove temp arrays */
	free(sax);
	free(say);
}

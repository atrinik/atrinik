/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*                     Copyright (C) 2009 Alex Tokar                     *
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

#include <include.h>

/** Darkness values */
static double dark_value[DARK_LEVELS] =
{
	1.0,
	0.828,
	0.685,
	0.542,
	0.399,
	0.256,
	0.113
};

/** Anim queue of current active map */
struct _anim *start_anim;

static int GetBitmapBorders(SDL_Surface *Surface, int *up, int *down, int *left, int *right, uint32 ckey);
static void grey_scale(SDL_Color *col_tab, SDL_Color *grey_tab, int num_col, int r, int g, int b);
static void red_scale(SDL_Color *col_tab, SDL_Color*grey_tab, int numcol, int rcol, int gcol, int bcol);
static void fow_scale(SDL_Color *col_tab, SDL_Color*grey_tab, int numcol, int rcol, int gcol, int bcol);

/**
 * Load sprite file.
 * @param fname Sprite filename
 * @param flags Flags for the sprite
 * @return NULL if failed, the sprite otherwise */
_Sprite *sprite_load_file(char *fname, uint32 flags)
{
	_Sprite *sprite;

	sprite = sprite_tryload_file(fname, flags, NULL);

	if (sprite == NULL)
	{
		LOG(LOG_ERROR, "ERROR: sprite_load_file(): Can't load sprite %s\n", fname);
		return NULL;
	}

	return sprite;
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
		*line = c;

	return flag;
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
	int i, s, ncol, dark_flag = 0;
	Uint8 r, g, b;
	uint32 ckflags, tmp = 0;
	SDL_Color colors[256], dark[256], ckey;

	if (fname)
	{
		if ((bitmap = IMG_Load_wrapper(fname)) == NULL)
			return NULL;
	}
	else
	{
		bitmap = IMG_LoadPNG_RW(rwop);
	}

	if ((sprite = malloc(sizeof(_Sprite))) == NULL)
		return NULL;

	memset(sprite, 0, sizeof(_Sprite));

	sprite->status = SPRITE_STATUS_LOADED;
	sprite->type = SPRITE_TYPE_NORMAL;

	/* Hm, must test this is used from displayformat too? */
	/* We set colorkey stuff. Depends on video hardware and how we need to store this */
	ckflags = SDL_SRCCOLORKEY | SDL_ANYFORMAT;

	if (options.rleaccel_flag)
		ckflags |= SDL_RLEACCEL;

	if (bitmap->format->palette)
		SDL_SetColorKey(bitmap, ckflags, (tmp = bitmap->format->colorkey));
	/* We force a true color png to colorkey. Default colkey is black (0). */
	else if (flag & SURFACE_FLAG_COLKEY_16M)
		SDL_SetColorKey(bitmap, ckflags, 0);

	GetBitmapBorders(bitmap, &sprite->border_up, &sprite->border_down, &sprite->border_left, &sprite->border_right, tmp);

#if 0
	LOG(0, "BORDERS %s -> U:%d D:%d L:%d R:%d\n", fname, sprite->border_up, sprite->border_down, sprite->border_left, sprite->border_right);
#endif

	/* We store our original bitmap */
	sprite->bitmap = bitmap;

	if (!(flag & SURFACE_FLAG_PALETTE))
	{
		/* We have a palette and we want store the 4 brightness level? */
		if (bitmap->format->palette)
		{
			/* Don't forget to map the default dark[0] */
			dark_flag = 1;

			/* First, we grab our color key and numbers of colors */
			SDL_GetRGB(bitmap->format->colorkey, bitmap->format, &ckey.r, &ckey.g, &ckey.b);
			ncol = bitmap->format->palette->ncolors;

			/* And save the original palette.
			 * Perhaps we have use in later improvements */
			for (i = 0; i < bitmap->format->palette->ncolors; i++)
			{
				colors[i].r = bitmap->format->palette->colors[i].r;
				colors[i].g = bitmap->format->palette->colors[i].g;
				colors[i].b = bitmap->format->palette->colors[i].b;
			}

			/* We want 7 + 1 dark level. Level 0 is our original brightness,
			 * level 6 is darkest - level 7 is "total dark" = no light */

			/* 1-6 darkened versions */
			for (s = 1; s < DARK_LEVELS; s++)
			{
				/* We adjust the colors. */
				for (i = 0; i < bitmap->format->palette->ncolors; i++)
				{
					/* First: grab RGB of this entry */
					r = colors[i].r;
					g = colors[i].g;
					b = colors[i].b;

					/* Second: if this is not the color key, adjust the color */
					if (r != ckey.r || g != ckey.g || b != ckey.b)
					{
						r = (Uint8) ((double) r * dark_value[s]);
						g = (Uint8) ((double) g * dark_value[s]);
						b = (Uint8) ((double) b * dark_value[s]);
					}

					/* Store color information in our new palette */
					dark[i].r = r;
					dark[i].g = g;
					dark[i].b = b;
				}

				SDL_SetColors(bitmap, dark, 0, ncol);
				sprite->dark_level[s] = SDL_DisplayFormat(bitmap);
			}

			grey_scale(colors, dark, ncol, ckey.r, ckey.g, ckey.b);
			SDL_SetColors(bitmap, dark, 0, ncol);
			sprite->grey = SDL_DisplayFormat(bitmap);

			red_scale(colors, dark, ncol, ckey.r, ckey.g, ckey.b);
			SDL_SetColors(bitmap, dark, 0, ncol);
			sprite->red = SDL_DisplayFormat(bitmap);

			fow_scale(colors, dark, ncol, ckey.r, ckey.g, ckey.b);
			SDL_SetColors(bitmap, dark, 0, ncol);
			sprite->fog_of_war = SDL_DisplayFormat(bitmap);

			SDL_SetColors(bitmap, colors, 0, ncol);
		}

		/* Map original color over default dark */
		if (dark_flag)
			sprite->dark_level[0] = sprite->bitmap;
	}

	if (flag & SURFACE_FLAG_DISPLAYFORMAT)
	{
		sprite->bitmap = SDL_DisplayFormat(bitmap);
		SDL_FreeSurface(bitmap);
	}

	return sprite;
}

static void red_scale(SDL_Color *col_tab, SDL_Color *grey_tab, int numcol, int rcol, int gcol, int bcol)
{
	int i;
	double r, g, b;

	for (i = 0; i < numcol; i++)
	{
		r = (double) col_tab[i].r;
		g = (double) col_tab[i].g;
		b = (double) col_tab[i].b;

		if (r != rcol || g != gcol || b != bcol )
		{
			r = (0.212671 * r + 0.715160 * g + 0.072169 * b) + 32;
			g = b = 0;
		}

		grey_tab[i].r = (int) r;
		grey_tab[i].g = (int) g;
		grey_tab[i].b = (int) b;
	}
}

static void fow_scale(SDL_Color *col_tab, SDL_Color *grey_tab, int numcol, int rcol, int gcol, int bcol)
{
	int i;
	double r, g, b;

	for (i = 0; i < numcol; i++)
	{
		r = (double) col_tab[i].r;
		g = (double) col_tab[i].g;
		b = (double) col_tab[i].b;

		if (r != rcol || g != gcol || b != bcol)
		{
			g = 0.212671 * r + 0.715160 * g + 0.072169 * b;
			r = (g * 0.34);
			/* It's a try... */
			b = (g * 0.34) + 16;
			g = (g * 0.34);
		}

		grey_tab[i].r = (int) r;
		grey_tab[i].g = (int) g;
		grey_tab[i].b = (int) b;
	}
}

static void grey_scale(SDL_Color *col_tab, SDL_Color *grey_tab, int numcol, int rcol, int gcol, int bcol)
{
	int i;
	double r, g, b;

	for (i = 0; i < numcol;i++)
	{
		r = (double) col_tab[i].r;
		g = (double) col_tab[i].g;
		b = (double) col_tab[i].b;

		if (r != rcol || g != gcol || b != bcol)
		{
			r = b = g = 0.212671 * r + 0.715160 * g + 0.072169 * b;
		}

		grey_tab[i].r = (int) r;
		grey_tab[i].g = (int) g;
		grey_tab[i].b = (int) b;
	}
}

void sprite_free_sprite(_Sprite *sprite)
{
	void *tmp_free;
	int i;

	if (!sprite)
		return;

	if (sprite->bitmap)
		SDL_FreeSurface(sprite->bitmap);

	if (sprite->grey)
		SDL_FreeSurface(sprite->grey);

	if (sprite->red)
		SDL_FreeSurface(sprite->red);

	if (sprite->fog_of_war)
		SDL_FreeSurface(sprite->fog_of_war);

	if (sprite->dark_level)
	{
		for (i = 1; i < DARK_LEVELS; i++)
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

/* Init this font structure with gfx data from sprite bitmap */
void CreateNewFont(_Sprite *sprite, _Font *font, int xlen, int ylen, int c32len)
{
	int i, y;
	int flag;

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
				if (GetSurfacePixel(sprite->bitmap, font->c[i].x + font->c[i].w - 1, font->c[i].y + y))
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

void StringBlt(SDL_Surface *surf, _Font *font, char *text, int x, int y, int col, SDL_Rect *area, _BLTFX *bltfx)
{
	int i, tmp, line_clip = -1, line_count = 0;
	int gflag, colorToggle = 0, mode, color_real;
	SDL_Rect src, dst;
	SDL_Color color, color_g;

	if (area)
		line_clip = area->w;

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
		SDL_SetAlpha(font->sprite->bitmap, SDL_SRCALPHA, bltfx->alpha);
	else
		SDL_SetAlpha(font->sprite->bitmap, SDL_RLEACCEL, 255);

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
				continue;

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
						color.r = 0xff;
					else
						color.r = 0x00;

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
				return;
		}

		if (text[i] != 32)
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
 * Show tooltip.
 * @param mx Mouse X position
 * @param my Mouse Y position
 * @param text Text for the tooltip */
void show_tooltip(int mx, int my, char *text)
{
	SDL_Rect rec;
	char *tooltip = text;

	if (!options.show_tooltips)
		return;

	rec.w = 3;

	while (*text)
		rec.w += SystemFont.c[(int) *text++].w + SystemFont.char_offset;

	rec.x = mx + 9;
	rec.y = my + 17;
	rec.h = 12;

	if (rec.x + rec.w >= Screensize->x)
		rec.x -= (rec.x + rec.w + 1) - Screensize->x;

	SDL_FillRect(ScreenSurface, &rec, -1);
	StringBlt(ScreenSurface, &SystemFont, tooltip, rec.x + 2, rec.y - 1, COLOR_BLACK, NULL, NULL);
}

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
			if (GetSurfacePixel(Surface, x, y) != ckey)
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
			if (GetSurfacePixel(Surface, x, y) != ckey)
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
			if (GetSurfacePixel(Surface, x, y) != ckey)
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
			if (GetSurfacePixel(Surface, x, y) != ckey)
			{
				*down = (Surface->h - 1) - y;
				return 1;
			}
		}
	}

	return 1;
}

/* Grabs a pixel from a SDL_Surface on the position x, y in the right format and colors */
Uint32 GetSurfacePixel(SDL_Surface *Surface, Sint32 X, Sint32 Y)
{
	Uint8  *bits;
	Uint32 Bpp;

	Bpp = Surface->format->BytesPerPixel;
	bits = ((Uint8 *)Surface->pixels) + Y * Surface->pitch + X * Bpp;

	/* Get the pixel */
	switch (Bpp)
	{
		case 1:
			return *((Uint8 *)Surface->pixels + Y * Surface->pitch + X);

		case 2:
			return *((Uint16 *)Surface->pixels + Y * Surface->pitch/2 + X);

		case 3:
		{
			/* Format/endian independent*/
			Uint8 r, g, b;

			r = *((bits) + Surface->format->Rshift / 8);
			g = *((bits) + Surface->format->Gshift / 8);
			b = *((bits) + Surface->format->Bshift / 8);
			return SDL_MapRGB(Surface->format, r, g, b);
		}

		case 4:
			return *((Uint32 *) Surface->pixels + Y * Surface->pitch / 4 + X);
	}

	return -1;
}

int get_string_pixel_length(char *text, struct _Font *font)
{
	int i, len = 0;

	for (i = 0; text[i] != 0;i++)
		len += font->c[(int) text[i]].w + font->char_offset;

	return len;
}

void sprite_blt(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx)
{
	SDL_Rect dst;
	SDL_Surface *surface, *blt_sprite;

	if (!sprite)
		return;

	blt_sprite = sprite->bitmap;

	if (bltfx && (bltfx->surface))
		surface = bltfx->surface;
	else
		surface = ScreenSurface;

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
		return;

	if (box)
		SDL_BlitSurface(blt_sprite, box, surface, &dst);
	else
		SDL_BlitSurface(blt_sprite, NULL, surface, &dst);

	if (bltfx && bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
	{
		SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
	}
}

/* This function supports the whole BLTFX flags, and is only used to blit the map! */
void sprite_blt_map(_Sprite *sprite, int x, int y, SDL_Rect *box, _BLTFX *bltfx)
{
	SDL_Rect dst;
	SDL_Surface *surface, *blt_sprite;

	if (!sprite)
		return;

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
				return;

			blt_sprite = sprite->dark_level[bltfx->dark_level];
		}
		else if (bltfx->flags & BLTFX_FLAG_FOW)
			blt_sprite = sprite->fog_of_war;
		else if (bltfx->flags & BLTFX_FLAG_RED)
			blt_sprite = sprite->red;
		else if (bltfx->flags & BLTFX_FLAG_GREY)
			blt_sprite = sprite->grey;

		if (!blt_sprite)
			return;

		if (bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
		{
			SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, bltfx->alpha);
		}
	}

	if (!blt_sprite)
		return;

	if (box)
		SDL_BlitSurface(blt_sprite, box, surface, &dst);
	else
		SDL_BlitSurface(blt_sprite, NULL,surface, &dst);

	if (bltfx && bltfx->flags & BLTFX_FLAG_SRCALPHA && !(ScreenSurface->flags & SDL_HWSURFACE))
	{
		SDL_SetAlpha(blt_sprite, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
	}
}

struct _anim *add_anim(int type, int mapx, int mapy, int value)
{
	struct _anim *tmp, *anim;
	int num_ticks;

	for (tmp = start_anim; tmp; tmp = tmp->next)
	{
		if (!tmp->next)
			break;
	}

	/* tmp == null - no anim in que, else tmp = last anim */
	anim = (struct _anim *)malloc(sizeof(struct _anim));

	if (!tmp)
		start_anim = anim;
	else
		tmp->next = anim;

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

void remove_anim(struct _anim *anim)
{
	struct _anim *tmp, *tmp_next;
	void *tmp_free;

	if (!anim)
		return;

	tmp = anim->before;
	tmp_next = anim->next;
	tmp_free = &anim;
	/* free node memory */
	FreeMemory(tmp_free);

	if (tmp)
		tmp->next = tmp_next;
	else
		start_anim = tmp_next;

	if (tmp_next)
		tmp_next->before = tmp;
}

void delete_anim_que()
{
	struct _anim *tmp, *tmp_next;
	void *tmp_free;

	for (tmp = start_anim; tmp; )
	{
		tmp_next = tmp->next;
		tmp_free = &tmp;
		/* free node memory */
		FreeMemory(tmp_free);
		tmp = tmp_next;
	}

	start_anim = NULL;
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

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + MapStatusX && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + MapStatusY)
					{
						xpos = options.mapstart_x + MAP_START_XOFF+(anim->mapx-MapData.posx)*MAP_TILE_YOFF-(anim->mapy-MapData.posy-1)*MAP_TILE_YOFF-4;
						ypos = options.mapstart_y + MAP_START_YOFF+(anim->mapx-MapData.posx)*MAP_TILE_XOFF+(anim->mapy-MapData.posy-1)*MAP_TILE_XOFF-34;

						if (anim->value < 0)
						{
							snprintf(buf, sizeof(buf), "%d", abs(anim->value));
							StringBlt(ScreenSurface, &SystemFontOut, buf, xpos + anim->x + 4 - (strlen(buf) * 4), ypos + tmp_y, COLOR_GREEN, NULL, NULL);
						}
						else if (xpos == 396 && ypos == 289)
						{
							snprintf(buf, sizeof(buf), "%d", anim->value);
							StringBlt(ScreenSurface, &SystemFontOut, buf, xpos + anim->x + 4 - strlen(buf) * 4, ypos + tmp_y, COLOR_RED, NULL, NULL);
						}
						else
						{
							snprintf(buf, sizeof(buf), "%d", anim->value);
							StringBlt(ScreenSurface, &SystemFontOut, buf, xpos + anim->x + 4 - strlen(buf) * 4, ypos + tmp_y, COLOR_ORANGE, NULL, NULL);
						}
					}

					break;

				case ANIM_KILL:
					tmp_y = anim->y - (int) ((float) num_ticks * anim->yoff);

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + MapStatusX && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + MapStatusY)
					{
						xpos = options.mapstart_x + MAP_START_XOFF + (anim->mapx-MapData.posx) * MAP_TILE_YOFF - (anim->mapy-MapData.posy - 1) * MAP_TILE_YOFF - 4;
						ypos = options.mapstart_y + MAP_START_YOFF + (anim->mapx-MapData.posx) * MAP_TILE_XOFF + (anim->mapy-MapData.posy - 1) * MAP_TILE_XOFF - 26;

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

						StringBlt(ScreenSurface, &SystemFontOut, buf, xpos + anim->x + tmp_off, ypos + tmp_y, COLOR_ORANGE, NULL, NULL);
					}

					break;

				default:
					LOG(LOG_ERROR, "WARNING: Unknown animation type\n");
					break;
			}
		}
	}
}

/* A very special collision for the multi tile face & the player sprite,
 * used to make the player overlapping objects transparent */
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
		return 0;

	if (top1 > bottom2)
		return 0;

	if (right1 < left2)
		return 0;

	if (left1 > right2)
		return 0;

	return 1;
}

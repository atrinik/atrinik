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
 * Sprite related functions. */

#include <global.h>

/** Anim queue of current active map */
struct _anim *start_anim;

/** Format holder for red_scale(), fow_scale() and grey_scale() functions. */
SDL_Surface *FormatHolder;

/** Darkness alpha values. */
static int dark_alpha[DARK_LEVELS] =
{
	0, 44, 80, 117, 153, 190, 226
};

static void red_scale(sprite_struct *sprite);
static void grey_scale(sprite_struct *sprite);
static void fow_scale(sprite_struct *sprite);

/**
 * Initialize the sprite system. */
void sprite_init_system(void)
{
	FormatHolder = SDL_CreateRGBSurface(SDL_SRCALPHA, 1, 1, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	SDL_SetAlpha(FormatHolder, SDL_SRCALPHA, 255);
}

/**
 * Load sprite file.
 * @param fname Sprite filename.
 * @param flags Flags for the sprite.
 * @return NULL if failed, the sprite otherwise. */
sprite_struct *sprite_load_file(char *fname, uint32 flags)
{
	sprite_struct *sprite = sprite_tryload_file(fname, flags, NULL);

	if (sprite == NULL)
	{
		logger_print(LOG(BUG), "Can't load sprite %s", fname);
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
sprite_struct *sprite_tryload_file(char *fname, uint32 flag, SDL_RWops *rwop)
{
	sprite_struct *sprite;
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

	if (!(sprite = malloc(sizeof(sprite_struct))))
	{
		return NULL;
	}

	memset(sprite, 0, sizeof(sprite_struct));

	ckflags = SDL_SRCCOLORKEY | SDL_ANYFORMAT | SDL_RLEACCEL;

	if (bitmap->format->palette)
	{
		SDL_SetColorKey(bitmap, ckflags, (tmp = bitmap->format->colorkey));
	}
	/* We force a true color png to colorkey. Default colkey is black (0). */
	else if (flag & SURFACE_FLAG_COLKEY_16M)
	{
		SDL_SetColorKey(bitmap, ckflags, 0);
	}

	surface_borders_get(bitmap, &sprite->border_up, &sprite->border_down, &sprite->border_left, &sprite->border_right, tmp);

	/* We store our original bitmap */
	sprite->bitmap = bitmap;

	if (flag & SURFACE_FLAG_DISPLAYFORMATALPHA)
	{
		sprite->bitmap = SDL_DisplayFormatAlpha(bitmap);
		SDL_FreeSurface(bitmap);
	}
	else if (flag & SURFACE_FLAG_DISPLAYFORMAT)
	{
		sprite->bitmap = SDL_DisplayFormat(bitmap);
		SDL_FreeSurface(bitmap);
	}

	return sprite;
}

/**
 * Free a sprite.
 * @param sprite Sprite to free. */
void sprite_free_sprite(sprite_struct *sprite)
{
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

	if (sprite->effect)
	{
		SDL_FreeSurface(sprite->effect);
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

	free(sprite);
}

void surface_show(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src)
{
	SDL_Rect dstrect;

	dstrect.x = x;
	dstrect.y = y;

	SDL_BlitSurface(src, srcrect, surface, &dstrect);
}

void surface_show_effects(SDL_Surface *surface, int x, int y, SDL_Rect *srcrect, SDL_Surface *src, uint8 alpha, uint32 stretch, sint16 zoom_x, sint16 zoom_y, sint16 rotate)
{
	int smooth;

	if (stretch)
	{
		SDL_Surface *tmp;
		Uint8 n = (stretch >> 24) & 0xFF;
		Uint8 e = (stretch >> 16) & 0xFF;
		Uint8 w = (stretch >> 8) & 0xFF;
		Uint8 s = stretch & 0xFF;

		tmp = tile_stretch(src, n, e, s, w);

		if (!tmp)
		{
			return;
		}

		y -= tmp->h - src->h;
		src = tmp;
	}

	/* If this is just a flip with no rotate, force disabled interpolation. */
	if (!rotate && (zoom_x == 0 || zoom_x == -100 || zoom_x == 100) && (zoom_y == 0 || zoom_y == -100 || zoom_y == 100))
	{
		smooth = 0;
	}
	else
	{
		smooth = setting_get_int(OPT_CAT_CLIENT, OPT_ZOOM_SMOOTH);
	}

	if (rotate)
	{
		src = rotozoomSurfaceXY(src, rotate, zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, smooth);

		if (!src)
		{
			return;
		}
	}
	else if ((zoom_x && zoom_x != 100) || (zoom_y && zoom_y != 100))
	{
		src = zoomSurface(src, zoom_x ? zoom_x / 100.0 : 1.0, zoom_y ? zoom_y / 100.0 : 1.0, smooth);

		if (!src)
		{
			return;
		}
	}

	if (alpha)
	{
		SDL_SetAlpha(src, SDL_SRCALPHA, alpha);
	}

	surface_show(surface, x, y, srcrect, src);

	if (alpha)
	{
		SDL_SetAlpha(src, SDL_SRCALPHA, SDL_ALPHA_OPAQUE);
	}

	if (stretch || (zoom_x && zoom_x != 100) || (zoom_y && zoom_y != 100) || rotate)
	{
		SDL_FreeSurface(src);
	}
}

void map_sprite_show(sprite_struct *sprite, int x, int y, uint32 flags, uint8 dark_level, uint8 alpha, uint32 stretch, sint16 zoom_x, sint16 zoom_y, sint16 rotate)
{
	SDL_Surface *surface;

	if (!sprite)
	{
		return;
	}

	surface = sprite->bitmap;

	/* Is there an effect overlay active? */
	if (effect_has_overlay())
	{
		/* There is one, so add an overlay to the image if there isn't
		 * one yet. */
		if (!sprite->effect)
		{
			effect_scale(sprite);
		}

		surface = sprite->effect;
	}
	/* No overlay, but the image was previously overlayed; need to
	 * free the dark surfaces so they can be re-rendered, without the
	 * overlay. */
	else if (sprite->effect)
	{
		uint8 i;

		SDL_FreeSurface(sprite->effect);
		sprite->effect = NULL;

		for (i = 0; i < DARK_LEVELS; i++)
		{
			if (sprite->dark_level[i])
			{
				SDL_FreeSurface(sprite->dark_level[i]);
				sprite->dark_level[i] = NULL;
			}
		}
	}

	if (flags & SPRITE_FLAG_DARK)
	{
		/* Last dark level is "no color" */
		if (dark_level == DARK_LEVELS)
		{
			return;
		}

		if (sprite->dark_level[dark_level])
		{
			surface = sprite->dark_level[dark_level];
		}
		else
		{
			surface = SDL_DisplayFormatAlpha(surface);
			filledRectAlpha(surface, 0, 0, surface->w - 1, surface->h - 1, dark_alpha[dark_level]);
			sprite->dark_level[dark_level] = surface;
		}
	}
	else if (flags & SPRITE_FLAG_FOW)
	{
		if (!sprite->fog_of_war)
		{
			fow_scale(sprite);
		}

		surface = sprite->fog_of_war;
	}
	else if (flags & SPRITE_FLAG_RED)
	{
		if (!sprite->red)
		{
			red_scale(sprite);
		}

		surface = sprite->red;
	}
	else if (flags & SPRITE_FLAG_GRAY)
	{
		if (!sprite->grey)
		{
			grey_scale(sprite);
		}

		surface = sprite->grey;
	}

	surface_show_effects(cur_widget[MAP_ID]->widgetSF, x, y, NULL, surface, alpha, stretch, zoom_x, zoom_y, rotate);
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
 * Create sprite_struct::red surface for a sprite.
 * @param sprite Sprite. */
static void red_scale(sprite_struct *sprite)
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
 * Create sprite_struct::grey surface for a sprite.
 * @param sprite Sprite. */
static void grey_scale(sprite_struct *sprite)
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
 * Create sprite_struct::fow surface for a sprite.
 * @param sprite Sprite. */
static void fow_scale(sprite_struct *sprite)
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
 * Calculate the left border in the surface - this is the position of
 * the first pixel from the left that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_left(SDL_Surface *surface, int *pos, uint32 ckey)
{
	int x, y;

	for (x = 0; x < surface->w; x++)
	{
		for (y = 0; y < surface->h; y++)
		{
			if (getpixel(surface, x, y) != ckey)
			{
				*pos = x;
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Calculate the right border in the surface - this is the position of
 * the first pixel from the right that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_right(SDL_Surface *surface, int *pos, uint32 ckey)
{
	int x, y;

	for (x = surface->w - 1; x >= 0; x--)
	{
		for (y = 0; y < surface->h; y++)
		{
			if (getpixel(surface, x, y) != ckey)
			{
				*pos = (surface->w - 1) - x;
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Calculate the top border in the surface - this is the position of
 * the first pixel from the top that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_top(SDL_Surface *surface, int *pos, uint32 ckey)
{
	int x, y;

	for (y = 0; y < surface->h; y++)
	{
		for (x = 0; x < surface->w; x++)
		{
			if (getpixel(surface, x, y) != ckey)
			{
				*pos = y;
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Calculate the bottom border in the surface - this is the position of
 * the first pixel from the bottom that that does not match 'color'.
 * @param surface Surface.
 * @param[out] pos Where to store the position.
 * @param color Color to check for.
 * @return 1 if the border was found, 0 otherwise. */
static int surface_border_get_bottom(SDL_Surface *surface, int *pos, uint32 color)
{
	int x, y;

	for (y = surface->h - 1; y >= 0; y--)
	{
		for (x = 0; x < surface->w; x++)
		{
			if (getpixel(surface, x, y) != color)
			{
				*pos = (surface->h - 1) - y;
				return 1;
			}
		}
	}

	return 0;
}

/**
 * Get borders from SDL_surface. The borders indicate the first pixel
 * from the border's side that does not match 'color'.
 * @param surface Surface to get borders from.
 * @param[out] top Where to store the top border.
 * @param[out] bottom Where to store the bottom border.
 * @param[out] left Where to store the left border.
 * @param[out] right Where to store the right border.
 * @param color Color to check for.
 * @return 1 if the borders were found, 0 otherwise (image is all filled
 * with 'color' color). */
int surface_borders_get(SDL_Surface *surface, int *top, int *bottom, int *left, int *right, uint32 color)
{
	*top = 0;
	*bottom = 0;
	*left = 0;
	*right = 0;

	/* If the border was not found, it means the surface is completely
	 * filled with 'color' color. */
	if (!surface_border_get_top(surface, top, color))
	{
		return 0;
	}

	surface_border_get_bottom(surface, bottom, color);
	surface_border_get_left(surface, left, color);
	surface_border_get_right(surface, right, color);

	return 1;
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

	/* Map coordinates */
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

	if (!anim)
	{
		return;
	}

	tmp = anim->before;
	tmp_next = anim->next;
	free(anim);

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
void play_anims(void)
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

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
					{
						xpos = cur_widget[MAP_ID]->x1 + (int) ((MAP_START_XOFF + (anim->mapx - MapData.posx) * MAP_TILE_YOFF - (anim->mapy - MapData.posy - 1) * MAP_TILE_YOFF - 4) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0));
						ypos = cur_widget[MAP_ID]->y1 + (int) ((MAP_START_YOFF + (anim->mapx - MapData.posx) * MAP_TILE_XOFF + (anim->mapy - MapData.posy - 1) * MAP_TILE_XOFF - 34) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0));

						if (anim->value < 0)
						{
							snprintf(buf, sizeof(buf), "%d", abs(anim->value));
							string_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 - (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1, COLOR_GREEN, TEXT_OUTLINE, NULL);
						}
						else
						{
							snprintf(buf, sizeof(buf), "%d", anim->value);
							string_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + 4 - (int) strlen(buf) * 4 + 1, ypos + tmp_y + 1, COLOR_ORANGE, TEXT_OUTLINE, NULL);
						}
					}

					break;

				case ANIM_KILL:
					tmp_y = anim->y - (int) ((float) num_ticks * anim->yoff);

					if (anim->mapx >= MapData.posx && anim->mapx < MapData.posx + setting_get_int(OPT_CAT_MAP, OPT_MAP_WIDTH) && anim->mapy >= MapData.posy && anim->mapy < MapData.posy + setting_get_int(OPT_CAT_MAP, OPT_MAP_HEIGHT))
					{
						xpos = cur_widget[MAP_ID]->x1 + (int) ((MAP_START_XOFF + (anim->mapx - MapData.posx) * MAP_TILE_YOFF - (anim->mapy - MapData.posy - 1) * MAP_TILE_YOFF - 4) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0));
						ypos = cur_widget[MAP_ID]->y1 + (int) ((MAP_START_YOFF + (anim->mapx - MapData.posx) * MAP_TILE_XOFF + (anim->mapy - MapData.posy - 1) * MAP_TILE_XOFF - 34) * (setting_get_int(OPT_CAT_MAP, OPT_MAP_ZOOM) / 100.0));

						surface_show(ScreenSurface, xpos + anim->x - 5, ypos + tmp_y - 4, NULL, TEXTURE_CLIENT("death"));
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

						string_show(ScreenSurface, FONT_MONO10, buf, xpos + anim->x + tmp_off, ypos + tmp_y, COLOR_ORANGE, TEXT_OUTLINE, NULL);
					}

					break;

				default:
					logger_print(LOG(BUG), "Unknown animation type");
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
int sprite_collision(int x, int y, int x2, int y2, sprite_struct *sprite1, sprite_struct *sprite2)
{
	int left1, left2;
	int right1, right2;
	int top1, top2;
	int bottom1, bottom2;

	left1 = x + sprite1->border_left;
	left2 = x2 + sprite2->border_left;

	right1 = x + sprite1->bitmap->w - sprite1->border_right;
	right2 = x2 + sprite2->bitmap->w - sprite2->border_right;

	top1 = y + sprite1->border_up;
	top2 = y2 + sprite2->border_down;

	bottom1 = y + sprite1->bitmap->h - sprite1->border_down;
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
 * Pans the surface.
 * @param surface Surface.
 * @param box Coordinates. */
void surface_pan(SDL_Surface *surface, SDL_Rect *box)
{
	if (box->x >= surface->w - box->w)
	{
		box->x = (Sint16) (surface->w - box->w);
	}

	if (box->x < 0)
	{
		box->x = 0;
	}

	if (box->y >= surface->h - box->h)
	{
		box->y = (Sint16) (surface->h - box->h);
	}

	if (box->y < 0)
	{
		box->y = 0;
	}
}

/**
 * Draw a single frame.
 * @param surface Surface to draw on.
 * @param x X position.
 * @param y Y position.
 * @param w Width of the frame.
 * @param h Height of the frame. */
void draw_frame(SDL_Surface *surface, int x, int y, int w, int h)
{
	SDL_Rect box;

	box.x = x;
	box.y = y;
	box.h = h;
	box.w = 1;
	SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
	box.x = x + w;
	box.h++;
	SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
	box.x = x;
	box.y+= h;
	box.w = w;
	box.h = 1;
	SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x60, 0x60, 0x60));
	box.x++;
	box.y = y;
	SDL_FillRect(surface, &box, SDL_MapRGB(surface->format, 0x55, 0x55, 0x55));
}

/**
 * Create a border around the specified coordinates.
 * @param surface Surface to use.
 * @param x X start of the border.
 * @param y Y start of the border.
 * @param w Maximum border width.
 * @param h Maximum border height.
 * @param color Color to use for the border.
 * @param size Border's size. */
void border_create(SDL_Surface *surface, int x, int y, int w, int h, int color, int size)
{
	SDL_Rect box;

	/* Left border. */
	box.x = x;
	box.y = y;
	box.h = h;
	box.w = size;
	SDL_FillRect(surface, &box, color);

	/* Right border. */
	box.x = x + w - size;
	SDL_FillRect(surface, &box, color);

	/* Top border. */
	box.x = x + size;
	box.y = y;
	box.w = w - size * 2;
	box.h = size;
	SDL_FillRect(surface, &box, color);

	/* Bottom border. */
	box.y = y + h - size;
	SDL_FillRect(surface, &box, color);
}

void border_create_line(SDL_Surface *surface, int x, int y, int w, int h, uint32 color)
{
	SDL_Rect dst;

	dst.x = x;
	dst.y = y;
	dst.w = w;
	dst.h = h;
	SDL_FillRect(surface, &dst, color);
}

void border_create_sdl_color(SDL_Surface *surface, SDL_Rect *coords, SDL_Color *color)
{
	uint32 color_mapped;

	color_mapped = SDL_MapRGB(surface->format, color->r, color->g, color->b);

	BORDER_CREATE_TOP(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, 1);
	BORDER_CREATE_BOTTOM(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, 1);
	BORDER_CREATE_LEFT(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, 1);
	BORDER_CREATE_RIGHT(surface, coords->x, coords->y, coords->w, coords->h, color_mapped, 1);
}

void border_create_color(SDL_Surface *surface, SDL_Rect *coords, const char *color_notation)
{
	SDL_Color color;

	if (!text_color_parse(color_notation, &color))
	{
		logger_print(LOG(BUG), "Invalid color: %s", color_notation);
		return;
	}

	border_create_sdl_color(surface, coords, &color);
}

void rectangle_create(SDL_Surface *surface, int x, int y, int w, int h, const char *color_notation)
{
	SDL_Color color;

	if (!text_color_parse(color_notation, &color))
	{
		logger_print(LOG(BUG), "Invalid color: %s", color_notation);
		return;
	}

	border_create_line(surface, x, y, w, h, SDL_MapRGB(surface->format, color.r, color.g, color.b));
}

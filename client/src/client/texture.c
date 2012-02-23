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
 * Texture management.
 *
 * @author Alex Tokar */

#include <global.h>

static texture_struct *textures[TEXTURE_TYPE_NUM];

static void texture_data_free(int type, texture_struct *tmp)
{
	if (type == TEXTURE_TYPE_SOFTWARE || type == TEXTURE_TYPE_CLIENT)
	{
		if (tmp->data.surface)
		{
			SDL_FreeSurface(tmp->data.surface);
		}
	}
}

static int texture_data_new(int type, texture_struct *tmp)
{
	if (type == TEXTURE_TYPE_SOFTWARE)
	{
		SDL_Surface *surface;

		if (strcmp(tmp->name, TEXTURE_FALLBACK_NAME) == 0)
		{
			SDL_Rect box;

			surface = SDL_CreateRGBSurface(get_video_flags(), 20, 20, video_get_bpp(), 0, 0, 0, 0);
			lineRGBA(surface, 0, 0, surface->w, surface->h, 255, 0, 0, 255);
			lineRGBA(surface, surface->w, 0, 0, surface->h, 255, 0, 0, 255);
			box.x = 0;
			box.y = 0;
			box.w = surface->w;
			box.h = surface->h;
			border_create_color(surface, &box, "950000");
		}
		else if (strncmp(tmp->name, "rectangle:", 10) == 0)
		{
			int w, h;

			if (sscanf(tmp->name + 10, "%d,%d", &w, &h) == 2)
			{
				surface = SDL_CreateRGBSurface(get_video_flags(), w, h, video_get_bpp(), 0, 0, 0, 0);
			}
			else
			{
				logger_print(LOG(BUG), "Invalid parameters for rectangle texture: %s", tmp->name);
				return 0;
			}
		}
		else
		{
			logger_print(LOG(BUG), "Invalid name for software texture: %s", tmp->name);
			return 0;
		}

		texture_data_free(type, tmp);
		tmp->data.surface = SDL_DisplayFormatAlpha(surface);
		SDL_FreeSurface(surface);
	}
	else if (type == TEXTURE_TYPE_CLIENT)
	{
		char path[HUGE_BUF];
		SDL_Surface *surface;

		snprintf(path, sizeof(path), "textures/%s.png", tmp->name);
		surface = IMG_Load_wrapper(path);

		if (!surface)
		{
			logger_print(LOG(BUG), "Could not load texture (%d): %s", type, path);
			return 0;
		}

		SDL_SetColorKey(surface, SDL_SRCCOLORKEY, surface->format->colorkey);
		texture_data_free(type, tmp);
		tmp->data.surface = SDL_DisplayFormatAlpha(surface);
		SDL_FreeSurface(surface);
	}

	return 1;
}

static void texture_free(int type, texture_struct *tmp)
{
	free(tmp->name);
	texture_data_free(type, tmp);
	free(tmp);
}

static texture_struct *texture_new(int type, const char *name)
{
	texture_struct *tmp;

	tmp = calloc(1, sizeof(*tmp));
	tmp->name = strdup(name);

	if (!texture_data_new(type, tmp))
	{
		texture_free(type, tmp);
		return NULL;
	}

	HASH_ADD_KEYPTR(hh, textures[type], tmp->name, strlen(tmp->name), tmp);

	return tmp;
}

void texture_init(void)
{
	int i;

	for (i = 0; i < TEXTURE_TYPE_NUM; i++)
	{
		textures[i] = NULL;
	}

	texture_new(TEXTURE_TYPE_SOFTWARE, TEXTURE_FALLBACK_NAME);
}

void texture_deinit(void)
{
	int i;
	texture_struct *curr, *tmp;

	for (i = 0; i < TEXTURE_TYPE_NUM; i++)
	{
		HASH_ITER(hh, textures[i], curr, tmp)
		{
			HASH_DEL(textures[i], curr);
			texture_free(i, curr);
		}
	}
}

void texture_clear_cache(void)
{
	int i;
	texture_struct *curr, *tmp;

	for (i = 0; i < TEXTURE_TYPE_NUM; i++)
	{
		HASH_ITER(hh, textures[i], curr, tmp)
		{
			texture_data_new(i, tmp);
		}
	}
}

texture_struct *texture_get(int type, const char *name)
{
	texture_struct *tmp;

	HASH_FIND_STR(textures[type], name, tmp);

	if (!tmp)
	{
		tmp = texture_new(type, name);

		if (!tmp)
		{
			tmp = texture_get(TEXTURE_TYPE_SOFTWARE, TEXTURE_FALLBACK_NAME);
		}
	}

	return tmp;
}

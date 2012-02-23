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
 * Texture system header file. */

#ifndef TEXTURE_H
#define TEXTURE_H

enum
{
	TEXTURE_TYPE_SOFTWARE,
	TEXTURE_TYPE_CLIENT,

	TEXTURE_TYPE_NUM
};

typedef struct texture_struct
{
	char *name;

	union
	{
		SDL_Surface *surface;
	} data;

	UT_hash_handle hh;
} texture_struct;

#define TEXTURE_FALLBACK_NAME "texture_fallback"

#define TEXTURE_SURFACE(_texture) ((_texture)->data.surface)
#define TEXTURE_CLIENT(_name) (TEXTURE_SURFACE(texture_get(TEXTURE_TYPE_CLIENT, (_name))))

#endif

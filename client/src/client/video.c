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
 * Video-related code. */

#include <include.h>

/**
 * Sets the screen surface to a new size, after updating ::Screensize.
 * @return 1 on success, 0 on failure. */
int video_set_size()
{
	uint32 videoflags;
	SDL_Surface *new;

	videoflags = get_video_flags();
	/* Try to set the video mode. */
	new = SDL_SetVideoMode(Screensize->x, Screensize->y, options.used_video_bpp, videoflags);

	/* Failed, try the default resolution. */
	if (!new)
	{
		new = SDL_SetVideoMode(WINDOW_DEFAULT_WIDTH, WINDOW_DEFAULT_HEIGHT, options.used_video_bpp, videoflags);
	}

	if (new)
	{
		const SDL_VideoInfo *info = SDL_GetVideoInfo();

		options.real_video_bpp = info->vfmt->BitsPerPixel;
		ScreenSurface = new;
		return 1;
	}

	return 0;
}

/**
 * Calculate the video flags from the settings.
 * When settings are changed at runtime, this MUST be called again.
 * @return The flags */
uint32 get_video_flags()
{
	uint32 videoflags_full, videoflags_win;

	videoflags_full = SDL_FULLSCREEN;

	if (options.Full_DOUBLEBUF)
		videoflags_full |= SDL_DOUBLEBUF;

	if (options.Full_HWSURFACE)
		videoflags_full |= SDL_HWSURFACE;

	if (options.Full_SWSURFACE)
		videoflags_full |= SDL_SWSURFACE;

	if (options.Full_HWACCEL)
		videoflags_full |= SDL_HWACCEL;

	if (options.Full_ANYFORMAT)
		videoflags_full |= SDL_ANYFORMAT;

	if (options.Full_ASYNCBLIT)
		videoflags_full |= SDL_ASYNCBLIT;

	if (options.Full_HWPALETTE)
		videoflags_full |= SDL_HWPALETTE;

	if (options.Full_RESIZABLE)
		videoflags_full |= SDL_RESIZABLE;

	if (options.Full_NOFRAME)
		videoflags_full |= SDL_NOFRAME;

	videoflags_win = 0;

	if (options.Win_DOUBLEBUF)
		videoflags_win |= SDL_DOUBLEBUF;

	if (options.Win_HWSURFACE)
		videoflags_win |= SDL_HWSURFACE;

	if (options.Win_SWSURFACE)
		videoflags_win |= SDL_SWSURFACE;

	if (options.Win_HWACCEL)
		videoflags_win |= SDL_HWACCEL;

	if (options.Win_ANYFORMAT)
		videoflags_win |= SDL_ANYFORMAT;

	if (options.Win_ASYNCBLIT)
		videoflags_win |= SDL_ASYNCBLIT;

	if (options.Win_HWPALETTE)
		videoflags_win |= SDL_HWPALETTE;

	if (options.Win_RESIZABLE)
		videoflags_win |= SDL_RESIZABLE;

	if (options.Win_NOFRAME)
		videoflags_win |= SDL_NOFRAME;

	options.videoflags_win = videoflags_win;
	options.videoflags_full = videoflags_full;

	if (options.fullscreen)
	{
		options.fullscreen_flag = 1;
		options.doublebuf_flag = 0;
		options.rleaccel_flag = 0;

		if (options.Full_RLEACCEL)
			options.rleaccel_flag = 1;

		if (options.videoflags_full & SDL_DOUBLEBUF)
			options.doublebuf_flag = 1;

		return videoflags_full;
	}
	else
	{
		options.fullscreen_flag = 0;
		options.doublebuf_flag = 0;
		options.rleaccel_flag = 0;

		if (options.Win_RLEACCEL)
			options.rleaccel_flag = 1;

		if (options.videoflags_win & SDL_DOUBLEBUF)
			options.doublebuf_flag = 1;

		return videoflags_win;
	}
}

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
* the Free Software Foundation; either version 3 of the License, or     *
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

/**
 * @file wrapper.c
 * General convenience functions for the client. */

#if defined( __WIN_32)  || defined(__LINUX)
FILE *logstream;
int logFlush;
#endif

/**
 * Logs an error, debug output, etc.
 * @param logLevel Level of the log message (LOG_MSG, LOG_DEBUG, ...)
 * @param format Formatting of the message, like sprintf
 * @param ... Additional arguments for format */
void LOG (int logLevel, char *format, ...)
{
#if defined( __WIN_32)  || defined(__LINUX)
	int flag = 0;
	va_list ap;

	/* We want log exactly ONE logLevel*/
	if (LOGLEVEL < 0)
	{
		if (LOGLEVEL * (-1) == logLevel)
			flag = 1;
	}
	/* We log all logLevel < LOGLEVEL*/
	else
	{
		if (logLevel <= LOGLEVEL)
			flag = 1;
	}

	/* Secure: we have no open stream */
	if (!logstream)
		flag = 0;

	va_start(ap, format);

	if (flag)
	{
		vfprintf(stdout, format, ap);
		vfprintf(logstream, format, ap);
	}
	/* No logstream, use stdout */
	else
		vfprintf(stdout, format, ap);

	va_end(ap);

	if (logstream)
		fflush(logstream);
#endif
}


/**
 * Start the base system, setting caption name and window icon.
 * @return Always returns 1. */
int SYSTEM_Start()
{
    SDL_Surface *icon;
    char buf[256];

    snprintf(buf, sizeof(buf), "%s%s", GetBitmapDirectory(), CLIENT_ICON_NAME);

    if ((icon = IMG_Load(buf)) != NULL)
        SDL_WM_SetIcon(icon, 0);

    SDL_WM_SetCaption(PACKAGE_NAME, PACKAGE_NAME);

#if defined( __WIN_32)  || defined(__LINUX)
	logstream  = fopen(LOG_FILE, "w");
#endif

	return 1;
}

/**
 * End the system. Currently does nothing, but could be used in the future.
 * @return Always returns 1. */
int SYSTEM_End()
{
	return 1;
}

/**
 * Get bitmap directory.
 * @return The bitmap directory */
char *GetBitmapDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
	return "./bitmaps/";
#endif
}

/**
 * Get the icon directory.
 * @return The icon directory */
char *GetIconDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
    return "./icons/";
#endif
}

/**
 * Get the sfx directory.
 * @return The sfx directory */
char *GetSfxDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
	return "./sfx/";
#endif
}

/**
 * Get the cache directory.
 * @return The cache directory */
char *GetCacheDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
	return "./cache/";
#endif
}

/**
 * Get the user defined GFX directory.
 * @return The user defined GFX directory */
char *GetGfxUserDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
	return "./gfx_user/";
#endif
}

/**
 * Get the media directory
 * @return The media directory */
char *GetMediaDirectory()
{
#if defined( __WIN_32)  || defined(__LINUX)
    return "./media/";
#endif
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

/* This seems to be lacking on some systems */
#if defined(HAVE_STRNICMP)
#else
#if !defined(HAVE_STRNCASECMP)
int strncasecmp(char *s1, char *s2, int n)
{
	register int c1, c2;

	while (*s1 && *s2 && n)
	{
		c1 = tolower(*s1);
		c2 = tolower(*s2);

		if (c1 != c2)
			return (c1 - c2);

		s1++;
		s2++;
		n--;
	}

	if (!n)
		return 0;

	return (int) (*s1 - *s2);
}
#endif
#endif

#if defined(HAVE_STRICMP)
#else
#if !defined(HAVE_STRCASECMP)
int strcasecmp(char *s1, char*s2)
{
	register int c1, c2;

	while (*s1 && *s2)
	{
		c1 = tolower(*s1);
		c2 = tolower(*s2);

		if (c1 != c2)
			return (c1 - c2);

		s1++;
		s2++;
	}

	if (*s1 == '\0' && *s2 == '\0')
		return 0;

	return (int) (*s1 - *s2);
}
#endif
#endif

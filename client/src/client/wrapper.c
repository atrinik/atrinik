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

/**
 * @file
 * General convenience functions for the client. */

#include <include.h>

FILE   *logstream;

int logFlush;

/**
 * Logs an error, debug output, etc.
 * @param logLevel Level of the log message (LOG_MSG, LOG_DEBUG, ...)
 * @param format Formatting of the message, like sprintf
 * @param ... Additional arguments for format */
void LOG(int logLevel, char *format, ...)
{
	int flag = 0;
	va_list ap;

	/* we want log exactly ONE logLevel */
	if (LOGLEVEL < 0)
	{
		if (LOGLEVEL * (-1) == logLevel)
			flag = 1;
	}
	/* we log all logLevel < LOGLEVEL */
	else
	{
		if (logLevel <= LOGLEVEL)
			flag = 1;
	}

	/* secure: we have no open stream */
	if (!logstream)
	{
		logstream = fopen_wrapper(LOG_FILE, "w");

		if (!logstream)
			flag = 0;
	}

	if (flag)
	{
		va_start(ap, format);
		vfprintf(stdout, format, ap);
		va_end(ap);
		va_start(ap, format);
		vfprintf(logstream, format, ap);
		va_end(ap);
	}

	fflush(logstream);
}

/**
 * Start the base system, setting caption name and window icon.
 * @return Always returns 1. */
void SYSTEM_Start()
{
	SDL_Surface *icon;
	char buf[256];

	snprintf(buf, sizeof(buf), "%s%s", GetBitmapDirectory(), CLIENT_ICON_NAME);

	if ((icon = IMG_Load_wrapper(buf)) != NULL)
	{
		SDL_WM_SetIcon(icon, 0);
	}

	SDL_WM_SetCaption(PACKAGE_NAME, PACKAGE_NAME);

	free(icon);

	logstream = fopen_wrapper(LOG_FILE, "w");
}

/**
 * End the system.
 * @return Always returns 1. */
int SYSTEM_End()
{
	free_help_files();
	SDL_Quit();
	return 1;
}

/**
 * Get bitmap directory.
 * @return The bitmap directory */
char *GetBitmapDirectory()
{
	return "bitmaps/";
}

/**
 * Get the icon directory.
 * @return The icon directory */
char *GetIconDirectory()
{
	return "icons/";
}

/**
 * Get the sfx directory.
 * @return The sfx directory */
char *GetSfxDirectory()
{
	return "sfx/";
}

/**
 * Get the cache directory.
 * @return The cache directory */
char *GetCacheDirectory()
{
	return "cache/";
}

/**
 * Get the user defined GFX directory.
 * @return The user defined GFX directory */
char *GetGfxUserDirectory()
{
	return "gfx_user/";
}

/**
 * Get the media directory
 * @return The media directory */
char *GetMediaDirectory()
{
	return "media/";
}

/**
 * Get a single word from a string, free from left and right whitespace.
 * @param str The string pointer
 * @param pos The position pointer
 * @return The word, or NULL if there is no word left */
char *get_word_from_string(char *str, int *pos)
{
	static char buf[HUGE_BUF];
	int i = 0;

	buf[0] = '\0';

	while (*(str + (*pos)) != '\0' && (!isalnum(*(str + (*pos))) && !isalpha(*(str + (*pos)))))
	{
		(*pos)++;
	}

	/* Nothing left */
	if (*(str + (*pos)) == '\0')
	{
		return NULL;
	}

	/* Copy until end of string or whitespace */
	while (*(str + (*pos)) != '\0' && (isalnum(*(str + (*pos))) || isalpha(*(str + (*pos)))))
	{
		buf[i++] = *(str + (*pos)++);
	}

	buf[i] = '\0';

	return buf;
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

#ifdef WIN32
char *file_path(const char *fname, const char *mode)
{
	static char tmp[256];

	(void) mode;

	snprintf(tmp, sizeof(tmp), "%s%s", SYSPATH, fname);

	return tmp;
}
#else

/**
 * Recursively creates directories from path.
 *
 * Used by file_path().
 * @param path The path
 * @return 0 on success, -1 otherwise */
static int mkdir_recurse(const char *path)
{
	char *copy, *p;

	p = copy = strdup(path);

	do
	{
		p = strchr(p + 1, '/');

		if (p)
		{
			*p = '\0';
		}

		if (access(copy, F_OK) == -1)
		{
			if (mkdir(copy, 0755) == -1)
			{
				free(p);
				free(copy);
				return -1;
			}
		}

		if (p)
		{
			*p = '/';
		}
	}
	while (p);

	free(p);
	free(copy);

	return 0;
}

/**
 * Get path to file, to implement saving settings related data to user's
 * home directory on GNU/Linux. For Win32, this function is just used to
 * prefix the path to the file with "./".
 * @param fname The file path
 * @param mode File mode
 * @return The path to the file. */
char *file_path(const char *fname, const char *mode)
{
	static char tmp[256];
	char *stmp, ctmp;

	snprintf(tmp, sizeof(tmp), "%s/.atrinik/%s", getenv("HOME"), fname);

	if (strchr(mode, 'w'))
	{
		if ((stmp = strrchr(tmp, '/')))
		{
			ctmp = stmp[0];
			stmp[0] = 0;
			mkdir_recurse(tmp);
			stmp[0] = ctmp;
		}
	}
	else if (strchr(mode, '+') || strchr(mode, 'a'))
	{
		if (access(tmp, W_OK))
		{
			char otmp[256], shtmp[517];

			snprintf(otmp, sizeof(otmp), "%s%s", SYSPATH, fname);

			if ((stmp = strrchr(tmp, '/')))
			{
				ctmp = stmp[0];
				stmp[0] = 0;
				mkdir_recurse(tmp);
				stmp[0] = ctmp;
			}

			/* Copy base file to home directory */
			snprintf(shtmp, sizeof(shtmp), "cp %s %s", otmp, tmp);
			if (system(shtmp))
			{
			}
		}
	}
	else
	{
		if (access(tmp, R_OK))
		{
            sprintf(tmp, "%s%s", SYSPATH, fname);
		}
	}

	return tmp;
}
#endif

/**
 * @defgroup file_wrapper_functions File wrapper functions
 * These functions are used as replacement to common C and SDL functions
 * that are related to file opening and reading/writing.
 *
 * For GNU/Linux, they call file_path() to determine the path to the file
 * to open in ~/.atrinik, and if the file doesn't exist there, copy it
 * there from the directory the client is running in.
 *@{*/

/**
 * fopen wrapper.
 * @param fname The file name
 * @param mode File mode
 * @return Return value of fopen().  */
FILE *fopen_wrapper(const char *fname, const char *mode)
{
    return fopen(file_path(fname, mode), mode);
}

/**
 * IMG_Load wrapper.
 * @param file The file name
 * @return Return value of IMG_Load().  */
SDL_Surface *IMG_Load_wrapper(const char *file)
{
    return IMG_Load(file_path(file, "r"));
}

#ifdef INSTALL_SOUND

/**
 * Mix_LoadWAV wrapper.
 * @param fname The file name
 * @return Return value of Mix_LoadWAV().  */
Mix_Chunk *Mix_LoadWAV_wrapper(const char *fname)
{
    return Mix_LoadWAV(file_path(fname, "r"));
}

/**
 * Mix_LoadMUS wrapper.
 * @param file The file name
 * @return Return value of Mix_LoadMUS().  */
Mix_Music *Mix_LoadMUS_wrapper(const char *file)
{
    return Mix_LoadMUS(file_path(file, "r"));
}
#endif
/*@}*/

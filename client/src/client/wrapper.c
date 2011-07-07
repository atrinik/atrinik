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
 * General convenience functions for the client. */

#include <include.h>

static FILE *logstream = NULL;

/**
 * Human-readable names of log levels. */
static const char *const loglevel_names[] =
{
	"[Error] ",
	"[Bug]   ",
	"[Debug] ",
	"[Info]  "
};

/**
 * Logs an error, debug output, etc.
 * @param logLevel Level of the log message (llevInfo, llevDebug, ...)
 * @param format Formatting of the message, like sprintf
 * @param ... Additional arguments for format */
void LOG(LogLevel logLevel, char *format, ...)
{
	va_list ap;
	char buf[HUGE_BUF * 4];

	if (!logstream)
	{
		logstream = fopen_wrapper(LOG_FILE, "w");
	}

	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	fputs(loglevel_names[logLevel], stdout);
	fputs(buf, stdout);

	if (logstream)
	{
		fputs(loglevel_names[logLevel], logstream);
		fputs(buf, logstream);
		fflush(logstream);
	}

	if (logLevel == llevError)
	{
		LOG(llevInfo, "\nFatal error encountered. Exiting...\n");
		system_end();
		abort();
		exit(-1);
	}
}

/**
 * Start the base system, setting caption name and window icon. */
void system_start()
{
	SDL_Surface *icon;

	icon = IMG_Load_wrapper(DIRECTORY_BITMAPS"/"CLIENT_ICON_NAME);

	if (icon)
	{
		SDL_WM_SetIcon(icon, 0);
		SDL_FreeSurface(icon);
	}

	SDL_WM_SetCaption(PACKAGE_NAME, PACKAGE_NAME);

	logstream = fopen_wrapper(LOG_FILE, "w");
}

/**
 * End the system. */
void system_end()
{
	list_remove_all();
	script_killall();
	save_interface_file();
	kill_widgets();
	curl_deinit();
	socket_deinitialize();
	sound_deinit();
	free_bitmaps();
	text_deinit();
	free_help_files();
	effects_deinit();
	settings_deinit();
	SDL_Quit();
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

	free(copy);

	return 0;
}

/**
 * Copy a file.
 * @param filename Source file.
 * @param filename_out Destination file. */
void copy_file(const char *filename, const char *filename_out)
{
	FILE *fp, *fp_out;
	char buf[HUGE_BUF];

	fp = fopen(filename, "r");

	if (!fp)
	{
		LOG(llevBug, "copy_file(): Failed to open '%s' for reading.\n", filename);
		return;
	}

	fp_out = fopen(filename_out, "w");

	if (!fp_out)
	{
		LOG(llevBug, "copy_file(): Failed to open '%s' for writing.\n", filename_out);
		fclose(fp);
		return;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		fputs(buf, fp_out);
	}

	fclose(fp);
	fclose(fp_out);
}

/**
 * Get configuration directory.
 * @return The configuration directory. */
const char *get_config_dir()
{
	const char *desc;

#ifdef LINUX
	desc = getenv("HOME");
#else
	desc = getenv("APPDATA");
#endif

	/* Failed to find an usable destination, so store it in the
	 * current directory. */
	if (!desc || !*desc)
	{
		desc = ".";
	}

	return desc;
}

/**
 * Get path to file, to implement saving settings related data to user's
 * home directory.
 * @param fname The file path.
 * @param mode File mode.
 * @return The path to the file. */
char *file_path(const char *fname, const char *mode)
{
	static char tmp[HUGE_BUF];
	char *stmp, ctmp, *data_dir;

	snprintf(tmp, sizeof(tmp), "%s/.atrinik/"PACKAGE_VERSION"/%s", get_config_dir(), fname);

	if (strchr(mode, 'w'))
	{
		if ((stmp = strrchr(tmp, '/')))
		{
			ctmp = stmp[0];
			stmp[0] = '\0';
			mkdir_recurse(tmp);
			stmp[0] = ctmp;
		}
	}
	else if (strchr(mode, '+') || strchr(mode, 'a'))
	{
		if (access(tmp, W_OK))
		{
			char otmp[HUGE_BUF];

#ifdef WIN32
			data_dir = SYSPATH;
#else
			data_dir = br_find_data_dir(SYSPATH);
#endif
			snprintf(otmp, sizeof(otmp), "%s%s", data_dir, fname);
#ifndef WIN32
			free(data_dir);
#endif

			if ((stmp = strrchr(tmp, '/')))
			{
				ctmp = stmp[0];
				stmp[0] = '\0';
				mkdir_recurse(tmp);
				stmp[0] = ctmp;
			}

			copy_file(otmp, tmp);
		}
	}
	else
	{
		if (access(tmp, R_OK))
		{
#ifdef WIN32
			data_dir = SYSPATH;
#else
			data_dir = br_find_data_dir(SYSPATH);
#endif
			snprintf(tmp, sizeof(tmp), "%s%s", data_dir, fname);
#ifndef WIN32
			free(data_dir);
#endif
		}
	}

	return tmp;
}

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
/*@}*/

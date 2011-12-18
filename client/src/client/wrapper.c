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

#include <global.h>

static FILE *logstream = NULL;

/**
 * Human-readable names of log levels. */
static const char *const loglevel_names[] =
{
	"[System] ",
	"[Error]  ",
	"[Bug]    ",
	"[Debug]  ",
	"[Info]   "
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
void system_start(void)
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
void system_end(void)
{
	notification_destroy();
	popup_destroy_all();
	save_interface_file();
	kill_widgets();
	curl_deinit();
	socket_deinitialize();
	effects_deinit();
	sound_deinit();
	free_bitmaps();
	text_deinit();
	hfiles_deinit();
	settings_deinit();
	keybind_deinit();
	packet_deinit();
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
 * Use mkdir_recurse() to ensure that destination 'path' exists, creating
 * sub-directories of the path, if they do not exist yet. If you're using
 * this to ensure directory path exists, make sure to end the 'path'
 * string with a forward slash, otherwise the function will assume that
 * the path is a file path.
 * @param path The path to ensure. */
void mkdir_ensure(const char *path)
{
	char *stmp;

	stmp = strrchr(path, '/');

	if (stmp)
	{
		char ctmp;

		ctmp = stmp[0];
		stmp[0] = '\0';
		mkdir_recurse(path);
		stmp[0] = ctmp;
	}
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

	mkdir_ensure(filename_out);

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
 * Copy a file/directory if it exists.
 * @param from Directory where to copy from.
 * @param to Directort to copy to.
 * @param src File/directory to copy.
 * @param dst Where to copy the file/directory to. */
void copy_if_exists(const char *from, const char *to, const char *src, const char *dst)
{
	char src_path[HUGE_BUF], dst_path[HUGE_BUF];

	snprintf(src_path, sizeof(src_path), "%s/%s", from, src);
	snprintf(dst_path, sizeof(dst_path), "%s/%s", to, dst);

	if (access(src_path, R_OK) == 0)
	{
		copy_rec(src_path, dst_path);
	}
}

/**
 * Recursively remove a directory and its contents.
 *
 * Effectively same as 'rf -rf path'.
 * @param path What to remove. */
void rmrf(const char *path)
{
	DIR *dir;
	struct dirent *currentfile;
	char buf[HUGE_BUF];
	struct stat st;

	dir = opendir(path);

	if (!dir)
	{
		return;
	}

	while ((currentfile = readdir(dir)))
	{
		if (!strcmp(currentfile->d_name, ".") || !strcmp(currentfile->d_name, ".."))
		{
			continue;
		}

		snprintf(buf, sizeof(buf), "%s/%s", path, currentfile->d_name);

		if (stat(buf, &st) != 0)
		{
			continue;
		}

		if (S_ISDIR(st.st_mode))
		{
			rmrf(buf);
		}
		else if (S_ISREG(st.st_mode))
		{
			unlink(buf);
		}
	}

	closedir(dir);
	rmdir(path);
}

/**
 * Recursively copy a file or directory.
 * @param src Source file/directory to copy.
 * @param dst Where to copy to. */
void copy_rec(const char *src, const char *dst)
{
	struct stat st;

	/* Does it exist? */
	if (stat(src, &st) != 0)
	{
		return;
	}

	/* Copy directory contents. */
	if (S_ISDIR(st.st_mode))
	{
		DIR *dir;
		struct dirent *currentfile;
		char dir_src[HUGE_BUF], dir_dst[HUGE_BUF];

		dir = opendir(src);

		if (!dir)
		{
			return;
		}

		/* Try to make the new directory. */
		if (access(dst, R_OK) != 0)
		{
			mkdir(dst, 0755);
		}

		while ((currentfile = readdir(dir)))
		{
			if (currentfile->d_name[0] == '.')
			{
				continue;
			}

			snprintf(dir_src, sizeof(dir_src), "%s/%s", src, currentfile->d_name);
			snprintf(dir_dst, sizeof(dir_dst), "%s/%s", dst, currentfile->d_name);
			copy_rec(dir_src, dir_dst);
		}

		closedir(dir);
	}
	/* Copy file. */
	else
	{
		copy_file(src, dst);
	}
}

/**
 * Get configuration directory.
 * @return The configuration directory. */
const char *get_config_dir(void)
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
 * Get path to a file in the data directory.
 * @param buf Buffer where to store the path.
 * @param len Size of buf.
 * @param fname File. */
void get_data_dir_file(char *buf, size_t len, const char *fname)
{
	/* Try the current directory first. */
	snprintf(buf, len, "./%s", fname);

#ifdef INSTALL_SUBDIR_SHARE
	/* Not found, try the share directory since it was defined... */
	if (access(buf, R_OK))
	{
		char *prefix;

		/* Get the prefix. */
		prefix = br_find_prefix("./");
		/* Construct the path. */
		snprintf(buf, len, "%s/"INSTALL_SUBDIR_SHARE"/%s", prefix, fname);
		free(prefix);
	}
#endif
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
	char *stmp, ctmp, version[MAX_BUF];

	snprintf(tmp, sizeof(tmp), "%s/.atrinik/%s/%s", get_config_dir(), package_get_version_partial(version, sizeof(version)), fname);

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

			get_data_dir_file(otmp, sizeof(otmp), fname);

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
			get_data_dir_file(tmp, sizeof(tmp), fname);
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
 * @param fname The file name.
 * @param mode File mode.
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

/**
 * TTF_OpenFont wrapper.
 * @param file The file name.
 * @param ptsize Size of font.
 * @return Return value of TTF_OpenFont(). */
TTF_Font *TTF_OpenFont_wrapper(const char *file, int ptsize)
{
	return TTF_OpenFont(file_path(file, "r"), ptsize);
}
/*@}*/

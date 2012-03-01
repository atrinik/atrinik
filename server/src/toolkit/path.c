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
 * OS path API. */

#include <global.h>

/**
 * Initialize the path API.
 * @internal */
void toolkit_path_init(void)
{
	TOOLKIT_INIT_FUNC_START(path)
	{
		toolkit_import(stringbuffer);
	}
	TOOLKIT_INIT_FUNC_END()
}

/**
 * Deinitialize the path API.
 * @internal */
void toolkit_path_deinit(void)
{
}

/**
 * Joins two path components, eg, '/usr' and 'bin' -> '/usr/bin'.
 * @param path First path component.
 * @param path2 Second path component.
 * @return The joined path; should be freed when no longer needed. */
char *path_join(const char *path, const char *path2)
{
	StringBuffer *sb;
	size_t len;
	char *cp;

	sb = stringbuffer_new();
	stringbuffer_append_string(sb, path);

	len = strlen(path);

	if (len && path[len - 1] != '/')
	{
		stringbuffer_append_string(sb, "/");
	}

	stringbuffer_append_string(sb, path2);
	cp = stringbuffer_finish(sb);

	return cp;
}

/**
 * Extracts the directory component of a path.
 *
 * Example:
 * @code
 * path_dirname("/usr/local/foobar"); --> "/usr/local"
 * @endcode
 * @param path A path.
 * @return A directory name. This string should be freed when no longer
 * needed.
 * @author Hongli Lai (public domain) */
char *path_dirname(const char *path)
{
	const char *end;
	char *result;

	if (!path)
	{
		return NULL;
	}

	end = strrchr(path, '/');

	if (!end)
	{
		return strdup(".");
	}

	while (end > path && *end == '/')
	{
		end--;
	}

	result = strndup(path, end - path + 1);

	if (result[0] == '\0')
	{
		free(result);
		return strdup("/");
	}

	return result;
}

/**
 * Extracts the basename from path.
 *
 * Example:
 * @code
 * path_basename("/usr/bin/kate"); --> "kate"
 * @endcode
 * @param path A path.
 * @return The basename of the path. Should be freed when no longer
 * needed. */
char *path_basename(const char *path)
{
	const char *slash;

	if (!path)
	{
		return NULL;
	}

	while ((slash = strrchr(path, '/')))
	{
		if (*(slash + 1) != '\0')
		{
			return strdup(slash + 1);
		}
	}

	return strdup(path);
}

/**
 * Checks whether any directories in the given path don't exist, and
 * creates them if necessary.
 * @param path The path to check. */
void path_ensure_directories(const char *path)
{
	char buf[MAXPATHLEN], *cp;
	struct stat statbuf;

	if (!path || *path == '\0')
	{
		return;
	}

	strncpy(buf, path, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	cp = buf;

	while ((cp = strchr(cp + 1, '/')))
	{
		*cp = '\0';

		if (stat(buf, &statbuf) || !S_ISDIR(statbuf.st_mode))
		{
			if (mkdir(buf, 0777))
			{
				logger_print(LOG(BUG), "Cannot mkdir %s: %s", buf, strerror(errno));
				return;
			}
		}

		*cp = '/';
	}
}

/**
 * Copy the contents of file 'src' into 'dst'.
 * @param src Path of the file to copy contents from.
 * @param dst Where to put the contents of 'src'.
 * @param mode Mode to open 'src' in.
 * @return 1 on success, 0 on failure. */
int path_copy_file(const char *src, FILE *dst, const char *mode)
{
	FILE *fp;
	char buf[HUGE_BUF];

	if (!src || !dst || !mode)
	{
		return 0;
	}

	fp = fopen(src, mode);

	if (!fp)
	{
		return 0;
	}

	while (fgets(buf, sizeof(buf), fp))
	{
		fputs(buf, dst);
	}

	fclose(fp);

	return 1;
}

/**
 * Check if the specified path exists.
 * @param path Path to check.
 * @return 1 if 'path' exists, 0 otherwise. */
int path_exists(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) != 0)
	{
		return 0;
	}

	return 1;
}

/**
 * Create a new blank file.
 * @param path Path to the file.
 * @return 1 on success, 0 on failure. */
int path_touch(const char *path)
{
	FILE *fp;

	path_ensure_directories(path);
	fp = fopen(path, "w");

	if (!fp)
	{
		return 0;
	}

	if (fclose(fp) == EOF)
	{
		return 0;
	}

	return 1;
}

/**
 * Get size of the specified file, in bytes.
 * @param path Path to the file.
 * @return Size of the file. */
size_t path_size(const char *path)
{
	struct stat statbuf;

	if (stat(path, &statbuf) != 0)
	{
		return 0;
	}

	return statbuf.st_size;
}

char *path_clean(const char *path, char *buf, size_t bufsize)
{
	size_t i;

	i = 0;

	while (path && *path != '\0' && i < bufsize - 1)
	{
		if (*path == '/')
		{
			buf[i] = '$';
		}
		else
		{
			buf[i] = *path;
		}

		path++;
		i++;
	}

	buf[i] = '\0';

	return buf;
}

char *path_unclean(const char *path, char *buf, size_t bufsize)
{
	const char *cp;
	size_t i;

	i = 0;
	cp = strrchr(path, '/');

	if (cp)
	{
		cp += 1;
	}
	else
	{
		cp = path;
	}

	while (cp && *cp != '\0' && i < bufsize - 1)
	{
		if (*cp == '$')
		{
			buf[i] = '/';
		}
		else
		{
			buf[i] = *cp;
		}

		cp++;
		i++;
	}

	buf[i] = '\0';

	return buf;
}

char *path_file_contents(const char *path)
{
	FILE *fp;
	StringBuffer *sb;
	char buf[MAX_BUF];

	fp = fopen(path, "rb");

	if (!fp)
	{
		return NULL;
	}

	sb = stringbuffer_new();

	while (fgets(buf, sizeof(buf), fp))
	{
		stringbuffer_append_string(sb, buf);
	}

	fclose(fp);

	return stringbuffer_finish(sb);
}

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
 * Miscellaneous functions. */

#include <global.h>

/**
 * Replaces "\n" by a newline char.
 *
 * Since we are replacing 2 chars by 1, no overflow should happen.
 * @param line Text to replace into. */
void convert_newline(char *str)
{
	char *next, buf[HUGE_BUF * 10];

	while ((next = strstr(str, "\\n")))
	{
		*next = '\n';
		*(next + 1) = '\0';
		snprintf(buf, sizeof(buf), "%s%s", str, next + 2);
		strcpy(str, buf);
	}
}

/**
 * Opens an url in the system's default browser.
 * @param url URL to open. */
void browser_open(const char *url)
{
#if defined(LINUX)
	char buf[HUGE_BUF];

	snprintf(buf, sizeof(buf), "xdg-open \"%s\"", url);

	if (system(buf) != 0)
	{
		snprintf(buf, sizeof(buf), "x-www-browser \"%s\"", url);

		if (system(buf) != 0)
		{
			LOG(llevBug, "browser_open(): Could not open '%s'.\n", url);
		}
	}
#elif defined(WIN32)
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT);
#else
	LOG(llevDebug, "browser_open(): Unknown platform, cannot open '%s'.\n", url);
#endif
}

/**
 * Get the full package version as string.
 *
 * If patch version is 0, it will not be appended to the version string.
 * @param dst Where to store the version.
 * @param dstlen Size of dst.
 * @return 'dst'. */
char *package_get_version_full(char *dst, size_t dstlen)
{
#if PACKAGE_VERSION_PATCH == 0
	package_get_version_partial(dst, dstlen);
#else
	snprintf(dst, dstlen, "%d.%d.%d", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCH);
#endif
	return dst;
}

/**
 * Get the partial package version. This means that the patch version
 * will not be included, even if it's not 0.
 * @param dst Where to store the version.
 * @param dstlen Size of dst.
 * @return 'dst' */
char *package_get_version_partial(char *dst, size_t dstlen)
{
	/* Upgrader version will overrule the package version if the upgrader
	 * is currently doing its job. */
	if (upgrader_get_version_partial(dst, dstlen))
	{
		return dst;
	}

	snprintf(dst, dstlen, "%d.%d", PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR);
	return dst;
}

/**
 * Convert BMP file to PNG, if supported by the platform.
 * @param path File to convert.
 * @return 1 if the file was converted to PNG, 0 otherwise. */
int bmp2png(const char *path)
{
#if defined(LINUX)
	char buf[HUGE_BUF];

	snprintf(buf, sizeof(buf), "convert \"%s\" \"`echo \"%s\" | sed -e 's/.bmp/.png/'`\" && rm \"%s\"", path, path, path);

	if (system(buf) != 0)
	{
		LOG(llevInfo, "bmp2png(): Could not convert %s from BMP to PNG.\n", path);
		return 0;
	}

	return 1;
#else
	(void) path;
	return 0;
#endif
}

/**
 * Create a screenshot of the specified surface.
 * @param surface The surface to take a screenshot of. */
void screenshot_create(SDL_Surface *surface)
{
	char path[HUGE_BUF], timebuf[MAX_BUF];
	struct timeval tv;
	struct tm *tm;

	if (!surface)
	{
		return;
	}

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	if (tm)
	{
		char timebuf2[MAX_BUF];

		strftime(timebuf2, sizeof(timebuf2), "%Y-%m-%d-%H-%M-%S", tm);
		snprintf(timebuf, sizeof(timebuf), "%s-%06"FMT64U, timebuf2, (uint64) tv.tv_usec);
	}
	else
	{
		draw_info(COLOR_RED, "Could not get time information.");
		return;
	}

	snprintf(path, sizeof(path), "%s/.atrinik/screenshots/Atrinik-%s.bmp", get_config_dir(), timebuf);
	mkdir_ensure(path);

	if (SDL_SaveBMP(surface, path) == 0)
	{
		draw_info_format(COLOR_GREEN, "Saved screenshot as %s successfully.", path);

		if (bmp2png(path))
		{
			draw_info(COLOR_GREEN, "Converted to PNG successfully.");
		}
	}
	else
	{
		draw_info_format(COLOR_RED, "Failed to write screenshot data (path: %s).", path);
	}
}

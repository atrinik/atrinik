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
 * Upgrades the settings from an older installation. */

#include <include.h>

/**
 * Client version numbers to migrate settings from. The last is tried
 * first. */
static const char *const client_versions[] =
{
	"1.0", "1.1", "1.1.1"
};

/** Files/directories to copy. */
static const char *const files_copy[] =
{
	/* Keys, format has been the same across all versions. */
	"keys.dat",
	/* Ignore lists. */
	"settings",
	/* Scripts to load automatically, format has been unchanged. */
	"scripts_autoload",
	/* Client options, format unchanged. */
	"options.dat"
};

/**
 * Perform the copying.
 * @param source_dir Source directory.
 * @param file File/directory inside 'source_dir' to copy.
 * @param dest_dir Destination directory for 'file'. */
static void copy_rec(const char *source_dir, const char *file, const char *dest_dir)
{
	char source[HUGE_BUF], tmp[HUGE_BUF];
	struct stat st;

	snprintf(source, sizeof(source), "%s/%s", source_dir, file);

	/* Does it exist? */
	if (stat(source, &st) != 0)
	{
		return;
	}

	/* Copy directory contents. */
	if (S_ISDIR(st.st_mode))
	{
		DIR *dir = opendir(source);
		struct dirent *currentfile;
		char newdir[HUGE_BUF], tmp2[HUGE_BUF];

		if (!dir)
		{
			return;
		}

		snprintf(newdir, sizeof(newdir), "%s/%s", dest_dir, file);
		mkdir(newdir, 0755);

		while ((currentfile = readdir(dir)))
		{
			if (currentfile->d_name[0] == '.')
			{
				continue;
			}

			snprintf(tmp, sizeof(tmp), "%s/%s", source, currentfile->d_name);
			snprintf(tmp2, sizeof(tmp2), "%s/%s", newdir, currentfile->d_name);
			copy_file(tmp, tmp2);
		}

		closedir(dir);
	}
	/* Copy file. */
	else
	{
		snprintf(tmp, sizeof(tmp), "%s/%s", dest_dir, file);
		copy_file(source, tmp);
	}
}

/**
 * Perform the settings upgrade.
 * @param source_dir Source directory. */
void upgrade_do(const char *source_dir)
{
	char dest_dir[HUGE_BUF];
	size_t i;

	/* Make the destination directory. */
	snprintf(dest_dir, sizeof(dest_dir), "%s/.atrinik/"PACKAGE_VERSION, get_config_dir());
	mkdir(dest_dir, 0755);

	/* Copy files from the old settings directory. */
	for (i = 0; i < arraysize(files_copy); i++)
	{
		copy_rec(source_dir, files_copy[i], dest_dir);
	}

	load_options_dat();

	/* Change resolution X/Y if needed. */
	if (options.resolution_x < 1024)
	{
		options.resolution_x = 1024;
	}

	if (options.resolution_y < 768)
	{
		options.resolution_y = 768;
	}

	save_options_dat();
}

/**
 * Called before anything else on start, to check if we need to migrate
 * settings. */
void upgrader_init()
{
	char tmp[HUGE_BUF];
	struct stat st;
	size_t i;

	snprintf(tmp, sizeof(tmp), "%s/.atrinik", get_config_dir());

	/* The .atrinik directory doesn't exist yet, nothing to migrate. */
	if (stat(tmp, &st) != 0)
	{
		return;
	}

	snprintf(tmp, sizeof(tmp), "%s/"PACKAGE_VERSION, tmp);

	/* If the settings directory for the current version already exists,
	 * leave. */
	if (stat(tmp, &st) == 0)
	{
		return;
	}

	/* Try looking for directory to migrate otherwise. */
	for (i = arraysize(client_versions); i > 0; i--)
	{
		snprintf(tmp, sizeof(tmp), "%s/.atrinik/%s", get_config_dir(), client_versions[i - 1]);

		if (stat(tmp, &st) == 0)
		{
			upgrade_do(tmp);
			return;
		}
	}
}

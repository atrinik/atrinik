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
 * Those functions deal with style for random maps. */

#include <global.h>

/**
 * Char comparison for sorting purposes.
 * @param p1 First pointer to compare.
 * @param p2 Second pointer to compare.
 * @return Return of strcmp() on pointed strings. */
static int pointer_strcmp(const void *p1, const void *p2)
{
	const char *s1 = *(const char * const *) p1;
	const char *s2 = *(const char * const *) p2;

	return (strcmp(s1, s2));
}

/**
 * This is our own version of scandir/select_regular_files/sort.
 *
 * To support having subdirectories in styles, we need to know if in fact
 * the directory we read is a subdirectory. However, we can't get that
 * through the normal dirent entry. So instead, we do our own where we do
 * have the full directory path so can do stat calls to see if in fact it
 * is a directory.
 * @param dir Name of the directory to scan.
 * @param namelist Array of file names returned. It needs to be freed by
 * the caller.
 * @param skip_dirs If nonzero, we don't skip any subdirectories - if
 * zero, we store those away, since there are cases where we want to
 * choose a random directory.
 * @return -1 if directory is invalid, number of files otherwise. */
int load_dir(const char *dir, char ***namelist, int skip_dirs)
{
	DIR *dp;
	struct dirent *d;
	int entries = 0, entry_size = 0;
	char name[NAME_MAX + 1], **rn = NULL;
	struct stat sb;

	dp = opendir(dir);

	if (dp == NULL)
	{
		return -1;
	}

	while ((d = readdir(dp)) != NULL)
	{
		if (skip_dirs)
		{
			snprintf(name, sizeof(name), "%s/%s", dir, d->d_name);
			stat(name, &sb);

			if (S_ISDIR(sb.st_mode))
			{
				continue;
			}
		}

		if (entries == entry_size)
		{
			entry_size += 10;
			rn = realloc(rn, sizeof(char *) * entry_size);
		}

		rn[entries] = strdup(d->d_name);
		entries++;
	}

	closedir(dp);

	qsort(rn, entries, sizeof(char *), pointer_strcmp);

	*namelist = rn;

	return entries;
}

/**
 * Loaded styles maps cache, to avoid having to load all the time. */
mapstruct *styles = NULL;

/**
 * Loads specified map (or take it from cache list).
 * @param style_name Map to load.
 * @return The loaded map. */
mapstruct *load_style_map(char *style_name)
{
	mapstruct *style_map;

	/* Given a file.  See if its in memory */
	for (style_map = styles; style_map != NULL; style_map = style_map->next)
	{
		if (!strcmp(style_name, style_map->path))
		{
			return style_map;
		}
	}

	style_map = load_original_map(style_name, MAP_STYLE);

	/* Remove it from global list, put it on our local list */
	if (style_map)
	{
		mapstruct *tmp;

		if (style_map == first_map)
		{
			first_map = style_map->next;
		}
		else
		{
			for (tmp = first_map; tmp && tmp->next != style_map; tmp = tmp->next)
			{
			}

			if (tmp)
			{
				tmp->next = style_map->next;
			}
		}

		style_map->next = styles;
		styles = style_map;
	}

	return style_map;
}

/**
 * Loads and returns the map requested.
 *
 * Dirname, for example, is "/styles/wallstyles", stylename, is,
 * for example, "castle", difficulty is -1 when difficulty is
 * irrelevant to the style.
 *
 * If dirname is given, but stylename isn't, and difficulty is -1, it
 * returns a random style map.
 *
 * Otherwise, it tries to match the difficulty given with a style file,
 * named style_name_# where # is an integer.
 * @param dirname Where to look.
 * @param stylename Style to use, can be NULL.
 * @param difficulty Style difficulty.
 * @return Style, or NULL if none suitable. */
mapstruct *find_style(char *dirname, char *stylename, int difficulty)
{
	char style_file_path[256], style_file_full_path[256];
	mapstruct *style_map = NULL;
	struct stat file_stat;
	int i;

	/* If stylename exists, set style_file_path to that file.*/
	if (stylename && strlen(stylename) > 0)
	{
		snprintf(style_file_path, sizeof(style_file_path), "%s/%s", dirname, stylename);
	}
	/* Otherwise, just use the dirname.  We'll pick a random stylefile.*/
	else
	{
		snprintf(style_file_path, sizeof(style_file_path), "%s", dirname);
	}

	/* Is what we were given a directory, or a file? */
	snprintf(style_file_full_path, sizeof(style_file_full_path), "%s/%s", settings.mapspath, style_file_path);

	stat(style_file_full_path, &file_stat);

	if (!(S_ISDIR(file_stat.st_mode)))
	{
		style_map = load_style_map(style_file_path);
	}

	/* Maybe we were given a directory! */
	if (style_map == NULL)
	{
		char **namelist;
		int n, only_subdirs = 0;
		char style_dir_full_path[256];

		/* Get the names of all the files in that directory */
		snprintf(style_dir_full_path, sizeof(style_dir_full_path), "%s/%s", settings.mapspath, style_file_path);

		/* First, skip subdirectories.  If we don't find anything, then try again
		 * without skipping subdirs. */
		n = load_dir(style_dir_full_path, &namelist, 1);

		if (n <= 0)
		{
			n = load_dir(style_dir_full_path, &namelist, 0);
			only_subdirs = 1;
		}

		/* Nothing to load.  Bye. */
		if (n <= 0)
		{
			return 0;
		}

		/* pick a random style from this dir. */
		if (difficulty == -1)
		{
			if (only_subdirs)
			{
				style_map = NULL;
			}
			else
			{
				strcat(style_file_path, "/");
				strcat(style_file_path, namelist[RANDOM() % n]);

				style_map = load_style_map(style_file_path);
			}
		}
		/* find the map closest in difficulty */
		else
		{
			int min_dist = 32000, min_index = -1;

			for (i = 0; i < n; i++)
			{
				int dist;
				char *mfile_name = strrchr(namelist[i], '_') + 1;

				/* since there isn't a sequence, pick one at random to recurse */
				if ((mfile_name - 1) == NULL)
				{
					int q;

					style_map = find_style(style_file_path, namelist[RANDOM() % n], difficulty);

					for (q = 0; q < n; q++)
					{
						free(namelist[q]);
					}

					free(namelist);

					return style_map;
				}
				else
				{
					dist = abs(difficulty - atoi(mfile_name));

					if (dist < min_dist)
					{
						min_dist = dist;
						min_index = i;
					}
				}
			}

			/* presumably now we've found the "best" match for the
			 * difficulty. */
			strcat(style_file_path, "/");
			strcat(style_file_path, namelist[min_index]);

			style_map = load_style_map(style_file_path);
		}

		for (i = 0; i < n; i++)
		{
			free(namelist[i]);
		}

		free(namelist);
	}

	return style_map;

}

/**
 * Picks a random object from a style map.
 * @param style Map to pick from.
 * @return The random object. Can be NULL. */
object *pick_random_object(mapstruct *style)
{
	int x, y, i;
	object *new_obj;

	/* If someone makes a style map that is empty, this will loop forever,
	 * but the callers will crash if we return a NULL object, so either
	 * way is not good. */
	do
	{
		i = RANDOM () % (MAP_WIDTH(style) * MAP_HEIGHT(style));

		x = i / MAP_HEIGHT(style);
		y = i % MAP_HEIGHT(style);
		new_obj = GET_MAP_OB(style, x, y);
	}
	while (new_obj == NULL);

	if (new_obj->head)
	{
		return new_obj->head;
	}
	else
	{
		return new_obj;
	}
}

/**
 * Frees cached style maps. */
void free_style_maps(void)
{
	mapstruct *next;
	int style_maps = 0;

	/* delete_map will try to free it from the linked list,
	 * but won't find it, so we need to do it ourselves */
	while (styles)
	{
		next = styles->next;
		delete_map(styles);
		styles = next;
		style_maps++;
	}
}

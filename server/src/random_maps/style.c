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


#include <global.h>
#include <random_map.h>

#ifndef WIN32 /* ---win32 exclude headers */
#ifdef NO_ERRNO_H
extern int errno;
#else
#   include <errno.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../include/autoconf.h"
#endif /* win32 */
#ifndef HAVE_SCANDIR

/* The scandir is grabbed from the gnulibc and modified slightly to remove
 * special gnu libc constructs/error conditions.
 */

/* Copyright (C) 1992, 1993, 1994, 1995, 1996 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

int alphasort( struct dirent **a, struct dirent **b)
{
	return strcmp ((*a)->d_name, (*b)->d_name);
}

int
scandir (dir, namelist, select, cmp)
const char *dir;
struct dirent ***namelist;
int (*select)(struct dirent *);
int (*cmp)(const void *, const void *);
{
	DIR *dp = opendir (dir);
	struct dirent **v = NULL;
	size_t vsize = 0, i;
	struct dirent *d;
	int save;

	if (dp == NULL)
		return -1;

	save=errno=0;

	i = 0;
	while ((d = readdir (dp)) != NULL)
		if (select == NULL || (*select) (d))
		{

			if (i == vsize)
			{
				struct dirent **new;
				if (vsize == 0)
					vsize = 10;
				else
					vsize *= 2;
				new = (struct dirent **) realloc (v, vsize * sizeof (*v));
				if (new == NULL)
				{
lose:
					errno=ENOMEM;

					break;
				}
				v = new;
			}

			/*	dsize = &d->d_name[sizeof(d->d_name)] - (char *) d; */
			v[i] = (struct dirent *) malloc (d->d_reclen);
			if (v[i] == NULL)
				goto lose;

			memcpy (v[i++], d, d->d_reclen);
		}

	if (errno != 0)
	{
		save = errno;
		(void) closedir (dp);
		while (i > 0)
			free (v[--i]);
		free (v);
		errno=save;
		return -1;
	}

	(void) closedir (dp);
	errno=save;

	/* Sort the list if we have a comparison function to sort with.  */
	if (cmp != NULL)
		qsort (v, i, sizeof (*v), cmp);
	*namelist = v;
	return i;
}

#endif



/* the warning here is because I've declared it "const", the
   .h file in linux allows non-const.  */
int select_regular_files(const struct dirent *the_entry)
{
	if (the_entry->d_name[0]=='.') return 0;
	if (strstr(the_entry->d_name,"CVS")) return 0;
	return 1;
}

/* this function loads and returns the map requested.
 * dirname, for example, is "/styles/wallstyles", stylename, is,
 * for example, "castle", difficulty is -1 when difficulty is
 * irrelevant to the style.  If dirname is given, but stylename
 * isn't, and difficult is -1, it returns a random style map.
 * Otherwise, it tries to match the difficulty given with a style
 * file, named style_name_# where # is an integer
 */

/* remove extern, so visible to command_style_map_info function */
mapstruct *styles=NULL;


mapstruct *load_style_map(char *style_name)
{
	mapstruct *style_map;

	/* Given a file.  See if its in memory */
	for (style_map = styles; style_map!=NULL; style_map=style_map->next)
	{
		if (!strcmp(style_name, style_map->path)) return style_map;
	}
	style_map = load_original_map(style_name,MAP_STYLE);
	/* Remove it from gloabl list, put it on our local list */
	if (style_map)
	{
		mapstruct *tmp;

		if (style_map == first_map)
			first_map = style_map->next;
		else
		{
			for (tmp = first_map; tmp && tmp->next != style_map; tmp = tmp->next);
			if (tmp)
				tmp->next = style_map->next;
		}
		style_map->next = styles;
		styles = style_map;
	}
	return style_map;
}

mapstruct *find_style(char *dirname,char *stylename,int difficulty)
{
	char style_file_path[256];
	char style_file_full_path[256];
	mapstruct *style_map = NULL;
	struct stat file_stat;
	int i;

	/* if stylename exists, set style_file_path to that file.*/
	if (stylename && strlen(stylename)>0)
		sprintf(style_file_path,"%s/%s",dirname,stylename);
	else /* otherwise, just use the dirname.  We'll pick a random stylefile.*/
		sprintf(style_file_path,"%s",dirname);

	/* is what we were given a directory, or a file? */
	sprintf(style_file_full_path,"%s/maps%s",settings.datadir,style_file_path);
	stat(style_file_full_path,&file_stat);


	if (! (S_ISDIR(file_stat.st_mode)))
	{
		style_map=load_style_map(style_file_path);
	}
	if (style_map == NULL) /* maybe we were given a directory! */
	{
		struct dirent **namelist;
		int n;
		char style_dir_full_path[256];

		/* get the names of all the files in that directory */
		sprintf(style_dir_full_path,"%s/maps%s",settings.datadir,style_file_path);
		n = scandir(style_dir_full_path,&namelist,select_regular_files,alphasort);

		if (n<=0) return 0; /* nothing to load.  Bye. */

		if (difficulty==-1)   /* pick a random style from this dir. */
		{
			strcat(style_file_path,"/");
			strcat(style_file_path,namelist[RANDOM()%n]->d_name);
			style_map = load_style_map(style_file_path);
		}
		else    /* find the map closest in difficulty */
		{
			int min_dist=32000,min_index=-1;

			for (i=0;i<n;i++)
			{
				int dist;
				char *mfile_name = strrchr(namelist[i]->d_name,'_')+1;

				if ((mfile_name-1) == NULL)  /* since there isn't a sequence, */
				{
					int q;
					/*pick one at random to recurse */
					style_map= find_style(style_file_path,
										  namelist[RANDOM()%n]->d_name,difficulty);
					for (q=0; q<n; q++)
						free(namelist[q]);
					free(namelist);
					return style_map;
				}
				else
				{
					dist = abs(difficulty-atoi(mfile_name));
					if (dist<min_dist)
					{
						min_dist = dist;
						min_index = i;
					}
				}
			}
			/* presumably now we've found the "best" match for the
			    difficulty. */
			strcat(style_file_path,"/");
			strcat(style_file_path,namelist[min_index]->d_name);
			style_map = load_style_map(style_file_path);
		}
		for (i=0; i<n; i++)
			free(namelist[i]);
		free(namelist);
	}
	return style_map;

}


/* picks a random object from a style map.
 * Redone by MSW so it should be faster and not use static
 * variables to generate tables.
 */
object *pick_random_object(mapstruct *style)
{
	int x,y, i;
	object *new_obj;

	/* If someone makes a style map that is empty, this will loop forever,
	 * but the callers will crash if we return a null object, so either
	 * way is not good.
	 */
	do
	{
		i = RANDOM () % (MAP_WIDTH(style) * MAP_HEIGHT(style));

		x = i / MAP_HEIGHT(style);
		y = i % MAP_HEIGHT(style);
		new_obj = get_map_ob(style,x,y);
	}
	while (new_obj == NULL);
	if (new_obj->head) return new_obj->head;
	else return new_obj;
}


void free_style_maps()
{
	mapstruct *next;
	int  style_maps=0;

	/* delete_map will try to free it from the linked list,
	 * but won't find it, so we need to do it ourselves
	 */
	while (styles)
	{
		next = styles->next;
		delete_map(styles);
		styles=next;
		style_maps++;
	}
	LOG(llevDebug,"free_style_maps: Freed %d maps\n", style_maps);
}


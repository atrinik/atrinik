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
 * Handles face-related stuff, including the actual face data. */

#include <global.h>

New_Face *new_faces;

/**
 * bmappair and ::xbm are used when looking for the image ID numbers of a
 * face by name. xbm is sorted alphabetically so that bsearch can be used
 * to quickly find the entry for a name. The number is then an index into
 * the ::new_faces array.
 *
 * This data is redundant with new_face information - the difference is
 * that this data gets sorted, and that doesn't necessarily happen with
 * the new_face data - when accessing new_face[some number], that some
 * number corresponds to the face at that number - for ::xbm, it may not.
 * At current time, these do in fact match because the bmaps file is
 * created in a sorted order. */
struct bmappair
{
	char *name;

	unsigned int number;
};

/**
 * The xbm array (which contains name and number information, and is then
 * sorted) contains ::nroffiles entries. */
static struct bmappair *xbm = NULL;

/**
 * Following can just as easily be pointers, but
 * it is easier to keep them like this. */
New_Face *blank_face, *next_item_face, *prev_item_face;

/** The actual number of bitmaps defined. */
int nroffiles = 0;

/**
 * The number of bitmaps loaded.  With the automatic generation of the
 * bmaps file, this is now equal to ::nroffiles. */
int nrofpixmaps = 0;

/**
 * Used for bsearch searching. */
static int compar(struct bmappair *a, struct bmappair *b)
{
	return strcmp(a->name, b->name);
}

/**
 * This reads the bmaps file to get all the bitmap names and stuff. It
 * only needs to be done once, because it is player independent (ie, what
 * display the person is on will not make a difference). */
int read_bmap_names()
{
	char buf[MAX_BUF], *cp;
	FILE *fp;
	int nrofbmaps = 0, i;
	size_t line = 0;

	snprintf(buf, sizeof(buf), "%s/bmaps", settings.datadir);
	LOG(llevDebug, "Reading bmaps from %s...", buf);

	if ((fp = fopen(buf, "r")) == NULL)
	{
		LOG(llevError, "ERROR: Can't open bmaps file: %s\n", buf);
	}

	/* First count how many bitmaps we have, so we can allocate correctly */
	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		if (buf[0] != '#' && buf[0] != '\n')
		{
			nrofbmaps++;
		}
	}

	rewind(fp);

	xbm = (struct bmappair *) malloc(sizeof(struct bmappair) * (nrofbmaps + 1));
	memset(xbm, 0, sizeof(struct bmappair) * (nrofbmaps + 1));

	while (fgets(buf, sizeof(buf) - 1, fp))
	{
		if (*buf == '#')
		{
			continue;
		}

		cp = strchr(buf, '\n');

		if (cp)
		{
			*cp = '\0';
		}

		cp = strchr(buf, ' ');

		if (cp)
		{
			cp++;
			xbm[nroffiles].name = strdup_local(cp);
		}
		else
		{
			xbm[nroffiles].name = strdup_local(buf);
		}

		xbm[nroffiles].number = line;

		nroffiles++;

		if ((int) line > nrofpixmaps)
		{
			nrofpixmaps++;
		}

		line++;
	}

	fclose(fp);

	LOG(llevDebug, " done (got %d/%d/%d)\n", nrofpixmaps, nrofbmaps, nroffiles);

	new_faces = (New_Face *) malloc(sizeof(New_Face) * (nrofpixmaps + 1));

	for (i = 0; i <= nrofpixmaps; i++)
	{
		new_faces[i].name = "";
		new_faces[i].number = i;
	}

	for (i = 0; i < nroffiles; i++)
	{
		new_faces[xbm[i].number].name = xbm[i].name;
	}

	nrofpixmaps++;

	qsort(xbm, nrofbmaps, sizeof(struct bmappair), (void *) (int (*)())compar);

	blank_face = &new_faces[find_face(BLANK_FACE_NAME, 0)];
	next_item_face = &new_faces[find_face(NEXT_ITEM_FACE_NAME, 0)];
	prev_item_face = &new_faces[find_face(PREVIOUS_ITEM_FACE_NAME, 0)];

	return nrofpixmaps;
}

/**
 * This returns the face number of face 'name'. Number is constant during
 * an invocation, but not necessarily between versions (this is because
 * the faces are arranged in alphabetical order, so if a face is removed
 * or added, all faces after that will have a different number).
 * @param name Face to search for.
 * @param error Value to return if face was not found. */
int find_face(char *name, int error)
{
	int i;
	struct bmappair *bp, tmp;
	char *p;

	/* Using actual numbers for faces is a very bad idea.  This is because
	 * each time the archetype file is rebuilt, all the face numbers
	 * change. */
	if ((i = atoi(name)))
	{
		LOG(llevBug, "BUG: Integer face name used: %s\n", name);
		return i;
	}

	if ((p = strchr(name, '\n')))
	{
		*p = '\0';
	}

	tmp.name = name;
	bp = (struct bmappair *) bsearch(&tmp, xbm, nroffiles, sizeof(struct bmappair), (void *) (int (*)()) compar);

	return bp ? bp->number : (unsigned int) error;
}

/** Deallocates memory allocated by read_bmap_names(). */
void free_all_images()
{
	int i;

	for (i = 0; i < nroffiles; i++)
	{
		free(xbm[i].name);
	}

	free(xbm);
	free(new_faces);
}

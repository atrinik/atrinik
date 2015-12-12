/*************************************************************************
 *           Atrinik, a Multiplayer Online Role Playing Game             *
 *                                                                       *
 *   Copyright (C) 2009-2014 Alex Tokar and Atrinik Development Team     *
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
 * Handles face-related stuff, including the actual face data. */

#include <global.h>
#include <toolkit_string.h>

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
struct bmappair {
    char *name;

    unsigned int number;
} ;

/**
 * The xbm array (which contains name and number information, and is then
 * sorted) contains ::nroffiles entries. */
static struct bmappair *xbm = NULL;

/** The actual number of bitmaps defined. */
int nroffiles = 0;

/**
 * The number of bitmaps loaded.  With the automatic generation of the
 * bmaps file, this is now equal to ::nroffiles. */
int nrofpixmaps = 0;

/**
 * Used for bsearch searching. */
static int compar(const void *a, const void *b)
{
    return strcmp(((const struct bmappair *) a)->name, ((const struct bmappair *) b)->name);
}

/**
 * This reads the bmaps file to get all the bitmap names and stuff. It
 * only needs to be done once, because it is player independent (ie, what
 * display the person is on will not make a difference). */
int read_bmap_names(void)
{
    char buf[MAX_BUF], *cp;
    FILE *fp;
    int nrofbmaps = 0, i;
    size_t line = 0;

    snprintf(buf, sizeof(buf), "%s/bmaps", settings.libpath);

    if ((fp = fopen(buf, "r")) == NULL) {
        LOG(ERROR, "Can't open bmaps file: %s", buf);
        exit(1);
    }

    /* First count how many bitmaps we have, so we can allocate correctly */
    while (fgets(buf, sizeof(buf) - 1, fp)) {
        if (buf[0] != '#' && buf[0] != '\n') {
            nrofbmaps++;
        }
    }

    rewind(fp);

    nrofpixmaps = 0;
    nroffiles = 0;

    xbm = emalloc(sizeof(struct bmappair) * (nrofbmaps + 1));
    memset(xbm, 0, sizeof(struct bmappair) * (nrofbmaps + 1));

    while (fgets(buf, sizeof(buf) - 1, fp)) {
        if (*buf == '#') {
            continue;
        }

        cp = strchr(buf, '\n');

        if (cp) {
            *cp = '\0';
        }

        cp = strchr(buf, ' ');

        if (cp) {
            cp++;
            xbm[nroffiles].name = estrdup(cp);
        } else {
            xbm[nroffiles].name = estrdup(buf);
        }

        xbm[nroffiles].number = line;

        nroffiles++;

        if ((int) line > nrofpixmaps) {
            nrofpixmaps++;
        }

        line++;
    }

    fclose(fp);

    new_faces = emalloc(sizeof(New_Face) * (nrofpixmaps + 1));

    for (i = 0; i < nrofpixmaps + 1; i++) {
        new_faces[i].name = "";
        new_faces[i].number = i;
    }

    for (i = 0; i < nroffiles; i++) {
        new_faces[xbm[i].number].name = xbm[i].name;
    }

    nrofpixmaps++;

    qsort(xbm, nrofbmaps, sizeof(struct bmappair), compar);

    return nrofpixmaps;
}

/**
 * This returns the face number of face 'name'. Number is constant during
 * an invocation, but not necessarily between versions (this is because
 * the faces are arranged in alphabetical order, so if a face is removed
 * or added, all faces after that will have a different number).
 * @param name Face to search for.
 * @param error Value to return if face was not found. */
int find_face(const char *name, int error)
{
    struct bmappair *bp, tmp;

    tmp.name = (char *) name;
    bp = bsearch(&tmp, xbm, nroffiles, sizeof(struct bmappair), compar);

    return bp ? bp->number : (unsigned int) error;
}

/** Deallocates memory allocated by read_bmap_names(). */
void free_all_images(void)
{
    int i;

    for (i = 0; i < nroffiles; i++) {
        efree(xbm[i].name);
    }

    efree(xbm);
    efree(new_faces);
}

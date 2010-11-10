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
 * This file deals with the image related communication to the client. */

#include <global.h>

#include <newclient.h>
#include <newserver.h>
#include <loader.h>
#include "zlib.h"

/** Maximum different face sets. */
#define MAX_FACE_SETS	1

/** Face info structure. */
typedef struct FaceInfo
{
	/** Image data */
	uint8 *data;

	/** Length of the XPM data */
	uint16 datalen;

	/** Checksum of face data */
	uint32 checksum;
} FaceInfo;

/** Face sets structure. */
typedef struct
{
	/** Prefix */
	char *prefix;

	/** Full name */
	char *fullname;

	/** Fallback */
	uint8 fallback;

	/** Size */
	char *size;

	/** Extension */
	char *extension;

	/** Comment */
	char *comment;

	/** Faces */
	FaceInfo *faces;
} FaceSets;

/** The face sets. */
static FaceSets facesets[MAX_FACE_SETS];

/**
 * Check if a face set is valid.
 * @param fsn The face set number to check
 * @return 1 if the face set is valid, 0 otherwise */
int is_valid_faceset(int fsn)
{
	if (fsn >= 0 && fsn < MAX_FACE_SETS && facesets[fsn].prefix)
	{
		return 1;
	}

	return 0;
}

/**
 * Free all the information in face sets. */
void free_socket_images()
{
	int num, q;

	for (num = 0; num < MAX_FACE_SETS; num++)
	{
		if (facesets[num].prefix)
		{
			for (q = 0; q < nrofpixmaps; q++)
			{
				if (facesets[num].faces[q].data)
				{
					free(facesets[num].faces[q].data);
				}
			}

			free(facesets[num].prefix);
			free(facesets[num].fullname);
			free(facesets[num].size);
			free(facesets[num].extension);
			free(facesets[num].comment);
			free(facesets[num].faces);
		}
	}
}

/**
 * This returns the set we will actually use when sending
 * a face. This is used because the image files may be sparse.
 *
 * This function is recursive.
 * @param faceset The face set ID to use.
 * @param imageno The face number we're trying to send.
 * @return The face set ID. */
static int get_face_fallback(int faceset, int imageno)
{
	/* faceset 0 is supposed to have every image, so just return.  Doing
	 * so also prevents infinite loops in the case if it not having
	 * the face, but in that case, we are likely to crash when we try
	 * to access the data, but that is probably preferable to an infinite
	 * loop. */
	if (faceset == 0)
	{
		return 0;
	}

	if (!facesets[faceset].prefix)
	{
		LOG(llevBug, "BUG: get_face_fallback called with unused set (%d)?\n", faceset);

		/* use default set */
		return 0;
	}

	if (facesets[faceset].faces[imageno].data)
	{
		return faceset;
	}

	return get_face_fallback(facesets[faceset].fallback, imageno);
}

/**
 * This is a simple recursive function that makes sure the fallbacks are
 * all proper (eg, they fall back to defined sets, and also eventually
 * fall back to 0).
 *
 * This is only run when we first load the facesets.
 * @param faceset The faceset ID
 * @param togo At the top level, this is set to MAX_FACE_SETS. If it gets
 * to zero, it means we have a loop. */
static void check_faceset_fallback(int faceset, int togo)
{
	int fallback = facesets[faceset].fallback;

	/* proper case - falls back to base set */
	if (fallback == 0)
	{
		return;
	}

	if (!facesets[fallback].prefix)
	{
		LOG(llevError, "Face set %d falls to non set faceset %d\n", faceset, fallback);
	}

	togo--;

	if (togo == 0)
	{
		LOG(llevError, "Infinite loop found in facesets. Aborting.\n");
	}

	check_faceset_fallback(fallback, togo);
}

/** Maximum possible size of a single image in bytes. */
#define MAX_IMAGE_SIZE 50000

/**
 * Loads up all the image types into memory.
 *
 * This way, we can easily send them to the client.
 *
 * This function also generates client_bmaps file here.
 *
 * At the moment, Atrinik only uses one face set file, no files like
 * atrinik.1, atrinik.2, etc. */
void read_client_images()
{
	char filename[400], buf[HUGE_BUF], *cp, *cps[7 + 1];
	FILE *infile, *fbmap;
	int num, len, compressed, fileno, i, badline;

	memset(facesets, 0, sizeof(facesets));

	snprintf(filename, sizeof(filename), "%s/image_info", settings.datadir);

	if ((infile = open_and_uncompress(filename, 0, &compressed)) == NULL)
	{
		LOG(llevError, "ERROR: read_client_images(): Unable to open %s\n", filename);
	}

	while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
	{
		badline = 0;

		if (buf[0] == '#')
		{
			continue;
		}

		if (split_string(buf, cps, sizeof(cps) / sizeof(*cps), ':') != 7)
		{
			LOG(llevBug, "BUG: read_client_images(): Bad line in image_info file, ignoring line:\n  %s", buf);
		}
		else
		{
			len = atoi(cps[0]);

			if (len >= MAX_FACE_SETS)
			{
				LOG(llevError, "ERROR: read_client_images(): Too high a setnum in image_info file: %d > %d\n", len, MAX_FACE_SETS);
			}

			facesets[len].prefix = strdup_local(cps[1]);
			facesets[len].fullname = strdup_local(cps[2]);
			facesets[len].fallback = atoi(cps[3]);
			facesets[len].size = strdup_local(cps[4]);
			facesets[len].extension = strdup_local(cps[5]);
			facesets[len].comment = strdup_local(cps[6]);
		}
	}

	close_and_delete(infile, compressed);

	for (i = 0; i < MAX_FACE_SETS; i++)
	{
		if (facesets[i].prefix)
		{
			check_faceset_fallback(i, MAX_FACE_SETS);
		}
	}

	/* Loaded the faceset information - now need to load up the
	 * actual faces. */
	for (fileno = 0; fileno < MAX_FACE_SETS; fileno++)
	{
		/* If prefix is not set, this is not used */
		if (!facesets[fileno].prefix)
		{
			continue;
		}

		facesets[fileno].faces = calloc(nrofpixmaps, sizeof(FaceInfo));

		snprintf(filename, sizeof(filename), "%s/atrinik.%d", settings.datadir, fileno);
		LOG(llevDebug, "Loading image file %s\n", filename);

		/* We don't use more than one face set here! */
		LOG(llevInfo, "Creating client_bmap....\n");
		snprintf(buf, sizeof(buf), "%s/client_bmaps", settings.localdir);

		if ((fbmap = fopen(buf, "wb")) == NULL)
		{
			LOG(llevError, "ERROR: read_client_images(): Unable to open %s\n", buf);
		}

		if ((infile = open_and_uncompress(filename, 0, &compressed)) == NULL)
		{
			LOG(llevError, "ERROR: read_client_images(): Unable to open %s\n", filename);
		}

		while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
		{
			if (strncmp(buf, "IMAGE ", 6) != 0)
			{
				LOG(llevError, "ERROR: read_client_images(): Bad image line - not IMAGE, instead\n%s", buf);
			}

			num = atoi(buf + 6);

			if (num < 0 || num >= nrofpixmaps)
			{
				LOG(llevError, "ERROR: read_client_images(): Image num %d not in 0..%d\n%s", num, nrofpixmaps, buf);
			}

			/* Skip accross the number data */
			for (cp = buf + 6; *cp != ' '; cp++)
			{
			}

			len = atoi(cp);

			if (len == 0 || len > MAX_IMAGE_SIZE)
			{
				LOG(llevError, "ERROR: read_client_images(): Length not valid: %d > %d \n%s", len, MAX_IMAGE_SIZE, buf);
			}

			/* We don't actualy care about the name if the image that
			 * is embedded in the image file, so just ignore it. */
			facesets[fileno].faces[num].datalen = len;
			facesets[fileno].faces[num].data = malloc(len);

			if ((i = fread(facesets[fileno].faces[num].data, len, 1, infile)) != 1)
			{
				LOG(llevError, "ERROR: read_client_images(): Did not read desired amount of data, wanted %d, got %d\n%s", len, i, buf);
			}

			facesets[fileno].faces[num].checksum = (uint32) crc32(1L, facesets[fileno].faces[num].data, len);
			snprintf(buf, sizeof(buf), "%x %x %s\n", len, facesets[fileno].faces[num].checksum, new_faces[num].name);
			fputs(buf, fbmap);
		}

		close_and_delete(infile, compressed);
		fclose(fbmap);
	}
}

/**
 * Client has requested a face.
 * @param buf The data sent to us.
 * @param len Length of 'buf'.
 * @param ns Client's socket. */
void SendFaceCmd(char *buf, int len, socket_struct *ns)
{
	long tmpnum;
	short facenum;

	if (!buf || !len)
	{
		return;
	}

	tmpnum = atoi(buf);
	facenum = tmpnum & 0xffff;

	if (facenum != 0)
	{
		esrv_send_face(ns, facenum);
	}
}

/**
 * Sends a face to client.
 * @param ns Client's socket.
 * @param face_num Face number to send.
 * @return One of @ref SEND_FACE_xxx. */
int esrv_send_face(socket_struct *ns, short face_num)
{
	SockList sl;
	int fallback;

	if (face_num < 0 || face_num >= nrofpixmaps)
	{
		LOG(llevBug, "BUG: esrv_send_face(): Face %d out of bounds!\n", face_num);
		return SEND_FACE_OUT_OF_BOUNDS;
	}

	fallback = get_face_fallback(ns->faceset, face_num);

	if (facesets[fallback].faces[face_num].data == NULL)
	{
		LOG(llevBug, "BUG: esrv_send_face(): faces[%d].data == NULL\n", face_num);
		return SEND_FACE_NO_DATA;
	}

	/* 1 byte for the command ID, 4 bytes for the face ID, 4 bytes for
	 * length of the face data. */
	sl.buf = malloc(1 + 4 + 4 + facesets[fallback].faces[face_num].datalen);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_IMAGE);
	SockList_AddInt(&sl, face_num);
	SockList_AddInt(&sl, facesets[fallback].faces[face_num].datalen);
	memcpy(sl.buf + sl.len, facesets[fallback].faces[face_num].data, facesets[fallback].faces[face_num].datalen);
	sl.len += facesets[fallback].faces[face_num].datalen;
	Send_With_Handling(ns, &sl);
	free(sl.buf);

	return SEND_FACE_OK;
}

/**
 * Get face's data.
 * @param face The face.
 * @param[out] ptr Pointer that will contain the image data, can be NULL.
 * @param[out] len Pointer that will contain the image data length, can
 * be NULL. */
void face_get_data(int face, uint8 **ptr, uint16 *len)
{
	if (ptr)
	{
		*ptr = facesets[0].faces[face].data;
	}

	if (len)
	{
		*len = facesets[0].faces[face].datalen;
	}
}

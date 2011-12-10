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
 * This file deals with the image related communication to the client. */

#include <global.h>
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
void free_socket_images(void)
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
void read_client_images(void)
{
	char filename[400], buf[HUGE_BUF], *cp, *cps[7 + 1];
	FILE *infile, *fbmap;
	int num, len, file_num, i;

	memset(facesets, 0, sizeof(facesets));

	snprintf(filename, sizeof(filename), "%s/image_info", settings.datadir);
	infile = fopen(filename, "rb");

	if (!infile)
	{
		LOG(llevError, "read_client_images(): Unable to open %s\n", filename);
	}

	while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
	{
		if (buf[0] == '#')
		{
			continue;
		}

		if (split_string(buf, cps, sizeof(cps) / sizeof(*cps), ':') != 7)
		{
			LOG(llevBug, "read_client_images(): Bad line in image_info file, ignoring line:\n  %s", buf);
		}
		else
		{
			len = atoi(cps[0]);

			if (len >= MAX_FACE_SETS)
			{
				LOG(llevError, "read_client_images(): Too high a setnum in image_info file: %d > %d\n", len, MAX_FACE_SETS);
			}

			facesets[len].prefix = strdup_local(cps[1]);
			facesets[len].fullname = strdup_local(cps[2]);
			facesets[len].size = strdup_local(cps[4]);
			facesets[len].extension = strdup_local(cps[5]);
			facesets[len].comment = strdup_local(cps[6]);
		}
	}

	fclose(infile);

	/* Loaded the faceset information - now need to load up the
	 * actual faces. */
	for (file_num = 0; file_num < MAX_FACE_SETS; file_num++)
	{
		/* If prefix is not set, this is not used */
		if (!facesets[file_num].prefix)
		{
			continue;
		}

		facesets[file_num].faces = calloc(nrofpixmaps, sizeof(FaceInfo));

		snprintf(filename, sizeof(filename), "%s/atrinik.%d", settings.datadir, file_num);
		LOG(llevDebug, "Loading image file %s\n", filename);

		/* We don't use more than one face set here! */
		LOG(llevDebug, "Creating client_bmap....\n");
		snprintf(buf, sizeof(buf), "%s/client_bmaps", settings.localdir);

		if ((fbmap = fopen(buf, "wb")) == NULL)
		{
			LOG(llevError, "read_client_images(): Unable to open %s\n", buf);
		}

		infile = fopen(filename, "rb");

		if (!infile)
		{
			LOG(llevError, "read_client_images(): Unable to open %s\n", filename);
		}

		while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
		{
			if (strncmp(buf, "IMAGE ", 6) != 0)
			{
				LOG(llevError, "read_client_images(): Bad image line - not IMAGE, instead\n%s", buf);
			}

			num = atoi(buf + 6);

			if (num < 0 || num >= nrofpixmaps)
			{
				LOG(llevError, "read_client_images(): Image num %d not in 0..%d\n%s", num, nrofpixmaps, buf);
			}

			/* Skip across the number data */
			for (cp = buf + 6; *cp != ' '; cp++)
			{
			}

			len = atoi(cp);

			if (len == 0 || len > MAX_IMAGE_SIZE)
			{
				LOG(llevError, "read_client_images(): Length not valid: %d > %d \n%s", len, MAX_IMAGE_SIZE, buf);
			}

			/* We don't actually care about the name if the image that
			 * is embedded in the image file, so just ignore it. */
			facesets[file_num].faces[num].datalen = len;
			facesets[file_num].faces[num].data = malloc(len);

			if ((i = fread(facesets[file_num].faces[num].data, len, 1, infile)) != 1)
			{
				LOG(llevError, "read_client_images(): Did not read desired amount of data, wanted %d, got %d\n%s", len, i, buf);
			}

			facesets[file_num].faces[num].checksum = (uint32) crc32(1L, facesets[file_num].faces[num].data, len);
			snprintf(buf, sizeof(buf), "%x %x %s\n", len, facesets[file_num].faces[num].checksum, new_faces[num].name);
			fputs(buf, fbmap);
		}

		fclose(infile);
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
	packet_struct *packet;

	if (face_num < 0 || face_num >= nrofpixmaps)
	{
		LOG(llevBug, "esrv_send_face(): Face %d out of bounds!\n", face_num);
		return SEND_FACE_OUT_OF_BOUNDS;
	}

	if (facesets[0].faces[face_num].data == NULL)
	{
		LOG(llevBug, "esrv_send_face(): faces[%d].data == NULL\n", face_num);
		return SEND_FACE_NO_DATA;
	}

	packet = packet_new(BINARY_CMD_IMAGE, 16, 0);
	packet_append_uint32(packet, face_num);
	packet_append_uint32(packet, facesets[0].faces[face_num].datalen);
	packet_append_data_len(packet, facesets[0].faces[face_num].data, facesets[0].faces[face_num].datalen);
	socket_send_packet(ns, packet);

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

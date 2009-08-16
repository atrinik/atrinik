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

/* This file deals with the image related communication to the
 * client.  I've located all the functions in this file - this
 * localizes it more, and means that we don't need to declare
 * things like all the structures as globals. */

#include <global.h>
#include <sproto.h>

#include <newclient.h>
#include <newserver.h>
#include <loader.h>
#include "zlib.h"

#define MAX_FACE_SETS	1

typedef struct FaceInfo
{
	/* image data */
	uint8 *data;
	/* length of the xpm data */
	uint16 datalen;
	/* Checksum of face data */
	uint32 checksum;
} FaceInfo;


typedef struct
{
	char *prefix;
	char *fullname;
	uint8 fallback;
	char *size;
	char *extension;
	char *comment;
	FaceInfo *faces;
} FaceSets;

static FaceSets facesets[MAX_FACE_SETS];

int is_valid_faceset(int fsn)
{
	if (fsn >= 0 && fsn < MAX_FACE_SETS && facesets[fsn].prefix)
		return 1;

	return 0;
}

void free_socket_images()
{
	int num, q;

	for (num = 0; num < MAX_FACE_SETS; num++)
	{
		if (facesets[num].prefix)
		{
			for (q = 0; q < nrofpixmaps; q++)
				if (facesets[num].faces[q].data)
					free(facesets[num].faces[q].data);

			free(facesets[num].prefix);
			free(facesets[num].fullname);
			free(facesets[num].size);
			free(facesets[num].extension);
			free(facesets[num].comment);
			free(facesets[num].faces);
		}
	}
}

/* This returns the set we will actually use when sending
 * a face.  This is used because the image files may be sparse.
 * This function is recursive.  imageno is the face number we are
 * trying to send. */
static int get_face_fallback(int faceset, int imageno)
{
	/* faceset 0 is supposed to have every image, so just return.  Doing
	 * so also prevents infinite loops in the case if it not having
	 * the face, but in that case, we are likely to crash when we try
	 * to access the data, but that is probably preferable to an infinite
	 * loop. */
	if (faceset == 0)
		return 0;

	if (!facesets[faceset].prefix)
	{
		LOG(llevBug, "BUG: get_face_fallback called with unused set (%d)?\n", faceset);
		/* use default set */
		return 0;
	}

	if (facesets[faceset].faces[imageno].data)
		return faceset;

	return get_face_fallback(facesets[faceset].fallback, imageno);
}

/* This is a simple recursive function that makes sure the fallbacks
 * are all proper (eg, the fall back to defined sets, and also
 * eventually fall back to 0).  At the top level, togo is set to MAX_FACE_SETS,
 * if togo gets to zero, it means we have a loop.
 * This is only run when we first load the facesets. */
static void check_faceset_fallback(int faceset, int togo)
{
	int fallback = facesets[faceset].fallback;

	/* proper case - falls back to base set */
	if (fallback == 0)
		return;

	if (!facesets[fallback].prefix)
		LOG(llevError,"Face set %d falls to non set faceset %d\n", faceset, fallback);

	togo--;
	if (togo == 0)
		LOG(llevError, "Infinite loop found in facesets. Aborting.\n");

	check_faceset_fallback(fallback, togo);
}

/* read_client_images loads all the iamge types into memory.
 *  This  way, we can easily send them to the client.  We should really do something
 * better than abort on any errors - on the other hand, these are all fatal
 * to the server (can't work around them), but the abort just seems a bit
 * messy (exit would probably be better.) */

/* Couple of notes:  We assume that the faces are in a continous block.
 * This works fine for now, but this could perhaps change in the future */

/* Function largely rewritten May 2000 to be more general purpose.
 * The server itself does not care what the image data is - to the server,
 * it is just data it needs to allocate.  As such, the code is written
 * to do such. */

/* i added the generation of client_bmaps here... i generate only *one*
 * file - i don't use different sets (atrinik.1, atrinik.2,...) but i mix
 * the code up - if ever one is interested to add here and in the client full
 * different set power, he can complete this stuff... note now: have more than
 * one set will break the server atm */

#define MAX_IMAGE_SIZE 50000
void read_client_images()
{
	char filename[400];
	char buf[HUGE_BUF];
	char *cp, *cps[7];
	FILE *infile, *fbmap;
	int num, len, compressed, fileno, i, badline;

	memset(facesets, 0, sizeof(facesets));

	sprintf(filename, "%s/image_info", settings.datadir);
	if ((infile = open_and_uncompress(filename, 0, &compressed)) == NULL)
		LOG(llevError, "Unable to open %s\n", filename);

	while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
	{
		badline = 0;

		if (buf[0] == '#')
			continue;

		if (!(cps[0] = strtok(buf, ":")))
			badline = 1;

		for (i = 1; i<7; i++)
		{
			if (!(cps[i] = strtok(NULL, ":")))
				badline = 1;
		}

		if (badline)
			LOG(llevBug, "BUG: Bad line in image_info file, ignoring line:\n  %s", buf);
		else
		{
			len = atoi(cps[0]);

			if (len >= MAX_FACE_SETS)
				LOG(llevError, "Too high a setnum in image_info file: %d > %d\n", len, MAX_FACE_SETS);

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
			check_faceset_fallback(i, MAX_FACE_SETS);
	}

	/* Loaded the faceset information - now need to load up the
	 * actual faces. */

	for (fileno = 0; fileno < MAX_FACE_SETS; fileno++)
	{
		/* if prefix is not set, this is not used */
		if (!facesets[fileno].prefix)
			continue;

		facesets[fileno].faces = calloc(nrofpixmaps, sizeof(FaceInfo));

		sprintf(filename, "%s/atrinik.%d", settings.datadir, fileno);
		LOG(llevDebug, "Loading image file %s\n", filename);
		/* we don't use more than one face set here!! */
		LOG(llevInfo, "Creating client_bmap....\n");
		sprintf(buf, "%s/client_bmaps", settings.localdir);

		if ((fbmap = fopen(buf,"wb")) == NULL)
			LOG(llevError, "Unable to open %s\n", buf);

		if ((infile = open_and_uncompress(filename, 0, &compressed)) == NULL)
			LOG(llevError, "Unable to open %s\n", filename);

		while (fgets(buf, HUGE_BUF - 1, infile) != NULL)
		{
			if (strncmp(buf, "IMAGE ", 6) != 0)
				LOG(llevError, "read_client_images: Bad image line - not IMAGE, instead\n%s", buf);

			num = atoi(buf + 6);

			if (num < 0 || num >= nrofpixmaps)
				LOG(llevError, "read_client_images: Image num %d not in 0..%d\n%s", num, nrofpixmaps, buf);

			/* Skip accross the number data */
			for (cp = buf + 6; *cp != ' '; cp++);

			len = atoi(cp);

			if (len == 0 || len > MAX_IMAGE_SIZE)
				LOG(llevError, "read_client_images: length not valid: %d > %d \n%s", len, MAX_IMAGE_SIZE, buf);

			/* We don't actualy care about the name if the image that
			 * is embedded in the image file, so just ignore it. */
			facesets[fileno].faces[num].datalen = len;
			facesets[fileno].faces[num].data = malloc(len);

			if ((i = fread(facesets[fileno].faces[num].data, len, 1, infile)) != 1)
				LOG(llevError, "read_client_images: Did not read desired amount of data, wanted %d, got %d\n%s", len, i, buf);

			facesets[fileno].faces[num].checksum = (uint32)crc32(1L, facesets[fileno].faces[num].data, len);
			sprintf(buf, "%x %x %s\n", len, facesets[fileno].faces[num].checksum, new_faces[num].name);
			fputs(buf, fbmap);
		}
		close_and_delete(infile, compressed);
		fclose(fbmap);
	}
}

/* Client tells us what type of faces it wants.  Also sets
 * the caching attribute. */
void SetFaceMode(char *buf, int len, NewSocket *ns)
{
	char tmp[256];
	int mask = (atoi(buf) & CF_FACE_CACHE), mode = (atoi(buf) & ~CF_FACE_CACHE);

	(void) len;

	if (mode == CF_FACE_NONE)
	{
		ns->facecache = 1;
	}
	else if (mode != CF_FACE_PNG)
	{
		sprintf(tmp, "X%d %s", NDI_RED, "Warning - send unsupported face mode.  Will use Png");
		Write_String_To_Socket(ns, BINARY_CMD_DRAWINFO, tmp, strlen(tmp));
#ifdef ESRV_DEBUG
		LOG(llevDebug, "SetFaceMode: Invalid mode from client: %d\n", mode);
#endif
	}

	if (mask)
		ns->facecache = 1;
}

/* client has requested pixmap that it somehow missed getting
 * This will be called often if the client is
 * caching images. */
void SendFaceCmd(char *buff, int len, NewSocket *ns)
{
	long tmpnum = atoi(buff);
	short facenum = tmpnum & 0xffff;

	(void) len;

	if (facenum != 0)
		esrv_send_face(ns, facenum, 1);
}

/* esrv_send_face sends a face to a client if they are in pixmap mode
 * nothing gets sent in bitmap mode.
 * If nocache is true (nonzero), ignore the cache setting from the client -
 * this is needed for the askface, in which we really do want to send the
 * face (and askface is the only place that should be setting it).  Otherwise,
 * we look at the facecache, and if set, send the image name. */
/* return: 0 - all ok. 1: face nr out of bound, 2: face data not avaible
 * define in global.h:
 * #define SEND_FACE_OK 0
 * #define SEND_FACE_OUT_OF_BOUNDS 1
 * #define SEND_FACE_NO_DATA 2 */
int esrv_send_face(NewSocket *ns, short face_num, int nocache)
{
	SockList sl;
	int fallback;

	if (face_num < 0 || face_num >= nrofpixmaps)
	{
		LOG(llevBug, "BUG: esrv_send_face (%d) out of bounds??\n", face_num);
		return SEND_FACE_OUT_OF_BOUNDS;
	}

	sl.buf = malloc(MAXSOCKBUF);
	fallback = get_face_fallback(ns->faceset, face_num);

	if (facesets[fallback].faces[face_num].data == NULL)
	{
		LOG(llevBug, "BUG: esrv_send_face: faces[%d].data == NULL\n", face_num);
		return SEND_FACE_NO_DATA;
	}

	if (ns->facecache && !nocache)
	{
		if (ns->image2)
		{
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_FACE2);
		}
		else if (ns->sc_version >= 1026)
		{
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_FACE1);
		}
		else
		{
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_FACE);
		}

		SockList_AddShort(&sl, face_num);

		if (ns->image2)
			SockList_AddChar(&sl, (char) fallback);

		if (ns->sc_version >= 1026)
			SockList_AddInt(&sl, facesets[fallback].faces[face_num].checksum);

		strcpy((char*)sl.buf + sl.len, new_faces[face_num].name);
		sl.len += strlen(new_faces[face_num].name);
		Send_With_Handling(ns, &sl);
	}
	else
	{
		if (ns->image2)
		{
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_IMAGE2);
		}
		/*strcpy((char*)sl.buf, "image2 ");*/
		else
		{
			SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_IMAGE);
		}

		/*strcpy((char*)sl.buf, "image ");*/
		/*sl.len = strlen((char*)sl.buf);*/
		SockList_AddInt(&sl, face_num);

		if (ns->image2)
			SockList_AddChar(&sl, (char) fallback);

		SockList_AddInt(&sl, facesets[fallback].faces[face_num].datalen);
		memcpy(sl.buf+sl.len, facesets[fallback].faces[face_num].data, facesets[fallback].faces[face_num].datalen);
		sl.len += facesets[fallback].faces[face_num].datalen;
		Send_With_Handling(ns, &sl);
	}

	/*ns->faces_sent[face_num] = 1;*/
	free(sl.buf);

	return SEND_FACE_OK;
}

/* send_image_info: Sends the number of images, checksum of the face file,
 * and the image_info file information.  See the doc/Developers/protocol
 * if you want further detail. */
void send_image_info(NewSocket *ns, char *params)
{
	SockList sl;
	int i;

	(void) params;

	sl.buf = malloc(MAXSOCKBUF);

	sprintf((char *)sl.buf, "replyinfo image_info\n%d\n%d\n", nrofpixmaps - 1, bmaps_checksum);
	for (i = 0; i < MAX_FACE_SETS; i++)
	{
		if (facesets[i].prefix)
		{
			sprintf((char *)sl.buf + strlen((char *)sl.buf), "%d:%s:%s:%d:%s:%s:%s", i, facesets[i].prefix, facesets[i].fullname, facesets[i].fallback, facesets[i].size, facesets[i].extension, facesets[i].comment);
		}
	}
	sl.len = strlen((char *)sl.buf);
	Send_With_Handling(ns, &sl);
	free(sl.buf);
}

void send_image_sums(NewSocket *ns, char *params)
{
	int start, stop, qq, i;
	char *cp, buf[MAX_BUF];
	SockList sl;

	sl.buf = malloc(MAXSOCKBUF);

	start = atoi(params);

	for (cp = params; *cp != '\0'; cp++)
		if (*cp == ' ')
			break;

	stop = atoi(cp);

	if (stop < start || *cp == '\0' || (stop-start) > 1000 || stop >= nrofpixmaps)
	{
		sprintf(buf, "Ximage_sums %d %d", start, stop);
		Write_String_To_Socket(ns, BINARY_CMD_REPLYINFO, buf, strlen(buf));
		return;
	}

	sprintf((char *)sl.buf, "Ximage_sums %d %d ", start, stop);
	*sl.buf = BINARY_CMD_REPLYINFO;

	sl.len = strlen((char *)sl.buf);

	for (i = start; i <= stop; i++)
	{
		SockList_AddShort(&sl, (uint16) i);
		qq = get_face_fallback(ns->faceset, i);
		SockList_AddInt(&sl, facesets[qq].faces[i].checksum);
		SockList_AddChar(&sl, (char) qq);
		qq = strlen(new_faces[i].name);
		SockList_AddChar(&sl, (char)(qq + 1));
		strcpy((char *)sl.buf + sl.len, new_faces[i].name);
		sl.len += qq;
		SockList_AddChar(&sl, 0);
	}

	/* It would make more sense to catch this pre-emptively in the code above.
	 * however, if this really happens, we probably just want to cut down the
	 * size to less than 1000, since that is what we claim the protocol would
	 * support. */
	if (sl.len > MAXSOCKBUF)
		LOG(llevError, "ERROR: send_image_send: buffer overrun, %s > %s\n", sl.len, MAXSOCKBUF);

	Send_With_Handling(ns, &sl);
	free(sl.buf);
}

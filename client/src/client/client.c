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

/**
 * @file
 * Client interface main routine.
 *
 * This file sets up a few global variables, connects to the server,
 * tells it what kind of pictures it wants, adds the client and enters
 * the main event loop (event_loop()) checks the tcp socket for input and
 * then polls for x events. This should be fixed since you can just block
 * on both filedescriptors.
 *
 * The DoClient function recieves a message (an ArgList), unpacks it, and
 * in a slow for loop dispatches the command to the right function
 * through the commands table. ArgLists are essentially like RPC things,
 * only they don't require going through RPCgen, and it's easy to get
 * variable length lists. They are just lists of longs, strings,
 * characters, and byte arrays that can be converted to a machine
 * independent format. */

#include <include.h>

/** Client player structure with things like stats, damage, etc */
Client_Player cpl;

/** Client socket */
ClientSocket csocket;

/** Used for socket commands */
typedef void (*CmdProc)(unsigned char *, int len);

struct CmdMapping
{
	char *cmdname;
	void (*cmdproc)(unsigned char *, int);
};

enum
{
	BINARY_CMD_COMC = 1,
	BINARY_CMD_MAP2,
	BINARY_CMD_DRAWINFO,
	BINARY_CMD_DRAWINFO2,
	BINARY_CMD_MAP_SCROLL,
	BINARY_CMD_ITEMX,
	BINARY_CMD_SOUND,
	BINARY_CMD_TARGET,
	BINARY_CMD_UPITEM,
	BINARY_CMD_DELITEM,
	BINARY_CMD_STATS,
	BINARY_CMD_IMAGE,
	BINARY_CMD_FACE1,
	BINARY_CMD_ANIM,
	BINARY_CMD_SKILLRDY,
	BINARY_CMD_PLAYER,
	BINARY_CMD_MAPSTATS,
	BINARY_CMD_SPELL_LIST,
	BINARY_CMD_SKILL_LIST,
	BINARY_CMD_GOLEMCMD,
	BINARY_CMD_ADDME_SUC,
	BINARY_CMD_ADDME_FAIL,
	BINARY_CMD_VERSION,
	BINARY_CMD_BYE,
	BINARY_CMD_SETUP,
	BINARY_CMD_QUERY,
	BINARY_CMD_DATA,
	BINARY_CMD_NEW_CHAR,
	BINARY_CMD_ITEMY,
	BINARY_CMD_BOOK,
	BINARY_CMD_PARTY,
	BINARY_CMD_QUICKSLOT,
	BINARY_CMD_SHOP,
	BINARY_CMD_QLIST,
	/* last entry */
	BINAR_CMD
};

/** Structure of all the socket commands */
struct CmdMapping commands[] =
{
	/* Order of this table doesn't make a difference.  I tried to sort
	 * of cluster the related stuff together. */
	{"comc", CompleteCmd},
	{"map2", Map2Cmd},
	{"drawinfo", (CmdProc)DrawInfoCmd},
	{"drawinfo2", (CmdProc)DrawInfoCmd2},
	{"map_scroll", (CmdProc)map_scrollCmd},
	{"itemx", ItemXCmd},
	{"sound", SoundCmd},
	{"to", TargetObject},
	{"upditem", UpdateItemCmd},
	{"delitem", DeleteItem},
	{"stats", StatsCmd},
	{"image", ImageCmd},
	{"face1", Face1Cmd},
	{"anim", AnimCmd},
	{"skill_rdy", (CmdProc) SkillRdyCmd},
	{"player", PlayerCmd},
	{"mapstats", (CmdProc)MapstatsCmd},
	{"splist", (CmdProc)SpelllistCmd},
	{"sklist", (CmdProc)SkilllistCmd},
	{"gc", (CmdProc)GolemCmd},
	{"addme_success", AddMeSuccess},
	{"addme_failed", AddMeFail},
	{"version", (CmdProc)VersionCmd},
	{"goodbye", GoodbyeCmd},
	{"setup", (CmdProc)SetupCmd},
	{"query", (CmdProc)handle_query},
	{"data", (CmdProc)DataCmd},
	{"new_char", (CmdProc)NewCharCmd},
	{"itemy", ItemYCmd},
	{"book", BookCmd},
	{"pt", PartyCmd},
	{"qs", (CmdProc)QuickSlotCmd},
	{"shop", ShopCmd},
	{"qlist", QuestListCmd},

	/* Unused! */
	{"magicmap", MagicMapCmd},
	{"delinv", (CmdProc)DeleteInventory},
};

static void face_flag_extension(int pnum, char *buf);

/**
 * Do client. The main loop for commands. From this, the data and
 * commands from server are received.
 * @param csocket Socket. */
void DoClient(ClientSocket *csocket)
{
	int i, len;
	uint8 cmd_id;
	unsigned char *data;

	while (1)
	{
		i = socket_read(csocket->fd, &csocket->inbuf, MAXSOCKBUF - 1);

		if (i == -1)
		{
			/* Need to add some better logic here */
			LOG(LOG_MSG, "Got error on read (error %d)\n", socket_get_error());
			socket_close(csocket->fd);

			return;
		}

		/* Don't have a full packet */
		if (i == 0)
		{
			return;
		}

		csocket->inbuf.buf[csocket->inbuf.len] = '\0';

		/* 3 byte header */
		if (csocket->inbuf.buf[0] & 0x80)
		{
			cmd_id = (uint8) csocket->inbuf.buf[3];
			data = csocket->inbuf.buf + 4;
			/* 3 byte package len + 1 byte binary cmd */
			len = csocket->inbuf.len - 4;
		}
		else
		{
			cmd_id = (uint8) csocket->inbuf.buf[2];
			data = csocket->inbuf.buf + 3;
			/* 2 byte package len + 1 byte binary cmd */
			len = csocket->inbuf.len - 3;
		}

		if (!cmd_id || cmd_id >= BINAR_CMD)
		{
			LOG(LOG_ERROR, "Bad command from server (%d) (%d)\n", cmd_id, BINAR_CMD);
		}
		else
		{
			commands[cmd_id - 1].cmdproc(data, len);
		}

		csocket->inbuf.len = 0;
	}
}

/**
 * Init socket list, setting the list's len to 0 and buf to NULL.
 * @param sl Socket list. */
void SockList_Init(SockList *sl)
{
	sl->len = 0;
	sl->buf = NULL;
}

/**
 * Add character to socket list.
 * @param sl Socket list.
 * @param c Character to add. */
void SockList_AddChar(SockList *sl, char c)
{
	sl->buf[sl->len] = c;
	sl->len++;
}

/**
 * Add short integer to socket list.
 * @param sl Socket list.
 * @param data Short integer data. */
void SockList_AddShort(SockList *sl, uint16 data)
{
	sl->buf[sl->len++] = (data >> 8) & 0xff;
	sl->buf[sl->len++] = data & 0xff;
}

/**
 * Add integer to socket list.
 * @param sl Socket list.
 * @param data Integer data. */
void SockList_AddInt(SockList *sl, uint32 data)
{
	sl->buf[sl->len++] = (data >> 24) & 0xff;
	sl->buf[sl->len++] = (data >> 16) & 0xff;
	sl->buf[sl->len++] = (data >> 8) & 0xff;
	sl->buf[sl->len++] = data & 0xff;
}

/**
 * Does the reverse of SockList_AddInt, but on strings instead.
 * @param data The string.
 * @return Integer from the string. */
int GetInt_String(unsigned char *data)
{
	return ((data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]);
}

/**
 * Does the reverse of SockList_AddShort, but on strings instead.
 * @param data The string.
 * @return Short integer from the string. */
short GetShort_String(unsigned char *data)
{
	return ((data[0] << 8) + data[1]);
}

/**
 * Send With Handling.
 * @param fd File descriptor to send the socklist to.
 * @param msg Message to send.
 * @return 0 on success, -1 on failure. */
int send_socklist(int fd, SockList msg)
{
	unsigned char sbuf[2];

	sbuf[0] = ((uint32)(msg.len) >> 8) & 0xFF;
	sbuf[1] = ((uint32)(msg.len)) & 0xFF;

	socket_write(fd, sbuf, 2);
	return socket_write(fd, msg.buf, msg.len);
}

/**
 * Takes a string of data, and writes it out to the socket.
 * @param fd File descriptor to send the string to.
 * @param buf The string.
 * @param len Length of the string.
 * @return 0 on success, -1 on failure. */
int cs_write_string(int fd, char *buf, int len)
{
	SockList sl;

	sl.len = len;
	sl.buf = (unsigned char *) buf;
	return send_socklist(fd, sl);
}

/**
 * Finish face command.
 * @param pnum ID of the face.
 * @param checksum Face checksum.
 * @param face Face name. */
void finish_face_cmd(int pnum, uint32 checksum, char *face)
{
	char buf[2048];
	FILE *stream;
	struct stat statbuf;
	int len;
	static uint32 newsum = 0;
	unsigned char *data;
	void *tmp_free;

	/* Loaded or requested. */
	if (FaceList[pnum].name)
	{
		/* Let's check the name, checksum and sprite. Only if all is ok,
		 * we stay with it */
		if (!strcmp(face, FaceList[pnum].name) && checksum == FaceList[pnum].checksum && FaceList[pnum].sprite)
		{
			face_flag_extension(pnum, FaceList[pnum].name);
			return;
		}

		/* Something is different. */
		tmp_free = &FaceList[pnum].name;
		FreeMemory(tmp_free);
		sprite_free_sprite(FaceList[pnum].sprite);
	}

	snprintf(buf, sizeof(buf), "%s.png", face);
	FaceList[pnum].name = (char *) _malloc(strlen(buf) + 1, "finish_face_cmd(): FaceList name");
	strcpy(FaceList[pnum].name, buf);

	FaceList[pnum].checksum = checksum;

	/* Check private cache first */
	snprintf(buf, sizeof(buf), "%s%s", GetCacheDirectory(), FaceList[pnum].name);

	if ((stream = fopen_wrapper(buf, "rb")) != NULL)
	{
		fstat(fileno (stream), &statbuf);
		len = (int) statbuf.st_size;
		data = malloc(len);
		len = fread(data, 1, len, stream);
		fclose(stream);
		newsum = 0;

		/* Something is wrong... Unlink the file and let it reload. */
		if (len <= 0)
		{
			unlink(buf);
			checksum = 1;
		}
		/* Checksum check */
		else
		{
			newsum = crc32(1L, data, len);
		}

		free(data);

		if (newsum == checksum)
		{
			FaceList[pnum].sprite = sprite_tryload_file(buf, 0, NULL);

			if (FaceList[pnum].sprite)
			{
				face_flag_extension(pnum, buf);
				return;
			}
		}
	}

	face_flag_extension(pnum, buf);
	snprintf(buf, sizeof(buf), "askface %d", pnum);
	cs_write_string(csocket.fd, buf, strlen(buf));
}

/**
 * Check face's flag extension.
 * @param pnum Face number.
 * @param buf Name of the face. */
static void face_flag_extension(int pnum, char *buf)
{
	char *stemp;

	FaceList[pnum].flags = FACE_FLAG_NO;

	/* Check for the "double" / "up" tag in the picture name. */
	if ((stemp = strstr(buf, ".d")))
	{
		FaceList[pnum].flags |= FACE_FLAG_DOUBLE;
	}
	else if ((stemp = strstr(buf, ".u")))
	{
		FaceList[pnum].flags |= FACE_FLAG_UP;
	}

	/* If a tag was there, grab the facing info */
	if (FaceList[pnum].flags && stemp)
	{
		int tc;

		for (tc = 0; tc < 4; tc++)
		{
			if (!*(stemp + tc))
			{
				return;
			}
		}

		/* Set the right flags for the tags */
		if (((FaceList[pnum].flags & FACE_FLAG_UP) && *(stemp + tc) == '5') || *(stemp + tc) == '1')
		{
			FaceList[pnum].flags |= FACE_FLAG_D1;
		}
		else if (*(stemp + tc) == '3')
		{
			FaceList[pnum].flags |= FACE_FLAG_D3;
		}
		else if (*(stemp + tc) == '4'|| *(stemp + tc) == '8' || *(stemp + tc) == '0')
		{
			FaceList[pnum].flags |= (FACE_FLAG_D3 | FACE_FLAG_D1);
		}
	}
}

/**
 * Load picture from atrinik.p0 file.
 * @param num ID of the picture to load.
 * @return 1 if the file does not exist, 0 otherwise. */
static int load_picture_from_pack(int num)
{
	FILE *stream;
	char *pbuf;
	SDL_RWops *rwop;

	if ((stream = fopen_wrapper(FILE_ATRINIK_P0, "rb")) == NULL)
	{
		return 1;
	}

	lseek(fileno(stream), bmaptype_table[num].pos, SEEK_SET);

	pbuf = malloc(bmaptype_table[num].len);

	if (!fread(pbuf, bmaptype_table[num].len, 1, stream))
	{
		fclose(stream);
		return 0;
	}

	fclose(stream);

	rwop = SDL_RWFromMem(pbuf, bmaptype_table[num].len);

	FaceList[num].sprite = sprite_tryload_file(NULL, 0, rwop);

	if (FaceList[num].sprite)
	{
		face_flag_extension(num, FaceList[num].name);
	}

	free(rwop);
	free(pbuf);

	return 0;
}

/** Maximum face request */
#define REQUEST_FACE_MAX 250

/**
 * We got a face - test if we have it loaded. If not, ask the server to
 * send us face command.
 * @param pnum Face ID.
 * @param mode Mode.
 * @return 0 if face is not there, 1 if face was requested or loaded. */
int request_face(int pnum, int mode)
{
	char buf[256 * 2];
	FILE *stream;
	struct stat statbuf;
	int len;
	unsigned char *data;
	static int count = 0;
	static char fr_buf[REQUEST_FACE_MAX * sizeof(uint16) + 4];
	uint16 num = (uint16)(pnum &~ 0x8000);

	/* Forced flush buffer and command */
	if (mode)
	{
		if (count)
		{
			fr_buf[0] = 'f';
			fr_buf[1] = 'r';
			fr_buf[2] = ' ';
			cs_write_string(csocket.fd, fr_buf, 4 + count * sizeof(uint16));
			count = 0;
		}

		return 1;
	}

	/* Loaded or requested */
	if (FaceList[num].name || FaceList[num].flags & FACE_REQUESTED)
	{
		return 1;
	}

	if (num >= bmaptype_table_size)
	{
		LOG(LOG_ERROR, "REQUEST_FILE(): server sent picture id to big (%d %d)\n", num, bmaptype_table_size);
		return 0;
	}

	/* First check for this image in gfx_user directory. */
	snprintf(buf, sizeof(buf), "%s%s.png", GetGfxUserDirectory(), bmaptype_table[num].name);

	if ((stream = fopen_wrapper(buf, "rb")) != NULL)
	{
		fstat(fileno(stream), &statbuf);
		len = (int) statbuf.st_size;
		data = malloc(len);
		len = fread(data, 1, len, stream);
		fclose(stream);

		if (len > 0)
		{
			/* Try to load it. */
			FaceList[num].sprite = sprite_tryload_file(buf, 0, NULL);

			if (FaceList[num].sprite)
			{
				face_flag_extension(num, buf);
				snprintf(buf, sizeof(buf), "%s%s.png", GetGfxUserDirectory(), bmaptype_table[num].name);
				FaceList[num].name = (char*) malloc(strlen(buf) + 1);
				strcpy(FaceList[num].name, buf);
				FaceList[num].checksum = crc32(1L, data, len);
				free(data);
				return 1;
			}
		}

		/* If we are here something was wrong with the file. */
		free(data);
	}

	/* Best case - we have it in atrinik.p0 */
	if (bmaptype_table[num].pos != -1)
	{
		snprintf(buf, sizeof(buf), "%s.png", bmaptype_table[num].name);
		FaceList[num].name = (char *) _malloc(strlen(buf) + 1, "request_face(): FaceList name");
		strcpy(FaceList[num].name, buf);
		FaceList[num].checksum = bmaptype_table[num].crc;
		load_picture_from_pack(num);
	}
	/* Second best case - check the cache for it, or request it. */
	else
	{
		FaceList[num].flags |= FACE_REQUESTED;
		finish_face_cmd(num, bmaptype_table[num].crc, bmaptype_table[num].name);
	}

	return 1;
}

/**
 * Check animation status.
 * @param anum Animation ID. */
void check_animation_status(int anum)
{
	/* Check if it has been loaded. */
	if (animations[anum].loaded)
	{
		return;
	}

	/* Mark this animation as loaded. */
	animations[anum].loaded = 1;
	/* Same as server sends it */
	AnimCmd((unsigned char *) anim_table[anum].anim_cmd, anim_table[anum].len);
}

/**
 * Remove whitespace from right side.
 * @param buf The string to adjust.
 * @return The adjusted string. */
char *adjust_string(char *buf)
{
	int i, len = strlen(buf);

	for (i = len - 1; i >= 0; i--)
	{
		if (!isspace(buf[i]))
		{
			return buf;
		}

		buf[i] = '\0';
	}

	return buf;
}

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
 * Client interface main routine.
 *
 * This file sets up a few global variables, connects to the server,
 * tells it what kind of pictures it wants, adds the client and enters
 * the main event loop (event_loop()) checks the tcp socket for input and
 * then polls for x events. This should be fixed since you can just block
 * on both filedescriptors.
 *
 * The DoClient function receives a message (an ArgList), unpacks it, and
 * in a slow for loop dispatches the command to the right function
 * through the commands table. ArgLists are essentially like RPC things,
 * only they don't require going through RPCgen, and it's easy to get
 * variable length lists. They are just lists of longs, strings,
 * characters, and byte arrays that can be converted to a machine
 * independent format. */

#include <global.h>

/** Client player structure with things like stats, damage, etc */
Client_Player cpl;

/** Client socket */
ClientSocket csocket;

/** Used for socket commands */
typedef void (*CmdProc)(unsigned char *, int len);

struct CmdMapping
{
	const char *cmdname;
	void (*cmdproc)(unsigned char *, int);
	enum CmdFormat cmdformat;
};

enum
{
	BINARY_CMD_COMC = 1,
	BINARY_CMD_MAP2,
	BINARY_CMD_DRAWINFO,
	BINARY_CMD_DRAWINFO2,
	BINARY_CMD_FILE_UPD,
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
	BINARY_CMD_CLEAR,
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
	BINARY_CMD_REGION_MAP,
	BINARY_CMD_READY,
	BINARY_CMD_KEEPALIVE,
	BINARY_CMD_SOUND_AMBIENT,
	BINARY_CMD_INTERFACE,
	BINARY_CMD_NOTIFICATION,
	/* last entry */
	BINAR_CMD
};

/** Structure of all the socket commands */
static struct CmdMapping commands[] =
{
	/* Order of this table doesn't make a difference.  I tried to sort
	 * of cluster the related stuff together. */
	{"comc", CompleteCmd, SHORT_INT},
	{"map2", Map2Cmd, MIXED},
	{"drawinfo", (CmdProc) DrawInfoCmd, ASCII},
	{"drawinfo2", (CmdProc) DrawInfoCmd2, ASCII},
	{"request_upd", cmd_request_update, ASCII},
	{"itemx", ItemXCmd, MIXED},
	{"sound", SoundCmd, ASCII},
	{"to", TargetObject, ASCII},
	{"upditem", UpdateItemCmd, MIXED},
	{"delitem", DeleteItem, INT_ARRAY},
	{"stats", StatsCmd, STATS},
	{"image", ImageCmd, ASCII},
	{"face1", NULL, SHORT_ARRAY},
	{"anim", AnimCmd, SHORT_ARRAY},
	{"skill_rdy", (CmdProc) SkillRdyCmd, ASCII},
	{"player", PlayerCmd, MIXED},
	{"mapstats", MapStatsCmd, ASCII},
	{"splist", (CmdProc) SpelllistCmd, ASCII},
	{"sklist", (CmdProc) SkilllistCmd, ASCII},
	{"clr", NULL, ASCII},
	{"addme_success", AddMeSuccess, NODATA},
	{"addme_failed", AddMeFail, NODATA},
	{"version", cmd_version, MIXED},
	{"goodbye", NULL, NODATA},
	{"setup", (CmdProc) SetupCmd, ASCII},
	{"query", (CmdProc) handle_query, ASCII},
	{"data", (CmdProc) DataCmd, MIXED},
	{"new_char", (CmdProc) NewCharCmd, NODATA},
	{"itemy", ItemYCmd, MIXED},
	{"book", BookCmd, ASCII},
	{"pt", PartyCmd, ASCII},
	{"qs", QuickSlotCmd, ASCII},
	{"compressed", cmd_compressed, MIXED},
	{"qlist", QuestListCmd, ASCII},
	{"region_map", RegionMapCmd, ASCII},
	{"rd", ReadyCmd, INT_ARRAY},
	{"ka", NULL, NODATA},
	{"sound_ambient", cmd_sound_ambient, MIXED},
	{"interface", cmd_interface, MIXED},
	{"notification", cmd_notification, MIXED},

	/* Unused! */
	{"magicmap", MagicMapCmd, NODATA},
	{"delinv", (CmdProc) DeleteInventory, NODATA},
};

/**
 * Do client. The main loop for commands. From this, the data and
 * commands from server are received. */
void DoClient(void)
{
	command_buffer *cmd;

	/* Handle all enqueued commands */
	while ((cmd = get_next_input_command()))
	{
		uint8 *data = cmd->data, *dest = NULL;
		size_t len = cmd->len;

		/* Binary command #0 is reserved for compressed data packets, so
		 * attempt to uncompress it. */
		if (data[0] == 0)
		{
			unsigned long ucomp_len;

			/* Get original length so we can allocate a large enough
			 * buffer. */
			ucomp_len = GetInt_String(data + 1) + 1;
			/* Allocate the buffer. */
			dest = malloc(ucomp_len);

			if (!dest)
			{
				LOG(llevError, "DoClient(): Out of memory.\n");
			}

			uncompress((Bytef *) dest, (uLongf *) &ucomp_len, (const Bytef *) data + 5, (uLong) len - 5);
			data = dest;
			len = ucomp_len;
			data[len] = '\0';
		}

		if (!data[0] || data[0] > BINAR_CMD)
		{
			LOG(llevBug, "Bad command from server (%d)\n", data[0]);
		}
		else
		{
			script_trigger_event(commands[data[0] - 1].cmdname, data + 1, len - 1, commands[data[0] - 1].cmdformat);
			commands[data[0] - 1].cmdproc(data + 1, len - 1);
		}

		/* Should we free the data because it was allocated by previous
		 * uncompression? */
		if (dest)
		{
			free(dest);
		}

		command_buffer_free(cmd);
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
 * Add an unterminated string.
 * @param sl SockList instance to add to.
 * @param data The string to add. */
void SockList_AddString(SockList *sl, char *data)
{
	char c;

	while ((c = *data++))
	{
		sl->buf[sl->len] = c;
		sl->len++;
	}
}

/**
 * Add a NULL terminated string.
 * @param sl SockList instance to add to.
 * @param data The string to add. */
void SockList_AddStringTerminated(SockList *sl, char *data)
{
	char c;

	while ((c = *data++))
	{
		sl->buf[sl->len] = c;
		sl->len++;
	}

	sl->buf[sl->len] = c;
	sl->len++;
}

/**
 * Does the reverse of SockList_AddInt, but on strings instead.
 * @param data The string.
 * @return Integer from the string. */
int GetInt_String(const unsigned char *data)
{
	return ((data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3]);
}

/**
 * 64-bit version of GetInt_String().
 * @param data The string.
 * @return Integer from the string. */
sint64 GetInt64_String(const unsigned char *data)
{
#ifdef WIN32
	return (((sint64) data[0] << 56) + ((sint64) data[1] << 48) + ((sint64) data[2] << 40) + ((sint64) data[3] << 32) + ((sint64) data[4] << 24) + ((sint64) data[5] << 16) + ((sint64) data[6] << 8) + (sint64) data[7]);
#else
	return (((uint64) data[0] << 56) + ((uint64) data[1] << 48) + ((uint64) data[2] << 40) + ((uint64) data[3] << 32) + ((uint64) data[4] << 24) + (data[5] << 16) + (data[6] << 8) + data[7]);
#endif
}

/**
 * Does the reverse of SockList_AddShort, but on strings instead.
 * @param data The string.
 * @return Short integer from the string. */
short GetShort_String(const unsigned char *data)
{
	return ((data[0] << 8) + data[1]);
}

/**
 * Construct a string from data packet.
 * @param data Data packet.
 * @param[out] pos Position in the data packet.
 * @param dest Will contain the string from data packet.
 * @param dest_size Size of 'dest'.
 * @return 'dest'. */
char *GetString_String(uint8 *data, int *pos, char *dest, size_t dest_size)
{
	size_t i = 0;
	char c;

	while ((c = (char) (data[(*pos)++])))
	{
		if (i < dest_size - 1)
		{
			dest[i++] = c;
		}
	}

	dest[i] = '\0';
	return dest;
}

/**
 * Takes a string of data, and writes it out to the socket.
 * @param fd File descriptor to send the string to.
 * @param buf The string.
 * @param len Length of the string.
 * @return 0 on success, -1 on failure. */
int cs_write_string(char *buf, size_t len)
{
	SockList sl;

	sl.len = (int) len;
	sl.buf = (unsigned char *) buf;

	return send_socklist(sl);
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

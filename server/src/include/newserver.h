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
 * Defines various structures and values that are used for the new client
 * server communication method.
 *
 * @see newclient.h */

#ifndef NEWSERVER_H
#define NEWSERVER_H

/** How many items to show in the below window. Used in esrv_draw_look(). */
#define NUM_LOOK_OBJECTS 15

#ifdef WIN32
#pragma pack(push,1)
#endif

/** One map cell, as sent to the client */
typedef struct MapCell_struct
{
	int	count;

	short faces[MAP_LAYERS];

	uint8 fflag[MAP_LAYERS];

	uint8 ff_probe[MAP_LAYERS];

	char quick_pos[MAP_LAYERS];
} MapCell;

struct Map
{
	struct MapCell_struct cells[MAP_CLIENT_X][MAP_CLIENT_Y];
};

struct statsinfo
{
	char *range,
	*title,
	*ext_title;
};

/** Possible socket statuses. */
enum Sock_Status
{
	Ns_Avail,
	Ns_Wait,
	Ns_Add,
	Ns_Login,
	Ns_Dead
};

/**
 * The following is the setup for a ring buffer for storing outbut
 * data that the OS can't handle right away. */
typedef struct Buffer_struct
{
	int	start;

	int	len;

	char data[SOCKETBUFSIZE];
} Buffer;

#ifdef WIN32
#pragma pack(pop)
#endif

/** This contains basic information on the socket structure. */
typedef struct NewSocket_struct
{
	/** The actual file descriptor we are using. */
	int fd;

	/**
	 * If someone is too long idle in the login, he will get
	 * disconnected. */
	int login_count;

	/** X size of the map the client wants */
	int mapx;

	/** Y size of the map the client wants */
	int mapy;

	/** X size of the map the client wants / 2 */
	int mapx_2;

	/** Y size of the map the client wants / 2 */
	int mapy_2;

	/** Which host it is connected from (ip address) */
	char *host;

	/** Version of the client */
	uint32 cs_version;

	/** Version of the client */
	uint32 sc_version;

	/** Marker to see we must update the below windows of the tile the player is */
	uint32 update_tile;

	/** Marker to map draw/draw below */
	uint32 below_clear:1;

	/** When set, a "connect" was initizialised as "player" */
	uint32 addme:1;

	/** If true, client is caching images */
	uint32 facecache:1;

	/** Sent map scroll command */
	uint32 sent_scroll:1;

	/** Does the client want sound? */
	uint32 sound:1;

	/** Always use map2 protocol command */
	uint32 map2cmd:1;

	/** Send ext title to client */
	uint32 ext_title_flag:1;

	/** True if client wants darkness information */
	uint32 darkness:1;

	/** Client wants image2/face2 commands */
	uint32 image2:1;

	/** Can we write to this socket? */
	uint32 can_write:1;

	/** Has the client sent version command? */
	uint32 version:1;

	/** Has the client requested setup command? */
	uint32 setup:1;

	/** Has the client requested settings file? */
	uint32 rf_settings:1;

	/** Has the client requested skills file? */
	uint32 rf_skills:1;

	/** Has the client requested spells file? */
	uint32 rf_spells:1;

	/** Has the client requested animations file? */
	uint32 rf_anims:1;

	/** Has the client requested bitmaps file? */
	uint32 rf_bmaps:1;

	/** Has the client requested hfiles file? */
	uint32 rf_hfiles:1;

	/** Start of drawing of look window */
	sint16 look_position;

	/** Faceset the client is using, default 0 */
	uint8 faceset;

	/**
	 * How many times the player has failed to provide the right
	 * password. */
	uint8 password_fails;

	/** Current state of the socket. */
	enum Sock_Status status;

	/** Last map */
	struct Map lastmap;

	/** Socket stats */
	struct statsinfo stats;

	/** If we get an incomplete packet, this is used to hold the data. */
	SockList inbuf;

	/** For undeliverable data */
	Buffer outputbuffer;
} NewSocket;

/** Holds some system related information */
typedef struct Socket_Info_struct
{
	/** Timeout for select */
	struct timeval timeout;

	/** Max filedescriptor on the system */
	int	max_filedescriptor;

	/** Number of connections */
	int	nconns;

	/** Number of allocated in init_sockets */
	int allocated_sockets;
} Socket_Info;

extern Socket_Info socket_info;

/**
 * Maximum password failures allowed before the server kills the
 * socket. */
#define MAX_PASSWORD_FAILURES 3

#endif

/************************************************************************
*            Atrinik, a Multiplayer Online Role Playing Game            *
*                                                                       *
*    Copyright (C) 2009-2012 Alex Tokar and Atrinik Development Team    *
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
 * Defines various structures and values that are used for the new client
 * server communication method.
 *
 * @see newclient.h */

#ifndef NEWSERVER_H
#define NEWSERVER_H

/** How many items to show in the below window. Used in esrv_draw_look(). */
#define NUM_LOOK_OBJECTS 15

/**
 * One map cell. Used to hold 'cache' of faces we already sent
 * to the client. */
typedef struct MapCell_struct
{
	/** Cache of last sent ambient sound. */
	tag_t sound_ambient_count;

	/* Everything below will be cleared by memset() in when the map
	 * cell is no longer visible. */

	/** Darkness cache. */
	int	count;

	/** Faces we sent. */
	sint16 faces[NUM_REAL_LAYERS];

	/** Multi-arch cache. */
	uint8 quick_pos[NUM_REAL_LAYERS];

	/** Flags cache. */
	uint8 flags[NUM_REAL_LAYERS];

	/**
	 * Probe cache. No need for an array, since this only appears
	 * for players or monsters, both on layer 6. */
	uint8 probe;

	/**
	 * Possible target object UID cache. */
	tag_t target_object_count;
} MapCell;

/** One map for a player. */
struct Map
{
	/** The map cells. */
	struct MapCell_struct cells[MAP_CLIENT_X][MAP_CLIENT_Y];
};

/** Possible socket statuses. */
enum
{
	ST_AVAILABLE,
	ST_WAITING,
	ST_LOGIN,
	ST_PLAYING,
	ST_DEAD,
	ST_ZOMBIE
};

/** This contains basic information on the socket structure. */
typedef struct socket_struct
{
	/** The actual file descriptor we are using. */
	int fd;

	/**
	 * If someone is too long idle in the login, he will get
	 * disconnected. */
	int login_count;

	/** X size of the map the client wants. */
	int mapx;

	/** Y size of the map the client wants. */
	int mapy;

	/** X size of the map the client wants / 2. */
	int mapx_2;

	/** Y size of the map the client wants / 2. */
	int mapy_2;

	/** Which host it is connected from (ip address). */
	char *host;

	/** Version of the client. */
	uint32 socket_version;

	/** Marker to see we must update the below windows of the tile the player is. */
	uint32 update_tile;

	/** Marker to map draw/draw below. */
	uint32 below_clear:1;

	/** When set, a "connect" was initialized as "player". */
	uint32 addme:1;

	/** Does the client want sound? */
	uint32 sound:1;

	uint8 requested_file[SERVER_FILES_MAX];

	/** Is the client a bot? */
	uint8 is_bot;

	/** Start of drawing of look window. */
	sint16 look_position;

	/** Faceset the client is using, default 0. */
	uint8 faceset;

	/**
	 * How many times the player has failed to provide the right
	 * password. */
	uint8 password_fails;

	/** Send ext title to client. */
	uint8 ext_title_flag;

	/** Current state of the socket. */
	int state;

	/** Last map. */
	struct Map lastmap;

	struct packet_struct *packet_head;
	struct packet_struct *packet_tail;
	pthread_mutex_t packet_mutex;

	/**
	 * Buffer for how many ticks have passed since the last keep alive
	 * command. When this reaches @ref SOCKET_KEEPALIVE_TIMEOUT, the
	 * socket is disconnected. */
	uint32 keepalive;

	char *account;

    struct packet_struct *packet_recv;
    struct packet_struct *packet_recv_cmd;
} socket_struct;

/**
 * How many seconds must pass since the last keep alive command for the
 * socket to be disconnected. */
#define SOCKET_KEEPALIVE_TIMEOUT (60 * 10)

/** Holds some system related information. */
typedef struct Socket_Info_struct
{
	/** Timeout for select. */
	struct timeval timeout;

	/** Max filedescriptor on the system. */
	int	max_filedescriptor;

	/** Number of connections. */
	int	nconns;

	/** Number of allocated in init_sockets. */
	int allocated_sockets;
} Socket_Info;

/**
 * A single file loaded from the updates directory that the client can
 * request. */
typedef struct update_file_struct
{
	/** File's CRC32. */
	unsigned long checksum;

	/** Length of the file. */
	size_t len;

	/** Uncompressed length of the file. */
	size_t ucomp_len;

	/** Name of the file. */
	char *filename;

	/** Compressed contents of the file. */
	uint8 *contents;

	/** Packet to use for sending the file. */
	struct packet_struct *packet;
} update_file_struct;

/** Filename used to store information about the updated files. */
#define UPDATES_FILE_NAME "updates"
/**
 * Directory to recursively traverse, looking for files that the client
 * can request for an update. */
#define UPDATES_DIR_NAME "updates"

/**
 * Maximum password failures allowed before the server kills the
 * socket. */
#define MAX_PASSWORD_FAILURES 3

#endif

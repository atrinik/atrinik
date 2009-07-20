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

/*
    newserver.h defines various structures and values that are use for the
    new client server communication method.  Values defined here are only
    used on the server side code.  For shared client/server values, see
    newclient.h
*/


#ifndef NEWSERVER_H
#define NEWSERVER_H


#define NUM_LOOK_OBJECTS 15

#ifdef WIN32
#pragma pack(push,1)
#endif

typedef struct MapCell_struct {
    int	count;

    short faces[MAP_LAYERS];

    uint8 fflag[MAP_LAYERS];

    uint8 ff_probe[MAP_LAYERS];

    char quick_pos[MAP_LAYERS];
} MapCell;

struct Map {
  	struct MapCell_struct cells[MAP_CLIENT_X][MAP_CLIENT_Y];
};

struct statsinfo {
    char *range,
	*title,
	*ext_title;
};


/* This contains basic information on the socket structure.  status is its
 * current state.  we set up our on buffers for sending/receiving, so we can
 * handle some higher level functions.  fd is the actual file descriptor we
 * are using. */

enum Sock_Status {
	Ns_Avail,
	Ns_Wait,
	Ns_Add,
	Ns_Login,
	Ns_Dead
};


/* The following is the setup for a ring buffer for storing outbut
 * data that the OS can't handle right away. */

typedef struct Buffer_struct {
    int	start;

    int	len;

    char data[SOCKETBUFSIZE];
} Buffer;


#ifdef WIN32
#pragma pack(pop)
#endif

typedef struct NewSocket_struct {
    int fd;

	/* if someone is too long idle in the login, we kick him here! */
	int login_count;

	/* How large a map the client wants */
    int mapx, mapy;

	/* same like above but /2 */
    int mapx_2, mapy_2;

	/* Which host it is connected from (ip address)*/
    char *host;

	/* versions of the client */
    uint32 cs_version, sc_version;

	/* marker to see we must update the below windows of the tile the player is */
	uint32 update_tile;

	/* start of drawing of look window */
    sint16 look_position;

	/* Set the client is using, default 0 */
    uint8 faceset;

    enum Sock_Status status;

    struct Map lastmap;

    struct statsinfo stats;

    /* If we get an incomplete packet, this is used to hold the data. */
    SockList inbuf;

	/* For undeliverable data */
    Buffer outputbuffer;

	/* marker to map draw/draw below */
	uint32 below_clear:1;

	/* important: when set, a "connect" was initizialised as "player" */
	uint32 addme:1;

	/* If true, client is caching images */
    uint32 facecache:1;

    uint32 sent_scroll:1;

	/* does the client want sound */
    uint32 sound:1;

	/* Always use map2 protocol command */
    uint32 map2cmd:1;

	/* send ext title to client */
	uint32 ext_title_flag:1;

	/* True if client wants darkness information */
    uint32 darkness:1;

	/* Client wants image2/face2 commands */
    uint32 image2:1;

	/* Can we write to this socket? */
    uint32 can_write:1;

	/* these blocks are simple flags to ensure
	 * that the client don't hammer startup commands
	 * again & again to abuse the server. */
	uint32 version:1;

	uint32 setup:1;

	uint32 rf_settings:1;

	uint32 rf_skills:1;

	uint32 rf_spells:1;

	uint32 rf_anims:1;

	uint32 rf_bmaps:1;

	uint32 rf_hfiles:1;

    /* Below here is information only relevant for old sockets */
    /*char *comment;*/
} NewSocket;

#define FACE_TYPES  1

#define PNG_FACE_INDEX	0

typedef struct Socket_Info_struct {
	/* Timeout for select */
    struct timeval timeout;

	/* max filedescriptor on the system */
    int	max_filedescriptor;

	/* Number of connections */
    int	nconns;

	/* number of allocated in init_sockets */
    int	 allocated_sockets;
} Socket_Info;

extern Socket_Info socket_info;

#endif

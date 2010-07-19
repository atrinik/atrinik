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
 * Socket initialization related code. */

#include <global.h>

#ifndef WIN32
#	include <sys/types.h>
#	include <sys/time.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <netinet/tcp.h>
#	include <netdb.h>
#	include <sys/stat.h>
#	include <stdio.h>
#endif

#ifdef HAVE_ARPA_INET_H
#	include <arpa/inet.h>
#endif

#include <newserver.h>
#include "zlib.h"

/** All the server/client files. */
_srv_client_files SrvClientFiles[SRV_CLIENT_FILES];

/** Socket information. */
Socket_Info socket_info;
/** Established connections for clients not yet playing. */
socket_struct *init_sockets;

/**
 * Buffer size for sending socket data. */
static int send_bufsize = 24 * 1024;
/**
 * Buffer size for receiving socket data. */
static int read_bufsize = 8 * 1024;

/**
 * Initializes a connection - really, it just sets up the data structure,
 * socket setup is handled elsewhere.
 *
 * Sends server version to the client.
 * @param ns Client's socket.
 * @param from Where the connection is coming from.
 * @todo Remove version sending legacy support for older clients at some
 * point. */
void init_connection(socket_struct *ns, const char *from_ip)
{
#ifdef WIN32
	u_long temp = 1;

	if (ioctlsocket(ns->fd, FIONBIO, &temp) == -1)
	{
		LOG(llevDebug, "init_connection(): Error on ioctlsocket.\n");
	}
#else
	if (fcntl(ns->fd, F_SETFL, fcntl(ns->fd, F_GETFL) | O_NDELAY | O_NONBLOCK ) == -1)
	{
		LOG(llevError, "init_connection(): Error on fcntl %x.\n", fcntl(ns->fd, F_GETFL));
	}
#endif

	ns->login_count = 0;
	ns->addme = 0;
	ns->faceset = 0;
	ns->sound = 0;
	ns->ext_title_flag = 1;
	ns->status = Ns_Add;
	ns->mapx = 17;
	ns->mapy = 17;
	ns->mapx_2 = 8;
	ns->mapy_2 = 8;
	ns->version = 0;
	ns->setup = 0;
	ns->rf_settings = 0;
	ns->rf_skills = 0;
	ns->rf_spells = 0;
	ns->rf_anims = 0;
	ns->rf_hfiles = 0;
	ns->rf_bmaps = 0;
	ns->password_fails = 0;
	ns->is_bot = 0;

	ns->inbuf.len = 0;
	ns->inbuf.buf = malloc(MAXSOCKBUF_IN);
	ns->inbuf.buf[0] = '\0';

	ns->readbuf.len = 0;
	ns->readbuf.buf = malloc(MAXSOCKBUF_IN);
	ns->readbuf.buf[0] = '\0';

	ns->cmdbuf.len = 0;
	ns->cmdbuf.buf = malloc(MAXSOCKBUF);
	ns->cmdbuf.buf[0] = '\0';

	memset(&ns->lastmap, 0, sizeof(struct Map));
	ns->buffer_front = NULL;
	ns->buffer_back = NULL;

	ns->host = strdup_local(from_ip);

	/* Legacy support for older clients. */
	{
		unsigned char buf[256];
		SockList sl;

		strncpy((char *) buf, "X991017 991017 Atrinik Server", sizeof(buf) - 1);
		buf[0] = BINARY_CMD_VERSION;

		sl.buf = buf;
		sl.len = strlen((char *) buf);
		Send_With_Handling(ns, &sl);
	}

#if CS_LOGSTATS
	if (socket_info.nconns > cst_tot.max_conn)
	{
		cst_tot.max_conn = socket_info.nconns;
	}

	if (socket_info.nconns > cst_lst.max_conn)
	{
		cst_lst.max_conn = socket_info.nconns;
	}
#endif
}

/**
 * Set various socket options on the specified file descriptor.
 * @param fd File descriptor to set the options for. */
static void setsockopts(int fd)
{
	struct linger linger_opt;
	int tmp = 1;
#ifdef WIN32
	u_long tmp2 = 1;

	if (ioctlsocket(fd, FIONBIO, &tmp2) == -1)
	{
		LOG(llevDebug, "setsockopts(): Error on ioctlsocket.\n");
	}
#else
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NDELAY | O_NONBLOCK ) == -1)
	{
		LOG(llevError, "setsockopts(): Error on fcntl %x.\n", fcntl(fd, F_GETFL));
	}
#endif

	/* Turn LINGER off (don't send left data in background if socket gets closed) */
	linger_opt.l_onoff = 0;
	linger_opt.l_linger = 0;

	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *) &linger_opt, sizeof(struct linger)))
	{
		LOG(llevError, "ERROR: setsockopts(): Error on setsockopt LINGER\n");
	}

	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &tmp, sizeof(tmp)))
	{
		LOG(llevError, "ERROR: setsockopts(): Error on setsockopt TCP_NODELAY\n");
	}

	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char *) &send_bufsize, sizeof(send_bufsize)))
	{
		LOG(llevError, "ERROR: setsockopts(): Error on setsockopt SO_SNDBUF\n");
	}

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char *) &read_bufsize, sizeof(read_bufsize)))
	{
		LOG(llevError, "ERROR: setsockopts(): Error on setsockopt SO_RCVBUF\n");
	}

	/* Would be nice to have an autoconf check for this. It appears that
	 * these functions are both using the same calling syntax, just one
	 * of them needs extra valus passed. */
#if !defined(_WEIRD_OS_)
	{
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &tmp, sizeof(tmp)))
		{
			LOG(llevError, "ERROR: setsockopts(): Error on setsockopt REUSEADDR\n");
		}
	}
#else
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) NULL, 0))
	{
		LOG(llevError, "ERROR: setsockopts(): Error on setsockopt REUSEADDR\n");
	}
#endif
}

/**
 * This sets up the socket and reads all the image information into
 * memory. */
void init_ericserver()
{
	int oldbufsize;
	socklen_t buflen = sizeof(int);
	struct sockaddr_in insock;
#ifndef WIN32
	struct protoent *protox;

#	ifdef HAVE_SYSCONF
	socket_info.max_filedescriptor = sysconf(_SC_OPEN_MAX);
#	else
#		ifdef HAVE_GETDTABLESIZE
	socket_info.max_filedescriptor = getdtablesize();
#		else
	"Unable to find usable function to get max filedescriptors";
#		endif
#	endif
#else
	WSADATA w;

	/* Used in select, ignored in winsockets */
	socket_info.max_filedescriptor = 1;
	/* This sets up all socket stuff */
	WSAStartup(0x0101, &w);
#endif

	socket_info.timeout.tv_sec = 0;
	socket_info.timeout.tv_usec = 0;
	socket_info.nconns = 0;

#if CS_LOGSTATS
	memset(&cst_tot, 0, sizeof(CS_Stats));
	memset(&cst_lst, 0, sizeof(CS_Stats));
	cst_tot.time_start = time(NULL);
	cst_lst.time_start = time(NULL);
#endif

	LOG(llevDebug, "Initialize new client/server data\n");
	socket_info.nconns = 1;
	init_sockets = malloc(sizeof(socket_struct));
	socket_info.allocated_sockets = 1;

#ifndef WIN32
	protox = getprotobyname("tcp");

	if (protox == NULL)
	{
		LOG(llevBug, "BUG: init_ericserver: Error getting protox\n");
		return;
	}

	init_sockets[0].fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);

#else
	init_sockets[0].fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif

	if (init_sockets[0].fd == -1)
	{
		LOG(llevError, "ERROR: Cannot create socket: %s\n", strerror_local(errno));
	}

	insock.sin_family = AF_INET;
	insock.sin_port = htons(settings.csport);
	insock.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopts(init_sockets[0].fd);
	getsockopt(init_sockets[0].fd, SOL_SOCKET, SO_SNDBUF, (char *) &oldbufsize, &buflen);

	if (oldbufsize > send_bufsize)
	{
		send_bufsize = (int) ((float) send_bufsize * ((float) send_bufsize / (float) oldbufsize));

		if (setsockopt(init_sockets[0].fd, SOL_SOCKET, SO_SNDBUF, (char *) &send_bufsize, sizeof(send_bufsize)))
		{
			LOG(llevError, "ERROR(): init_ericserver(): Error on setsockopt SO_SNDBUF\n");
		}

		LOG(llevDebug, "init_ericserver(): Send buffer adjusted to %d bytes! (old value: %d bytes)\n", send_bufsize, oldbufsize);
	}

	getsockopt(init_sockets[0].fd, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, &buflen);

	if (oldbufsize > read_bufsize)
	{
		read_bufsize = (int) ((float) read_bufsize * ((float) read_bufsize / (float) oldbufsize));

		if (setsockopt(init_sockets[0].fd, SOL_SOCKET, SO_RCVBUF, (char *) &read_bufsize, sizeof(read_bufsize)))
		{
			LOG(llevError, "ERROR(): init_ericserver(): Error on setsockopt SO_RCVBUF\n");
		}

		LOG(llevDebug, "init_ericserver(): Read buffer adjusted to %d bytes! (old value: %d bytes)\n", read_bufsize, oldbufsize);
	}

	if (bind(init_sockets[0].fd, (struct sockaddr *) &insock, sizeof(insock)) == -1)
	{
#ifndef WIN32
		close(init_sockets[0].fd);
#else
		shutdown(init_sockets[0].fd, SD_BOTH);
		closesocket(init_sockets[0].fd);
#endif
		LOG(llevError, "Cannot bind socket to port %d: %s\n", ntohs(insock.sin_port), strerror_local(errno));
	}

	if (listen(init_sockets[0].fd, 5) == -1)
	{
#ifndef WIN32
		close(init_sockets[0].fd);
#else
		shutdown(init_sockets[0].fd, SD_BOTH);
		closesocket(init_sockets[0].fd);
#endif
		LOG(llevError, "Cannot listen on socket: %s\n", strerror_local(errno));
	}

	init_sockets[0].status = Ns_Wait;
	read_client_images();
	updates_init();
	init_srv_files();
}

/**
 * Frees all the memory that ericserver allocates. */
void free_all_newserver()
{
	LOG(llevDebug, "Freeing all new client/server information.\n");

	free_socket_images();
	free(init_sockets);
}

/**
 * Basically, all we need to do here is free all data structures that
 * might be associated with the socket.
 *
 * It is up to the called to update the list.
 * @param ns The socket. */
void free_newsocket(socket_struct *ns)
{
#ifndef WIN32
	if (close(ns->fd))
#else
	shutdown(ns->fd, SD_BOTH);
	if (closesocket(ns->fd))
#endif
	{
#ifdef ESRV_DEBUG
		LOG(llevDebug, "Error closing socket %d\n", ns->fd);
#endif
	}

	if (ns->host)
	{
		free(ns->host);
	}

	if (ns->inbuf.buf)
	{
		free(ns->inbuf.buf);
	}

	if (ns->readbuf.buf)
	{
		free(ns->readbuf.buf);
	}

	if (ns->cmdbuf.buf)
	{
		free(ns->cmdbuf.buf);
	}

	socket_buffer_clear(ns);

	memset(ns, 0, sizeof(ns));
}

/**
 * Load server file.
 * @param fname Filename of the server file.
 * @param id ID of the server file.
 * @param cmd The data command. */
static void load_srv_file(char *fname, int id)
{
	FILE *fp;
	char *contents, *compressed;
	size_t fsize, numread;
	struct stat statbuf;

	LOG(llevDebug, "Loading %s...", fname);

	if ((fp = fopen(fname, "rb")) == NULL)
	{
		LOG(llevError, "\nERROR: Can't open file %s\n", fname);
	}

	fstat(fileno(fp), &statbuf);
	fsize = statbuf.st_size;
	/* Allocate a buffer to hold the whole file. */
	contents = malloc(fsize);

	if (!contents)
	{
		LOG(llevError, "ERROR: load_srv_file(): Out of memory.\n");
	}

	numread = fread(contents, 1, fsize, fp);
	fclose(fp);

	/* Get a crc from the uncompressed file. */
	SrvClientFiles[id].crc = crc32(1L, (const unsigned char FAR *) contents, numread);
	/* Store uncompressed length. */
	SrvClientFiles[id].len_ucomp = numread;

	/* Calculate the upper bound of the compressed size. */
	numread = compressBound(fsize);
	/* Allocate a buffer to hold the compressed file. */
	compressed = malloc(numread);

	if (!compressed)
	{
		LOG(llevError, "ERROR: load_srv_file(): Out of memory.\n");
	}

	compress2((Bytef *) compressed, (uLong *) &numread, (const unsigned char FAR *) contents, fsize, Z_BEST_COMPRESSION);
	SrvClientFiles[id].file = malloc(numread);

	if (!SrvClientFiles[id].file)
	{
		LOG(llevError, "ERROR: load_srv_file(): Out of memory.\n");
	}

	memcpy(SrvClientFiles[id].file, compressed, numread);
	SrvClientFiles[id].len = numread;

	/* Free temporary buffers. */
	free(contents);
	free(compressed);

	LOG(llevDebug, "(size: %"FMT64U" (%"FMT64U") (crc uncomp.: %lx)\n", (uint64) SrvClientFiles[id].len_ucomp, (uint64) numread, SrvClientFiles[id].crc);
}

/**
 * Get the lib/settings default file and create the data/client_settings
 * file from it. */
static void create_client_settings()
{
	char buf[MAX_BUF * 4];
	int i;
	FILE *fset_default, *fset_create;

	LOG(llevDebug, "Creating %s/client_settings...\n", settings.localdir);

	snprintf(buf, sizeof(buf), "%s/client_settings", settings.datadir);

	/* Open default */
	if ((fset_default = fopen(buf, "rb")) == NULL)
	{
		LOG(llevError, "\nERROR: Can not open file %s\n", buf);
	}

	/* Delete our target - we create it new now */
	snprintf(buf, sizeof(buf), "%s/client_settings", settings.localdir);
	unlink(buf);

	/* Open target client_settings */
	if ((fset_create = fopen(buf, "wb")) == NULL)
	{
		fclose(fset_default);
		LOG(llevError, "\nERROR: Can not open file %s\n", buf);
	}

	/* Copy default to target */
	while (fgets(buf, MAX_BUF, fset_default) != NULL)
	{
		fputs(buf, fset_create);
	}

	fclose(fset_default);

	/* Now add the level information */
	snprintf(buf, sizeof(buf), "level %d\n", MAXLEVEL);
	fputs(buf, fset_create);

	for (i = 0; i <= MAXLEVEL; i++)
	{
		snprintf(buf, sizeof(buf), "%"FMT64HEX"\n", new_levels[i]);
		fputs(buf, fset_create);
	}

	fclose(fset_create);
}

/**
 * Load all the server files we can send to client.
 *
 * client_bmaps is generated from the server at startup out of the
 * Atrinik png file. */
void init_srv_files()
{
	char buf[MAX_BUF];

	memset(&SrvClientFiles, 0, sizeof(SrvClientFiles));

	snprintf(buf, sizeof(buf), "%s/hfiles", settings.datadir);
	load_srv_file(buf, SRV_CLIENT_HFILES);

	snprintf(buf, sizeof(buf), "%s/animations", settings.datadir);
	load_srv_file(buf, SRV_CLIENT_ANIMS);

	snprintf(buf, sizeof(buf), "%s/client_bmaps", settings.localdir);
	load_srv_file(buf, SRV_CLIENT_BMAPS);

	snprintf(buf, sizeof(buf), "%s/client_skills", settings.datadir);
	load_srv_file(buf, SRV_CLIENT_SKILLS);

	snprintf(buf, sizeof(buf), "%s/client_spells", settings.datadir);
	load_srv_file(buf, SRV_CLIENT_SPELLS);

	create_client_settings();

	snprintf(buf, sizeof(buf), "%s/client_settings", settings.localdir);
	load_srv_file(buf, SRV_CLIENT_SETTINGS);

	snprintf(buf, sizeof(buf), "%s/%s", settings.localdir, UPDATES_FILE_NAME);
	load_srv_file(buf, SRV_FILE_UPDATES);
}

/**
 * Free all server files previously initialized by init_srv_files(). */
void free_srv_files()
{
	int i;

	LOG(llevDebug, "Freeing server/client files.\n");

	for (i = 0; i < SRV_CLIENT_FILES; i++)
	{
		free(SrvClientFiles[i].file);
	}
}

/**
 * A connecting client has requested a server file.
 *
 * Note that we don't know anything about the player at this point - we
 * got an open socket, an IP, a matching version, and an usable setup
 * string from the client.
 * @param ns The client's socket.
 * @param id ID of the server file. */
void send_srv_file(socket_struct *ns, int id)
{
	SockList sl;

	/* 1 byte for the command type, 1 byte for the srv file type,
	 * 4 bytes for original uncompressed length. */
	sl.buf = malloc(SrvClientFiles[id].len + 6);

	SOCKET_SET_BINARY_CMD(&sl, BINARY_CMD_DATA);

	if (ns->socket_version < 1036)
	{
		SockList_AddChar(&sl, (char) (id + 1) | DATA_PACKED_CMD);
	}
	else
	{
	SockList_AddChar(&sl, (char) id);
	SockList_AddInt(&sl, SrvClientFiles[id].len_ucomp);
	}

	memcpy(sl.buf + sl.len, SrvClientFiles[id].file, SrvClientFiles[id].len);
	sl.len += SrvClientFiles[id].len;

	Send_With_Handling(ns, &sl);
	free(sl.buf);
}

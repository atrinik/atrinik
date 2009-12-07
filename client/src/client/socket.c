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

#include <include.h>

int SOCKET_GetError()
{
#ifdef __WIN_32
	return WSAGetLastError();
#elif __LINUX
	return errno;
#endif
}

/* this readsfrom fd and puts the data in sl.  We return true if we think
 * we have a full packet, 0 if we have a partial packet.  The only processing
 * we do is remove the intial size value.  len (As passed) is the size of the
 * buffer allocated in the socklist.  We make the assumption the buffer is
 * at least 2 bytes long. */
int read_socket(int fd, SockList *sl, int len)
{
	int stat, toread, readsome = 0;
	extern int errno;

	/* We already have a partial packet */
	if (sl->len < 3)
	{
#ifdef __WIN_32
		stat = recv(fd, sl->buf + sl->len, 3 - sl->len, 0);
#elif __LINUX
		do
		{
			stat = read(fd, sl->buf + sl->len, 3 - sl->len);
		}
		while ((stat == -1) && (errno == EINTR));
#endif

		if (stat < 0)
		{
#ifdef __WIN_32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
#elif __LINUX
			/* In non blocking mode, EAGAIN is set when there is no
			 * data available. */
			if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
			{
#ifdef __WIN_32
				LOG(LOG_DEBUG, "ReadPacket got error %d, returning -1\n", WSAGetLastError());
#elif __LINUX
				LOG(LOG_DEBUG, "ReadPacket got error %d, returning -1", errno);
#endif

				draw_info("WARNING: Lost or bad server connection.", COLOR_RED);

				return -1;
			}

			return 0;
		}

		if (stat == 0)
		{
			draw_info("WARNING: Server read package error.", COLOR_RED);

			return -1;
		}

		sl->len += stat;

		/* Still don't have a full packet */
		if (sl->len < 3)
			return 0;

		readsome = 1;
	}

	/* Figure out how much more data we need to read.  Add 2 from the
	 * end of this - size header information is not included. */

	/* High bit set = 3 bytes size header */
	if (sl->buf[0] & 0x80)
	{
		toread = 3 + ((sl->buf[0] & 0x7f) << 16) + (sl->buf[1] << 8) + sl->buf[2] - sl->len;
	}
	else
	{
		toread = 2 + (sl->buf[0] << 8) + sl->buf[1] - sl->len;
	}

	if (toread == 0)
		return 1;

	if ((toread + sl->len) > len)
	{
		draw_info("WARNING: Server closed connection.", COLOR_RED);
		LOG(LOG_ERROR, "SockList_ReadPacket: Want to read more bytes than will fit in buffer.\n");

		/* Return error so the socket is closed */
		return -1;
	}

	do
	{
#ifdef __WIN_32
		stat = recv(fd, sl->buf + sl->len, toread, 0);
#elif __LINUX
		do
		{
			stat = read(fd, sl->buf + sl->len, toread);
		}
		while ((stat < 0) && (errno == EINTR));
#endif

		if (stat < 0)
		{
#ifdef __WIN_32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
#elif __LINUX
			if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
			{
#ifdef __WIN_32
				LOG(LOG_DEBUG, "ReadPacket got error %d, returning 0", WSAGetLastError());
#elif __LINUX
				LOG(LOG_DEBUG, "ReadPacket got error %d, returning 0", errno);
#endif

				draw_info("WARNING: Lost or bad server connection.", COLOR_RED);

				return -1;
			}

			return 0;
		}

		if (stat == 0)
		{
			draw_info("WARNING: Server read package error.", COLOR_RED);

			return -1;
		}

		sl->len += stat;
		toread -= stat;

		if (toread == 0)
			return 1;

		if (toread < 0)
		{
			LOG(LOG_ERROR, "SockList_ReadPacket: Read more bytes than desired.");
			draw_info("WARNING: Server read package error.", COLOR_RED);

			return -1;
		}
	}
	while (toread > 0);

	return 0;
}

/* This writes data to the socket.  we precede the len information on the
 * packet.  Len needs to be passed here because buf could be binary
 * data */
int write_socket(int fd, unsigned char *buf, int len)
{
	int amt = 0;
	unsigned char *pos = buf;

	/* If we manage to write more than we wanted, take it as a bonus */
	while (len > 0)
	{
#ifdef __WIN_32
		amt = send(fd, pos, len, 0);
#elif __LINUX
		do
		{
			amt = write(fd, pos, len);
		}
		while ((amt < 0) && (errno == EINTR));
#endif

#ifdef __WIN_32
		if (amt == -1 && WSAGetLastError() != WSAEWOULDBLOCK)
#elif __LINUX
		if (amt < 0)
#endif
		{
			/* We got an error */
#ifdef __WIN_32
			LOG(LOG_ERROR, "New socket write failed (wsb) (%d).\n", WSAGetLastError());
#elif __LINUX
			LOG(LOG_ERROR, "New socket (fd=%d) write failed.\n", fd);
#endif

			draw_info("SOCKET ERROR: Server write failed.", COLOR_RED);

			SOCKET_CloseSocket(csocket.fd);

			return -1;
		}

		if (amt == 0)
		{
#ifdef __WIN_32
			LOG(LOG_ERROR, "Write_To_Socket: No data written out (%d).\n", WSAGetLastError());
#elif __LINUX
			LOG(LOG_ERROR, "Write_To_Socket: No data written out.\n");
#endif

			draw_info("SOCKET ERROR: No data written out", COLOR_RED);

			SOCKET_CloseSocket(csocket.fd);

			return -1;
		}

		len -= amt;
		pos += amt;
	}

	return 0;
}

int SOCKET_InitSocket()
{
#ifdef WIN32
	WSADATA w;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int error;

	csocket.fd = SOCKET_NO;
	csocket.cs_version = 0;

	SocketStatusErrorNr = 0;
	error = WSAStartup(wVersionRequested, &w);

	if (error)
	{
		wVersionRequested = MAKEWORD(2, 0);
		error = WSAStartup(wVersionRequested, &w);

		if (error)
		{
			wVersionRequested = MAKEWORD(1, 1);
			error = WSAStartup(wVersionRequested, &w);

			if (error)
			{
				LOG(LOG_ERROR, "Error: Error init starting Winsock: %d!\n", error);
				return 0;
			}
		}
	}

	LOG(LOG_MSG, "Using socket version %x!\n", w.wVersion);
#endif

	return 1;
}

int SOCKET_DeinitSocket()
{
#ifdef __WIN_32
	/* Drop socket */
	WSACleanup();
#endif

	return 1;
}

int SOCKET_CloseSocket(SOCKET socket_temp)
{
	void *tmp_free;

	/* seems differents sockets have different way to shutdown connects??
	 * win32 needs this
	 * hard way, normally you should wait for a read() == 0... */
	if ((int) socket_temp == SOCKET_NO)
		return 1;

#ifdef __WIN_32
	shutdown(socket_temp, SD_BOTH);
	closesocket(socket_temp);
#elif __LINUX
	close(socket_temp);
#endif

	tmp_free = &csocket.inbuf.buf;
	FreeMemory(tmp_free);
	csocket.fd = SOCKET_NO;

	return 1;
}

int SOCKET_OpenSocket(SOCKET *socket_temp, struct ClientSocket *csock, char *host, int port)
{
#ifdef __WIN_32
	int error;
	long temp;
	uint32 start_timer;
#elif __LINUX
	struct protoent *protox;
	struct sockaddr_in insock;
#endif
	unsigned int oldbufsize, newbufsize = 65535, buflen = sizeof(int);

	printf("Opening to %s %i\n", host, port);

#ifdef __WIN_32
	/* The way to make the sockets work on XP Home - The 'unix' style socket
	 * seems to fail under xp home. */
	*socket_temp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	csocket.inbuf.buf = (unsigned char *) malloc(MAXSOCKBUF);
	csocket.inbuf.len = 0;
	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) port);

	if (ioctlsocket(*socket_temp, FIONBIO, &temp) == -1)
	{
		LOG(LOG_ERROR, "ERROR: ioctlsocket(*socket_temp, FIONBIO , &temp)\n");
		*socket_temp = SOCKET_NO;

		return 0;
	}
#elif __LINUX
	protox = getprotobyname("tcp");

	if (protox == (struct protoent *) NULL)
	{
		fprintf(stderr, "SOCKET_OpenSocket(): Error getting protobyname (tcp)\n");

		return 0;
	}

	*socket_temp = socket(PF_INET, SOCK_STREAM, protox->p_proto);

	if (*socket_temp == -1)
	{
		perror("SOCKET_OpenSocket(): Error on socket command.\n");
		*socket_temp = SOCKET_NO;

		return 0;
	}

	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) port);
#endif

	if (isdigit(*host))
	{
		insock.sin_addr.s_addr = inet_addr(host);
	}
	else
	{
		struct hostent *hostbn = gethostbyname(host);

		if (hostbn == (struct hostent *) NULL)
		{
			LOG(LOG_ERROR, "Unknown host: %s\n", host);
			*socket_temp = SOCKET_NO;

			return 0;
		}

		memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
	}

	csock->command_sent = 0;
	csock->command_received = 0;
	csock->command_time = 0;

#ifdef __WIN_32
	/* non-block */
	temp = 1;

	if (ioctlsocket(*socket_temp, FIONBIO, &temp) == -1)
	{
		LOG(LOG_ERROR, "ERROR: ioctlsocket(*socket_temp, FIONBIO , &temp)\n");
		*socket_temp = SOCKET_NO;

		return 0;
	}

	error = 0;

	start_timer = SDL_GetTicks();

	while (connect(*socket_temp, (struct sockaddr *) &insock, sizeof(insock)) == SOCKET_ERROR)
	{
		SDL_Delay(3);

		/* Timeout... Without connect will REALLY hang a long time */
		if (start_timer + SOCKET_TIMEOUT_SEC * 1000 < SDL_GetTicks())
		{
			*socket_temp = SOCKET_NO;

			return 0;
		}

		SocketStatusErrorNr = WSAGetLastError();

		/* we have a connect! */
		if (SocketStatusErrorNr == WSAEISCONN)
			break;

		/* loop until we finished */
		if (SocketStatusErrorNr == WSAEWOULDBLOCK || SocketStatusErrorNr == WSAEALREADY || (SocketStatusErrorNr == WSAEINVAL && error))
		{
			error = 1;

			continue;
		}

		LOG(LOG_MSG, "Connect Error: %d\n", SocketStatusErrorNr);
		*socket_temp = SOCKET_NO;

		return 0;
	}
#elif __LINUX
	csocket.inbuf.buf = (unsigned char *) _malloc(MAXSOCKBUF, "SOCKET_OpenSocket(): MAXSOCKBUF");
	csocket.inbuf.len = 0;

	if (connect(*socket_temp, (struct sockaddr *) &insock, sizeof(insock)) == -1)
	{
		perror("Can't connect to server\n");

		return 0;
	}

	if (fcntl(*socket_temp, F_SETFL, O_NDELAY) == -1)
	{
		fprintf(stderr, "InitConnection:  Error on fcntl.\n");
	}
#endif

	if (getsockopt(*socket_temp, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, &buflen) == -1)
	{
		oldbufsize = 0;
	}

	if (oldbufsize < newbufsize)
	{
		if (setsockopt(*socket_temp, SOL_SOCKET, SO_RCVBUF, (char *) &newbufsize, sizeof(&newbufsize)))
		{
			LOG(1, "InitConnection: setsockopt unable to set output buf size to %d\n", newbufsize);
			setsockopt(*socket_temp, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, sizeof(&oldbufsize));
		}
	}

	LOG(LOG_MSG, "Connected to %s:%d\n", host, port);

	return 1;
}

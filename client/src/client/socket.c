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
 * Socket related code. */

#include <include.h>

/**
 * Get error number.
 * @return The error number. */
int socket_get_error()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

/**
 * Reads from fd and puts the data in sl.
 *
 * The only processing we do is remove the initial size value.
 *
 * We make the assumption the buffer is at least 3 bytes long.
 * @param fd File descriptor to read from.
 * @param sl Socket list to put the data in.
 * @param len Maximum length of data we can store.
 * @retval 1 We think we have a full packet.
 * @retval 0 We have a partial packet.
 * @retval -1 Something went wrong with reading the socket. */
int socket_read(int fd, SockList *sl, int len)
{
	int stat, toread, readsome = 0;

	/* We already have a partial packet */
	if (sl->len < 3)
	{
#ifdef WIN32
		stat = recv(fd, sl->buf + sl->len, 3 - sl->len, 0);
#else
		do
		{
			stat = read(fd, sl->buf + sl->len, 3 - sl->len);
		}
		while ((stat == -1) && (errno == EINTR));
#endif

		if (stat < 0)
		{
#ifdef WIN32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
#else
			/* In non blocking mode, EAGAIN is set when there is no
			 * data available. */
			if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
			{
				LOG(llevDebug, "socket_read(): Got error %d, returning -1\n", socket_get_error());
				draw_info("Lost or bad server connection.", COLOR_RED);
				return -1;
			}

			return 0;
		}

		if (stat == 0)
		{
			draw_info("Server closed connection.", COLOR_RED);
			return -1;
		}

		sl->len += stat;

		/* Still don't have a full packet */
		if (sl->len < 3)
		{
			return 0;
		}

		readsome = 1;
	}

	/* Figure out how much more data we need to read. Add 2 from the end
	 * of this - size header information is not included. */

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
	{
		return 1;
	}

	if ((toread + sl->len) > len)
	{
		LOG(llevError, "socket_read(): Want to read more bytes than will fit in buffer.\n");
		draw_info("Server closed connection.", COLOR_RED);
		return -1;
	}

	do
	{
#ifdef WIN32
		stat = recv(fd, sl->buf + sl->len, toread, 0);
#else
		do
		{
			stat = read(fd, sl->buf + sl->len, toread);
		}
		while ((stat < 0) && (errno == EINTR));
#endif

		if (stat < 0)
		{
#ifdef WIN32
			if ((stat == -1) && WSAGetLastError() != WSAEWOULDBLOCK)
#else
			if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
			{
				LOG(llevDebug, "socket_read(): Got error %d, returning 0", socket_get_error());
				draw_info("Lost or bad server connection.", COLOR_RED);
				return -1;
			}

			return 0;
		}

		if (stat == 0)
		{
			draw_info("Server closed connection.", COLOR_RED);
			return -1;
		}

		sl->len += stat;
		toread -= stat;

		if (toread == 0)
		{
			return 1;
		}

		if (toread < 0)
		{
			LOG(llevError, "socket_read(): Read more bytes than desired.");
			draw_info("Server closed connection.", COLOR_RED);
			return -1;
		}
	}
	while (toread > 0);

	return 0;
}

/**
 * Writes data to the socket.
 *
 * We precede the len information on the packet. Len needs to be passed
 * here because buf could be binary data.
 * @param fd Socket's file descriptor.
 * @param buf Data to send.
 * @param len Length of buf.
 * @retval 0 Success.
 * @retval -1 Failed to write. */
int socket_write(int fd, unsigned char *buf, int len)
{
	int amt = 0;
	unsigned char *pos = buf;

	/* If we manage to write more than we wanted, take it as a bonus. */
	while (len > 0)
	{
#ifdef WIN32
		amt = send(fd, pos, len, 0);
#else
		do
		{
			amt = write(fd, pos, len);
		}
		while ((amt < 0) && (errno == EINTR));
#endif

#ifdef WIN32
		if (amt == -1 && WSAGetLastError() != WSAEWOULDBLOCK)
#else
		if (amt < 0)
#endif
		{
			/* We got an error */
			LOG(llevError, "socket_write(): Write failed with error %d.\n", socket_get_error());
			draw_info("Server write failed.", COLOR_RED);
			socket_close(csocket.fd);
			return -1;
		}

		if (amt == 0)
		{
			LOG(llevError, "socket_write(): No data written out: %d.\n", socket_get_error());
			draw_info("No data written out.", COLOR_RED);
			socket_close(csocket.fd);
			return -1;
		}

		len -= amt;
		pos += amt;
	}

	return 0;
}

/**
 * Initialize the socket.
 * @return 1 on success, 0 on failure. */
int socket_initialize()
{
#ifdef WIN32
	WSADATA w;
	WORD wVersionRequested = MAKEWORD(2, 2);
	int error;

	csocket.fd = SOCKET_NO;

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
				LOG(llevError, "Error: Error init starting Winsock: %d!\n", error);
				return 0;
			}
		}
	}

	LOG(llevMsg, "Using socket version %x!\n", w.wVersion);
#endif

	return 1;
}

/**
 * Deinitialize the socket. */
void socket_deinitialize()
{
#ifdef WIN32
	/* Drop socket */
	WSACleanup();
#endif
}

/**
 * Close a socket.
 * @param socket_temp Socket to close. */
void socket_close(SOCKET socket_temp)
{
	void *tmp_free;

	if ((int) socket_temp == SOCKET_NO)
	{
		return;
	}

#ifdef WIN32
	shutdown(socket_temp, SD_BOTH);
	closesocket(socket_temp);
#else
	close(socket_temp);
#endif

	tmp_free = &csocket.inbuf.buf;
	FreeMemory(tmp_free);
	csocket.fd = SOCKET_NO;
}

/**
 * Open a new socket.
 * @param socket_temp Socket to open.
 * @param csock Client socket.
 * @param host Host to connect to.
 * @param port Port to connect to.
 * @return 1 on success, 0 on failure. */
int open_socket(SOCKET *socket_temp, struct ClientSocket *csock, char *host, int port)
{
#ifdef WIN32
	int error;
	long temp;
	uint32 start_timer;
	int SocketStatusErrorNr;
#else
	struct protoent *protox;
	struct sockaddr_in insock;
#endif
	unsigned int oldbufsize, newbufsize = 65535, buflen = sizeof(int);

	LOG(llevMsg, "Opening to %s %d\n", host, port);

#ifdef WIN32
	/* The way to make the sockets work on XP Home - The 'unix' style socket
	 * seems to fail under xp home. */
	*socket_temp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	csocket.inbuf.buf = (unsigned char *) malloc(MAXSOCKBUF);
	csocket.inbuf.len = 0;
	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) port);

	if (ioctlsocket(*socket_temp, FIONBIO, &temp) == -1)
	{
		LOG(llevError, "ERROR: ioctlsocket(*socket_temp, FIONBIO , &temp)\n");
		*socket_temp = SOCKET_NO;

		return 0;
	}
#else
	protox = getprotobyname("tcp");

	if (protox == (struct protoent *) NULL)
	{
		LOG(llevError, "open_socket(): Error getting protobyname (tcp)\n");
		return 0;
	}

	*socket_temp = socket(PF_INET, SOCK_STREAM, protox->p_proto);

	if (*socket_temp == -1)
	{
		perror("open_socket(): Error on socket command");
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
			LOG(llevError, "Unknown host: %s\n", host);
			*socket_temp = SOCKET_NO;
			return 0;
		}

		memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
	}

	csock->command_sent = 0;
	csock->command_received = 0;
	csock->command_time = 0;

#ifdef WIN32
	/* non-block */
	temp = 1;

	if (ioctlsocket(*socket_temp, FIONBIO, &temp) == -1)
	{
		LOG(llevError, "ERROR: ioctlsocket(*socket_temp, FIONBIO , &temp)\n");
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
		{
			break;
		}

		/* Loop until we finished */
		if (SocketStatusErrorNr == WSAEWOULDBLOCK || SocketStatusErrorNr == WSAEALREADY || (SocketStatusErrorNr == WSAEINVAL && error))
		{
			error = 1;
			continue;
		}

		LOG(llevMsg, "Connect Error: %d\n", SocketStatusErrorNr);
		*socket_temp = SOCKET_NO;

		return 0;
	}
#else
	csocket.inbuf.buf = (unsigned char *) malloc(MAXSOCKBUF);
	csocket.inbuf.len = 0;

	if (connect(*socket_temp, (struct sockaddr *) &insock, sizeof(insock)) == -1)
	{
		perror("Can't connect to server");
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

	LOG(llevMsg, "Connected to %s:%d\n", host, port);

	return 1;
}

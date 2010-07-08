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

static SDL_Thread *input_thread;
static SDL_mutex *input_buffer_mutex;
static SDL_cond *input_buffer_cond;

static SDL_Thread *output_thread;
static SDL_mutex *output_buffer_mutex;
static SDL_cond *output_buffer_cond;

/**
 * Mutex to protect socket deinitialization. */
static SDL_mutex *socket_mutex;

/**
 * All socket threads will exit if they see this flag set. */
static int abort_thread = 0;

/* start is the first waiting item in queue, end is the most recent enqueued */
static command_buffer *input_queue_start = NULL, *input_queue_end = NULL;
static command_buffer *output_queue_start = NULL, *output_queue_end = NULL;

/**
 * Create a new command buffer of the given size, copying the data buffer
 * if not NULL. The buffer will always be null-terminated for safety (and
 * one byte larger than requested).
 * @param len Requested buffer size in bytes.
 * @param data Buffer data to copy (len bytes), or NULL.
 * @return A new command buffer or NULL in case of an error. */
static command_buffer *command_buffer_new(size_t len, uint8 *data)
{
	command_buffer *buf = (command_buffer *) malloc(sizeof(command_buffer) + len + 1);

	buf->next = buf->prev = NULL;
	buf->len = len;

	if (data)
	{
		memcpy(buf->data, data, len);
	}

	buf->data[len] = '\0';
	return buf;
}

/**
 * Free all memory related to a single command buffer.
 * @param buf Buffer to free. */
void command_buffer_free(command_buffer *buf)
{
	free(buf);
}

/**
 * Enqueue a command buffer last in a queue. */
static void command_buffer_enqueue(command_buffer *buf, command_buffer **queue_start, command_buffer **queue_end)
{
	buf->next = NULL;
	buf->prev = *queue_end;

	if (*queue_start == NULL)
	{
		*queue_start = buf;
	}

	if (buf->prev)
	{
		buf->prev->next = buf;
	}

	*queue_end = buf;
}

/**
 * Remove the first command buffer from a queue. */
static command_buffer *command_buffer_dequeue(command_buffer **queue_start, command_buffer **queue_end)
{
	command_buffer *buf = *queue_start;

	if (buf)
	{
		*queue_start = buf->next;

		if (buf->next)
		{
			buf->next->prev = NULL;
		}
		else
		{
			*queue_end = NULL;
		}
	}

	return buf;
}

/**
 * Add a binary command to the output buffer.
 * If body is NULL, a single-byte command is created from cmd.
 * Otherwise body should include the length and cmd header. */
int send_command_binary(uint8 cmd, uint8 *body, unsigned int len)
{
	command_buffer *buf;

	if (body)
	{
		buf = command_buffer_new(len, body);
	}
	else
	{
		uint8 tmp[3];

		len = 0x8001;
		/* Packet order is obviously big-endian for length data. */
		tmp[0] = (len >> 8) & 0xFF;
		tmp[1] = len & 0xFF;
		tmp[2] = cmd;
		buf = command_buffer_new(3, tmp);
	}

	if (!buf)
	{
		socket_close(&csocket);
		return -1;
	}

	SDL_LockMutex(output_buffer_mutex);
	command_buffer_enqueue(buf, &output_queue_start, &output_queue_end);
	SDL_CondSignal(output_buffer_cond);
	SDL_UnlockMutex(output_buffer_mutex);

	return 0;
}

/**
 * Move a command buffer to the out buffer so it can be written to the socket. */
int send_socklist(SockList msg)
{
	command_buffer *buf = command_buffer_new(msg.len + 2, NULL);

	if (!buf)
	{
		socket_close(&csocket);
		return -1;
	}

	memcpy(buf->data + 2, msg.buf, msg.len);

	buf->data[0] = (uint8) ((msg.len >> 8) & 0xFF);
	buf->data[1] = ((uint32) (msg.len)) & 0xFF;

	SDL_LockMutex(output_buffer_mutex);
	command_buffer_enqueue(buf, &output_queue_start, &output_queue_end);
	SDL_CondSignal(output_buffer_cond);
	SDL_UnlockMutex(output_buffer_mutex);

	return 0;
}

/**
 * Get a command from the queue.
 * @return The command (being removed from queue), NULL if there is no
 * command. */
command_buffer *get_next_input_command()
{
	command_buffer *buf;

	SDL_LockMutex(input_buffer_mutex);
	buf = command_buffer_dequeue(&input_queue_start, &input_queue_end);
	SDL_UnlockMutex(input_buffer_mutex);
	return buf;
}

static int reader_thread_loop(void *dummy)
{
	static uint8 *readbuf = NULL;
	static int readbuf_size = 256;
	int readbuf_len = 0;
	int header_len = 0;
	int cmd_len = -1;

	(void) dummy;
	LOG(llevDebug, "Reader thread started.\n");

	if (!readbuf)
	{
		readbuf = malloc(readbuf_size);
	}

	while (!abort_thread)
	{
		int ret;
		int toread;

		/* First, try to read a command length sequence */
		if (readbuf_len < 2)
		{
			/* Three-byte length? */
			if (readbuf_len > 0 && (readbuf[0] & 0x80))
			{
				toread = 3 - readbuf_len;
			}
			else
			{
				toread = 2 - readbuf_len;
			}
		}
		else if (readbuf_len == 2 && (readbuf[0] & 0x80))
		{
			toread = 1;
		}
		else
		{
			/* If we have a finished header, get the packet size from it. */
			if (readbuf_len <= 3)
			{
				uint8 *p = readbuf;

				header_len = (*p & 0x80) ? 3 : 2;
				cmd_len = 0;

				if (header_len == 3)
				{
					cmd_len += ((int) (*p++) & 0x7f) << 16;
				}

				cmd_len += ((int) (*p++)) << 8;
				cmd_len += ((int) (*p++));
			}

			toread = cmd_len + header_len - readbuf_len;

			if (readbuf_len + toread > readbuf_size)
			{
				uint8 *tmp = readbuf;

				readbuf_size = readbuf_len + toread;
				readbuf = (uint8 *) malloc(readbuf_size);
				memcpy(readbuf, tmp, readbuf_len);
				free(tmp);
			}
		}

		ret = recv(csocket.fd, (char *) readbuf + readbuf_len, toread, 0);

		/* End of file */
		if (ret == 0)
		{
			LOG(llevDebug, "Reader thread got EOF trying to read %d bytes.\n", toread);
			break;
		}
		else if (ret == -1)
		{
			/* IO error */
#ifdef WIN32
			LOG(llevDebug, "Reader thread got error %d\n", WSAGetLastError());
#else
			LOG(llevDebug, "Reader thread got error %d: %s\n", errno, strerror(errno));
#endif
			break;
		}
		else
		{
			readbuf_len += ret;
		}

		/* Finished with a command ? */
		if (readbuf_len == cmd_len + header_len && !abort_thread)
		{
			command_buffer *buf = command_buffer_new(readbuf_len - header_len, readbuf + header_len);

			if (!buf)
			{
				break;
			}

			SDL_LockMutex(input_buffer_mutex);
			command_buffer_enqueue(buf, &input_queue_start, &input_queue_end);
			SDL_CondSignal(input_buffer_cond);
			SDL_UnlockMutex(input_buffer_mutex);

			cmd_len = -1;
			header_len = 0;
			readbuf_len = 0;
		}
	}

	socket_close(&csocket);
	free(readbuf);
	readbuf = NULL;
	LOG(llevDebug, "Reader thread stopped.\n");
	return -1;
}

/**
 * Worker for the writer thread. It waits for enqueued outgoing packets
 * and sends them to the server as fast as it can.
 *
 * If any error is detected, the socket is closed and the thread exits. It is
 * up to them main thread to detect this and join() the worker threads. */
static int writer_thread_loop(void *dummy)
{
	command_buffer *buf = NULL;

	(void) dummy;
	LOG(llevDebug, "Writer thread started.\n");

	while (!abort_thread)
	{
		int written = 0;

		SDL_LockMutex(output_buffer_mutex);

		while (output_queue_start == NULL && !abort_thread)
		{
			SDL_CondWait(output_buffer_cond, output_buffer_mutex);
		}

		buf = command_buffer_dequeue(&output_queue_start, &output_queue_end);
		SDL_UnlockMutex(output_buffer_mutex);

		while (buf && written < buf->len && !abort_thread)
		{
			int ret = send(csocket.fd, (const char *) buf->data + written, buf->len - written, 0);

			if (ret == 0)
			{
				LOG(llevDebug, "Writer thread got EOF.\n");
				break;
			}
			else if (ret == -1)
			{
				/* IO error */
#ifdef WIN32
				LOG(llevDebug, "Writer thread got error %d\n", WSAGetLastError());
#else
				LOG(llevDebug, "Writer thread got error %d: %s\n", errno, strerror(errno));
#endif
				break;
			}
			else
			{
				written += ret;
			}
		}

		if (buf)
		{
			command_buffer_free(buf);
			buf = NULL;
		}
	}

	if (buf)
	{
		command_buffer_free(buf);
	}

	socket_close(&csocket);
	LOG(llevDebug, "Writer thread stopped.\n");
	return 0;
}

/**
 * Initialize and start up the worker threads. */
void socket_thread_start()
{
	LOG(llevMsg, "Starting socket threads.\n");

	if (input_buffer_cond == NULL)
	{
		input_buffer_cond = SDL_CreateCond();
		input_buffer_mutex = SDL_CreateMutex();
		output_buffer_cond = SDL_CreateCond();
		output_buffer_mutex = SDL_CreateMutex();
		socket_mutex = SDL_CreateMutex();
	}

	abort_thread = 0;

	input_thread = SDL_CreateThread(reader_thread_loop, NULL);

	if (input_thread == NULL)
	{
		LOG(llevError, "socket_thread_start(): Unable to start socket thread: %s\n", SDL_GetError());
	}

	output_thread = SDL_CreateThread(writer_thread_loop, NULL);

	if (output_thread == NULL)
	{
		LOG(llevError, "socket_thread_start(): Unable to start socket thread: %s\n", SDL_GetError());
	}
}

/**
 * Wait for the socket threads to finish.
 * Closes the socket first, if it hasn't already been done. */
void socket_thread_stop()
{
	LOG(llevMsg, "Stopping socket threads.\n");

	socket_close(&csocket);

	SDL_WaitThread(output_thread, NULL);
	SDL_WaitThread(input_thread, NULL);

	input_thread = output_thread = NULL;
}

/**
 * Detect and handle socket system shutdowns. Also reset the socket system
 * for a restart.
 *
 * The main thread should poll this function which detects connection
 * shutdowns and removes the threads if it happens. */
int handle_socket_shutdown()
{
	if (abort_thread)
	{
		socket_thread_stop();
		abort_thread = 0;

		/* Empty all queues */
		while (input_queue_start)
		{
			command_buffer_free(command_buffer_dequeue(&input_queue_start, &input_queue_end));
		}

		while (output_queue_start)
		{
			command_buffer_free(command_buffer_dequeue(&output_queue_start, &output_queue_end));
		}

		LOG(llevDebug, "Connection lost.\n");
		return 1;
	}

	return 0;
}

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
 * Close a socket.
 * @param csock Socket to close. */
int socket_close(struct ClientSocket *csock)
{
	SDL_LockMutex(socket_mutex);

	if ((int) csock->fd == SOCKET_NO)
	{
		SDL_UnlockMutex(socket_mutex);
		return 1;
	}

#ifdef __LINUX
	if (shutdown(csock->fd, SHUT_RDWR))
	{
		perror("shutdown");
	}

	if (close(csock->fd))
	{
		perror("close");
	}
#else
	shutdown(csock->fd, 2);
	closesocket(csock->fd);
#endif

	csock->fd = SOCKET_NO;

	abort_thread = 1;

	/* Poke anyone waiting at a cond */
	SDL_CondSignal(input_buffer_cond);
	SDL_CondSignal(output_buffer_cond);

	SDL_UnlockMutex(socket_mutex);

	return 1;
}

/**
 * Initialize the socket.
 * @return 1 on success, 0 on failure. */
int socket_initialize()
{
#ifdef WIN32
	WSADATA w;
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	int error;

	csocket.fd = SOCKET_NO;
	error = WSAStartup(wVersionRequested, &w);

	if (error)
	{
		wVersionRequested = MAKEWORD( 2, 0 );
		error = WSAStartup(wVersionRequested, &w);

		if (error)
		{
			wVersionRequested = MAKEWORD( 1, 1 );
			error = WSAStartup(wVersionRequested, &w);

			if (error)
			{
				LOG(llevError, "ERROR: socket_initialize(): Error initializing WinSock: %d.\n", error);
				return(0);
			}
		}
	}

	LOG(llevMsg, "Using socket version %x.\n", w.wVersion);
#endif
	return 1;
}

/**
 * Deinitialize the socket. */
void socket_deinitialize()
{
	if ((int) csocket.fd != SOCKET_NO)
	{
		socket_close(&csocket);
	}

#ifdef WIN32
	WSACleanup();
#endif
}

#ifndef WIN32

/**
 * Create a new socket.
 * @param[out] fd File descriptor we'll update.
 * @param host Host to connect to.
 * @param port Port to use.
 * @return 1 on success, 0 on failure. */
static int socket_create(SOCKET *fd, char *host, int port)
{
	unsigned int oldbufsize, newbufsize = 65535, buflen = sizeof(int);
	struct linger linger_opt;
	int flags;
	uint32 start_timer;

	/* Use new (getaddrinfo()) or old (gethostbyname()) socket API */
#ifndef HAVE_GETADDRINFO
	/* This method is preferable unless IPv6 is required, due to buggy distros. */
	struct protoent *protox;
	struct sockaddr_in insock;

	protox = getprotobyname("tcp");

	if (!protox)
	{
		LOG(llevError, "Error getting prorobyname (tcp)\n");
		return 0;
	}

	*fd = socket(PF_INET, SOCK_STREAM, protox->p_proto);

	if (*fd == SOCKET_NO)
	{
		perror("socket_create(): Error on socket command.\n");
		return 0;
	}

	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) port);

	if (isdigit(*host))
	{
		insock.sin_addr.s_addr = inet_addr(host);
	}
	else
	{
		struct hostent *hostbn = gethostbyname(host);

		if (!hostbn)
		{
			LOG(llevMsg, "Unknown host: %s\n", host);
			return 0;
		}

		memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
	}

	/* Set non-blocking. */
	flags = fcntl(*fd, F_GETFL);

	if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		LOG(llevError, "socket_create(): Error on switching to non-blocking. fcntl %x.\n", fcntl(*fd, F_GETFL));
		*fd = SOCKET_NO;
		return 0;
	}

	/* Try to connect. */
	start_timer = SDL_GetTicks();

	while (connect(*fd, (struct sockaddr *) &insock, sizeof(insock)) == -1)
	{
		SDL_Delay(3);

		if (start_timer + SOCKET_TIMEOUT_MS < SDL_GetTicks())
		{
			perror("Can't connect to server.");
			*fd = SOCKET_NO;
			return 0;
		}
	}

	/* Set back to blocking. */
	if (fcntl(*fd, F_SETFL, flags) == -1)
	{
		LOG(llevError, "socket_create(): Error on switching to blocking. fcntl %x.\n", fcntl(*fd, F_GETFL));
		*fd = SOCKET_NO;
		return 0;
	}
#else
	struct addrinfo hints;
	struct addrinfo *res = NULL, *ai;
	char port_str[6], hostaddr[40];

	snprintf(port_str, sizeof(port_str), "%d", port);
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;

	if (getaddrinfo(host, port_str, &hints, &res) != 0)
	{
		return 0;
	}

	for (ai = res; ai; ai = ai->ai_next)
	{
		getnameinfo(ai->ai_addr, ai->ai_addrlen, hostaddr, sizeof(hostaddr), NULL, 0, NI_NUMERICHOST);
		*fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

		if (*fd == SOCKET_NO)
		{
			continue;
		}

		/* Set non-blocking. */
		flags = fcntl(*fd, F_GETFL);

		if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) == -1)
		{
			LOG(llevError, "socket_create(): Error on switching to non-blocking. fcntl %x.\n", fcntl(*fd, F_GETFL));
			*fd = SOCKET_NO;
			return 0;
		}

		/* Try to connect. */
		start_timer = SDL_GetTicks();

		while (connect(*fd, ai->ai_addr, ai->ai_addrlen) != 0)
		{
			SDL_Delay(3);

			if (start_timer + SOCKET_TIMEOUT_MS < SDL_GetTicks())
			{
				close(*fd);
				*fd = SOCKET_NO;
				break;
			}
		}

		/* Set back to blocking. */
		if (*fd != SOCKET_NO && fcntl(*fd, F_SETFL, flags) == -1)
		{
			LOG(llevError, "socket_create(): Error on switching to blocking. fcntl %x.\n", fcntl(*fd, F_GETFL));
			*fd = SOCKET_NO;
			return 0;
		}

		if (*fd != SOCKET_NO)
		{
			break;
		}
	}

	freeaddrinfo(res);

	if (*fd == SOCKET_NO)
	{
		perror("Can't connect to server.");
		return 0;
	}
#endif

	linger_opt.l_onoff = 1;
	linger_opt.l_linger = 5;

	if (setsockopt(*fd, SOL_SOCKET, SO_LINGER, (char *) &linger_opt, sizeof(struct linger)))
	{
		LOG(llevError, "Error: Error on setsockopt LINGER\n");
	}

	if (getsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, &buflen) == -1)
	{
		oldbufsize = 0;
	}

	if (oldbufsize < newbufsize)
	{
		if (setsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &newbufsize, sizeof(&newbufsize)))
		{
			LOG(llevDebug, "socket_create(): setsockopt unable to set output buf size to %d\n", newbufsize);
			setsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, sizeof(&oldbufsize));
		}
	}

	return 1;
}
#else
int socket_create(SOCKET *fd, char *host, int port)
{
	int error, SocketStatusErrorNr;
	u_long temp;
	struct hostent *hostbn;
	int oldbufsize;
	int newbufsize = 65535, buflen = sizeof(int);
	uint32 start_timer;
	struct linger linger_opt;

	/* The way to make the sockets work on XP Home - The 'unix' style socket
	 * seems to fail under XP Home. */
	*fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	insock.sin_family = AF_INET;
	insock.sin_port = htons((unsigned short) port);

	if (isdigit(*host))
	{
		insock.sin_addr.s_addr = inet_addr(host);
	}
	else
	{
		hostbn = gethostbyname(host);

		if (!hostbn)
		{
			LOG(llevError, "Unknown host: %s\n", host);
			*fd = SOCKET_NO;
			return 0;
		}

		memcpy(&insock.sin_addr, hostbn->h_addr, hostbn->h_length);
	}

	temp = 1;

	/* Set non-blocking */
	if (ioctlsocket(*fd, FIONBIO, &temp) == -1)
	{
		LOG(llevError, "ERROR: ioctlsocket(*fd, FIONBIO, &temp)\n");
		*fd = SOCKET_NO;
		return 0;
	}

	linger_opt.l_onoff = 1;
	linger_opt.l_linger = 5;

	if (setsockopt(*fd, SOL_SOCKET, SO_LINGER, (char *) &linger_opt, sizeof(struct linger)))
	{
		LOG(llevError, "ERROR: Error on setsockopt LINGER\n");
	}

	error = 0;
	start_timer = SDL_GetTicks();

	while (connect(*fd, (struct sockaddr *) &insock, sizeof(insock)) == SOCKET_ERROR)
	{
		SDL_Delay(3);

		if (start_timer + SOCKET_TIMEOUT_MS < SDL_GetTicks())
		{
			*fd = SOCKET_NO;
			return 0;
		}

		SocketStatusErrorNr = WSAGetLastError();

		/* Connected. */
		if (SocketStatusErrorNr == WSAEISCONN)
		{
			break;
		}

		if (SocketStatusErrorNr == WSAEWOULDBLOCK || SocketStatusErrorNr == WSAEALREADY || (SocketStatusErrorNr == WSAEINVAL && error))
		{
			error = 1;
			continue;
		}

		LOG(llevMsg, "Connect error: %d\n", SocketStatusErrorNr);
		*fd = SOCKET_NO;
		return 0;
	}

	temp = 0;

	/* Set back to blocking. */
	if (ioctlsocket(*fd, FIONBIO, &temp) == -1)
	{
		LOG(llevError, "ERROR: ioctlsocket(*fd, FIONBIO, &temp == 0)\n");
		*fd = SOCKET_NO;
		return 0;
	}

	if (getsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, &buflen) == -1)
	{
		oldbufsize = 0;
	}

	if (oldbufsize < newbufsize)
	{
		if (setsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &newbufsize, sizeof(&newbufsize)))
		{
			setsockopt(*fd, SOL_SOCKET, SO_RCVBUF, (char *) &oldbufsize, sizeof(&oldbufsize));
		}
	}

	return 1;
}
#endif

/**
 * Open a new socket.
 * @param csock Socket to open.
 * @param host Host to connect to.
 * @param port Port to connect to.
 * @return 1 on success, 0 on failure. */
int socket_open(struct ClientSocket *csock, char *host, int port)
{
	int tmp = 1;

	printf("Connecting to %s:%d...\n", host, port);

	if (!socket_create(&csock->fd, host, port))
	{
		return 0;
	}

	LOG(llevMsg, "Connected to %s:%d\n", host, port);

	if (setsockopt(csock->fd, IPPROTO_TCP, TCP_NODELAY, (char *) &tmp, sizeof(tmp)))
	{
		LOG(llevError, "ERROR: setsockopt(TCP_NODELAY) failed\n");
	}

	return 1;
}

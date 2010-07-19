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
 * Low level socket related functions. */

#include <global.h>
#include <newclient.h>

#ifdef NO_ERRNO_H
extern int errno;
#else
#   include <errno.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <zlib.h>

/**
 * Add a NULL terminated string.
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

	sl->buf[sl->len] = c;
	sl->len++;
}

/**
 * Reads from socket.
 * @param ns Socket to read from.
 * @param len Max length of data to read.
 * @return 1 on success, -1 on failure. */
int SockList_ReadPacket(socket_struct *ns, int len)
{
	SockList *sl = &ns->readbuf;
	int stat_ret;

#ifdef WIN32
	stat_ret = recv(ns->fd, sl->buf + sl->len, len - sl->len, 0);
#else
	do
	{
		stat_ret = read(ns->fd, sl->buf + sl->len, len - sl->len);
	}
	while (stat_ret == -1 && errno == EINTR);
#endif

	if (stat_ret == 0)
	{
		return -1;
	}

	if (stat_ret > 0)
	{
		sl->len += stat_ret;
#if CS_LOGSTATS
		cst_tot.ibytes += stat_ret;
		cst_lst.ibytes += stat_ret;
#endif
	}
	else if (stat_ret < 0)
	{
#ifdef WIN32
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			if (WSAGetLastError() == WSAECONNRESET)
			{
				LOG(llevDebug, "Connection closed by client.\n");
			}
			else
			{
				LOG(llevDebug, "SockList_ReadPacket() got error %d, returning %d.\n", WSAGetLastError(), stat_ret);
			}

			return stat_ret;
		}
#else
		if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(llevDebug, "SockList_ReadPacket() got error %d: %s, returning %d.\n", errno, strerror_local(errno), stat_ret);
			return stat_ret;
		}
#endif
	}

	return 1;
}

/**
 * Read command from 'sl' and copy it to 'sl'.
 * @param sl Where to read command from.
 * @param sl2 Where to copy the command.
 * @return Length of the read command. */
int SockList_ReadCommand(SockList *sl, SockList *sl2)
{
	int toread, ret = 0;

	sl2->buf[0] = '\0';
	sl2->len = 0;

	/* Is there anything in our buffer that was read
	 * before? */
	if (sl->len >= 2)
	{
		/* Length of the command. */
		toread = 2 + (sl->buf[0] << 8) + sl->buf[1];

		/* If we have a command, copy it over. */
		if (toread <= sl->len)
		{
			memcpy(sl2->buf, sl->buf, toread);
			sl2->len = toread;

			if (sl->len - toread)
			{
				memmove(sl->buf, sl->buf + toread, sl->len - toread);
			}

			sl->len -= toread;
			ret = toread;
		}
	}

	return ret;
}

/**
 * Enqueue data to the socket buffer queue.
 * @param ns The socket we are adding the data to.
 * @param buf The data.
 * @param len Number of bytes to add. */
static void socket_buffer_enqueue(socket_struct *ns, unsigned char *buf, size_t len)
{
	socket_buffer *buffer;

	if (!len)
	{
		LOG(llevDebug, "DEBUG: socket_buffer_enqueue(): Tried to add buffer with 0 length.\n");
		return;
	}

	buffer = (socket_buffer *) malloc(sizeof(socket_buffer));

	buffer->buf = (char *) malloc(len + 1);
	memcpy(buffer->buf, buf, len);
	buffer->len = len;
	buffer->next = NULL;
	buffer->pos = 0;

	if (ns->buffer_front)
	{
		ns->buffer_front->last = buffer;
		buffer->next = ns->buffer_front;
		buffer->last = NULL;
		ns->buffer_front = buffer;
	}
	else
	{
		ns->buffer_back = ns->buffer_front = buffer;
		buffer->next = buffer->last = NULL;
	}
}

/**
 * Dequeue data from the socket buffer queue.
 * @param ns Socket we're going to dequeue the socket buffer from. */
static void socket_buffer_dequeue(socket_struct *ns)
{
	socket_buffer *tmp = ns->buffer_back;

	ns->buffer_back = ns->buffer_back->last;

	if (ns->buffer_back)
	{
		ns->buffer_back->next = NULL;
	}
	else
	{
		ns->buffer_front = NULL;
	}

	free(tmp->buf);
	free(tmp);
}

/**
 * Dequeue all socket buffers in the queue.
 * @param ns Socket to clear the socket buffers for. */
void socket_buffer_clear(socket_struct *ns)
{
	while (ns->buffer_back)
	{
		socket_buffer_dequeue(ns);
	}
}

/**
 * Write data to socket.
 * @param ns The socket we are writing to. */
void socket_buffer_write(socket_struct *ns)
{
	int amt, max;

	/* Nothing to send? */
	if (!ns->buffer_back)
	{
		return;
	}

	while (ns->buffer_back)
	{
		max = ns->buffer_back->len - ns->buffer_back->pos;
		amt = send(ns->fd, ns->buffer_back->buf + ns->buffer_back->pos, max, MSG_DONTWAIT);

#ifndef WIN32
		if (!amt)
		{
			amt = max;
		}
		else
#endif
		if (amt < 0)
		{
#ifdef WIN32
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LOG(llevDebug, "DEBUG: socket_buffer_write(): New socket write failed (%d).\n", WSAGetLastError());
#else
			if (errno != EWOULDBLOCK)
			{
				LOG(llevDebug, "DEBUG: socket_buffer_write(): New socket write failed (%d: %s).\n", errno, strerror_local(errno));
#endif
				ns->status = Ns_Dead;
				return;
			}
			/* EWOULDBLOCK: We can't write because socket is busy. */
			else
			{
				return;
			}
		}

		ns->buffer_back->pos += amt;
#if CS_LOGSTATS
		cst_tot.obytes += amt;
		cst_lst.obytes += amt;
#endif

		if (ns->buffer_back->len - ns->buffer_back->pos == 0)
		{
			socket_buffer_dequeue(ns);
		}
	}
}

/**
 * Calls Write_To_Socket to send data to the client.
 *
 * The only difference in this function is that we take a SockList, and
 * we prepend the length information.
 * @param ns Socket to send the data to
 * @param msg The SockList instance */
void Send_With_Handling(socket_struct *ns, SockList *msg)
{
	unsigned char sbuf[4];
	uint8 *buf;
	size_t len;
#if COMPRESS_DATA_PACKETS
	uint8 *dest = NULL;
#endif

	if (ns->status == Ns_Dead || !msg)
	{
		return;
	}

	buf = msg->buf;
	len = msg->len;

#if COMPRESS_DATA_PACKETS
	if (len > COMPRESS_DATA_PACKETS_SIZE && buf[0] != BINARY_CMD_DATA && ns->socket_version >= 1039)
	{
		size_t new_size = compressBound(len);

		dest = malloc(new_size + 5);
		/* Marker for the reserved #0 binary command. */
		dest[0] = 0;
		/* Add original length of the packet. */
		dest[1] = (len >> 24) & 0xff;
		dest[2] = (len >> 16) & 0xff;
		dest[3] = (len >> 8) & 0xff;
		dest[4] = (len) & 0xff;
		/* Compress it. */
		compress2((Bytef *) dest + 5, (uLong *) &new_size, (const unsigned char FAR *) buf, len, Z_BEST_COMPRESSION);
		buf = dest;
		len = new_size + 5;
	}
#endif

	/* If more than 32kb use 3 bytes header and set the high bit to show
	 * it to the client. */
	if (len > 32 * 1024 - 1)
	{
		sbuf[0] = ((uint32) (len) >> 16) & 0xFF;
		/* High bit marker for the client */
		sbuf[0] |= 0x80;
		sbuf[1] = ((uint32) (len) >> 8) & 0xFF;
		sbuf[2] = ((uint32) (len)) & 0xFF;

		socket_buffer_enqueue(ns, sbuf, 3);
	}
	else
	{
		sbuf[0] = ((uint32) (len) >> 8) & 0xFF;
		sbuf[1] = ((uint32) (len)) & 0xFF;

		socket_buffer_enqueue(ns, sbuf, 2);
	}

	socket_buffer_enqueue(ns, buf, len);

#if COMPRESS_DATA_PACKETS
	/* Free the buffer that was used for compression, if it was allocated. */
	if (dest)
	{
		free(dest);
	}
#endif
}

/**
 * Takes a string of data, and writes it out to the socket. A very handy
 * shortcut function. */
void Write_String_To_Socket(socket_struct *ns, char cmd, char *buf, int len)
{
	SockList sl;

	sl.len = len;
	sl.buf = (uint8 *) buf;
	*((char *) buf) = cmd;

	Send_With_Handling(ns, &sl);
}

#if CS_LOGSTATS

/** Life of the server. */
CS_Stats cst_tot;

/** Last series of stats. */
CS_Stats cst_lst;

/**
 * Writes out the gathered stats.
 *
 * We clear ::cst_lst. */
void write_cs_stats()
{
	time_t now = time(NULL);

	/* If no connections recently, don't bother to log anything */
	if (cst_lst.ibytes == 0 && cst_lst.obytes == 0)
	{
		return;
	}

	/* CSSTAT is put in so scripts can easily find the line */
	LOG(llevInfo, "CSSTAT: %.16s tot in:%d out:%d maxc:%d time:%lu last block-> in:%d out:%d maxc:%d time:%lu\n", ctime(&now), cst_tot.ibytes, cst_tot.obytes, cst_tot.max_conn, now - cst_tot.time_start, cst_lst.ibytes, cst_lst.obytes, cst_lst.max_conn, now - cst_lst.time_start);

	cst_lst.ibytes = 0;
	cst_lst.obytes = 0;
	cst_lst.max_conn = socket_info.nconns;
	cst_lst.time_start = now;
}
#endif
